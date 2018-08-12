/*
 * Copyright 2016 The Android Open Source Project
 * * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "GrallocMapperPassthrough"

#include "GrallocMapper.h"

#include "Gralloc0Mapper.h"
#include "Gralloc1Mapper.h"
#include "GrallocBufferDescriptor.h"

#include <inttypes.h>

#include <log/log.h>
#include <sync/sync.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::common::V1_0::BufferUsage;
using android::hardware::graphics::common::V1_0::PixelFormat;

namespace {

class RegisteredHandlePool {
   public:
    bool add(buffer_handle_t bufferHandle) {
        std::lock_guard<std::mutex> lock(mMutex);
        return mHandles.insert(bufferHandle).second;
    }

    native_handle_t* pop(void* buffer) {
        auto bufferHandle = static_cast<native_handle_t*>(buffer);

        std::lock_guard<std::mutex> lock(mMutex);
        return mHandles.erase(bufferHandle) == 1 ? bufferHandle : nullptr;
    }

    buffer_handle_t get(const void* buffer) {
        auto bufferHandle = static_cast<buffer_handle_t>(buffer);

        std::lock_guard<std::mutex> lock(mMutex);
        return mHandles.count(bufferHandle) == 1 ? bufferHandle : nullptr;
    }

   private:
    std::mutex mMutex;
    std::unordered_set<buffer_handle_t> mHandles;
};

// GraphicBufferMapper is expected to be valid (and leaked) during process
// termination.  We need to make sure IMapper, and in turn, gRegisteredHandles
// are valid as well.  Create the registered handle pool on the heap, and let
// it leak for simplicity.
//
// However, there is no way to make sure gralloc0/gralloc1 are valid.  Any use
// of static/global object in gralloc0/gralloc1 that may have been destructed
// is potentially broken.
RegisteredHandlePool* gRegisteredHandles = new RegisteredHandlePool;

}  // anonymous namespace

bool GrallocMapper::validateDescriptorInfo(
    const BufferDescriptorInfo& descriptorInfo) const {
    const uint64_t validUsageBits =
        BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK |
        BufferUsage::GPU_TEXTURE | BufferUsage::GPU_RENDER_TARGET |
        BufferUsage::COMPOSER_OVERLAY | BufferUsage::COMPOSER_CLIENT_TARGET |
        BufferUsage::PROTECTED | BufferUsage::COMPOSER_CURSOR |
        BufferUsage::VIDEO_ENCODER | BufferUsage::CAMERA_OUTPUT |
        BufferUsage::CAMERA_INPUT | BufferUsage::RENDERSCRIPT |
        BufferUsage::VIDEO_DECODER | BufferUsage::SENSOR_DIRECT_DATA |
        BufferUsage::GPU_DATA_BUFFER | BufferUsage::VENDOR_MASK |
        (mCapabilities.highUsageBits ? BufferUsage::VENDOR_MASK_HI
                                     : static_cast<BufferUsage>(0));

    if (!descriptorInfo.width || !descriptorInfo.height ||
        !descriptorInfo.layerCount) {
        return false;
    }

    if (!mCapabilities.layeredBuffers && descriptorInfo.layerCount > 1) {
        return false;
    }

    if (descriptorInfo.format == static_cast<PixelFormat>(0)) {
        return false;
    }

    if (descriptorInfo.usage & ~validUsageBits) {
        // could not fail as gralloc may use the reserved bits...
        ALOGW("buffer descriptor with invalid usage bits 0x%" PRIx64,
              descriptorInfo.usage & ~validUsageBits);
    }

    return true;
}

Return<void> GrallocMapper::createDescriptor(
    const BufferDescriptorInfo& descriptorInfo, createDescriptor_cb hidl_cb) {
    if (validateDescriptorInfo(descriptorInfo)) {
        hidl_cb(Error::NONE, grallocEncodeBufferDescriptor(descriptorInfo));
    } else {
        hidl_cb(Error::BAD_VALUE, BufferDescriptor());
    }

    return Void();
}

Return<void> GrallocMapper::importBuffer(const hidl_handle& rawHandle,
                                         importBuffer_cb hidl_cb) {
    // because of passthrough HALs, we must not generate an error when
    // rawHandle has been imported

    if (!rawHandle.getNativeHandle()) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    native_handle_t* bufferHandle =
        native_handle_clone(rawHandle.getNativeHandle());
    if (!bufferHandle) {
        hidl_cb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    Error error = registerBuffer(bufferHandle);
    if (error != Error::NONE) {
        native_handle_close(bufferHandle);
        native_handle_delete(bufferHandle);

        hidl_cb(error, nullptr);
        return Void();
    }

    // The newly cloned handle is already registered?  This can only happen
    // when a handle previously registered was native_handle_delete'd instead
    // of freeBuffer'd.
    if (!gRegisteredHandles->add(bufferHandle)) {
        ALOGE("handle %p has already been imported; potential fd leaking",
              bufferHandle);
        unregisterBuffer(bufferHandle);
        if (!mCapabilities.unregisterImplyDelete) {
            native_handle_close(bufferHandle);
            native_handle_delete(bufferHandle);
        }

        hidl_cb(Error::NO_RESOURCES, nullptr);
        return Void();
    }

    hidl_cb(Error::NONE, bufferHandle);
    return Void();
}

Return<Error> GrallocMapper::freeBuffer(void* buffer) {
    native_handle_t* bufferHandle = gRegisteredHandles->pop(buffer);
    if (!bufferHandle) {
        return Error::BAD_BUFFER;
    }

    unregisterBuffer(bufferHandle);
    if (!mCapabilities.unregisterImplyDelete) {
        native_handle_close(bufferHandle);
        native_handle_delete(bufferHandle);
    }

    return Error::NONE;
}

void GrallocMapper::waitFenceFd(int fenceFd, const char* logname) {
    if (fenceFd < 0) {
        return;
    }

    const int warningTimeout = 3500;
    const int error = sync_wait(fenceFd, warningTimeout);
    if (error < 0 && errno == ETIME) {
        ALOGE("%s: fence %d didn't signal in %u ms", logname, fenceFd,
              warningTimeout);
        sync_wait(fenceFd, -1);
    }
}

bool GrallocMapper::getFenceFd(const hidl_handle& fenceHandle,
                               int* outFenceFd) {
    auto handle = fenceHandle.getNativeHandle();
    if (handle && handle->numFds > 1) {
        ALOGE("invalid fence handle with %d fds", handle->numFds);
        return false;
    }

    *outFenceFd = (handle && handle->numFds == 1) ? handle->data[0] : -1;
    return true;
}

hidl_handle GrallocMapper::getFenceHandle(int fenceFd, char* handleStorage) {
    native_handle_t* handle = nullptr;
    if (fenceFd >= 0) {
        handle = native_handle_init(handleStorage, 1, 0);
        handle->data[0] = fenceFd;
    }

    return hidl_handle(handle);
}

Return<void> GrallocMapper::lock(void* buffer, uint64_t cpuUsage,
                                 const IMapper::Rect& accessRegion,
                                 const hidl_handle& acquireFence,
                                 lock_cb hidl_cb) {
    buffer_handle_t bufferHandle = gRegisteredHandles->get(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    int fenceFd;
    if (!getFenceFd(acquireFence, &fenceFd)) {
        hidl_cb(Error::BAD_VALUE, nullptr);
        return Void();
    }

    void* data = nullptr;
    Error error =
        lockBuffer(bufferHandle, cpuUsage, accessRegion, fenceFd, &data);

    hidl_cb(error, data);
    return Void();
}

Return<void> GrallocMapper::lockYCbCr(void* buffer, uint64_t cpuUsage,
                                      const IMapper::Rect& accessRegion,
                                      const hidl_handle& acquireFence,
                                      lockYCbCr_cb hidl_cb) {
    YCbCrLayout layout = {};

    buffer_handle_t bufferHandle = gRegisteredHandles->get(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, layout);
        return Void();
    }

    int fenceFd;
    if (!getFenceFd(acquireFence, &fenceFd)) {
        hidl_cb(Error::BAD_VALUE, layout);
        return Void();
    }

    Error error =
        lockBuffer(bufferHandle, cpuUsage, accessRegion, fenceFd, &layout);

    hidl_cb(error, layout);
    return Void();
}

Return<void> GrallocMapper::unlock(void* buffer, unlock_cb hidl_cb) {
    buffer_handle_t bufferHandle = gRegisteredHandles->get(buffer);
    if (!bufferHandle) {
        hidl_cb(Error::BAD_BUFFER, nullptr);
        return Void();
    }

    int fenceFd;
    Error error = unlockBuffer(bufferHandle, &fenceFd);
    if (error == Error::NONE) {
        NATIVE_HANDLE_DECLARE_STORAGE(fenceStorage, 1, 0);

        hidl_cb(error, getFenceHandle(fenceFd, fenceStorage));

        if (fenceFd >= 0) {
            close(fenceFd);
        }
    } else {
        hidl_cb(error, nullptr);
    }

    return Void();
}

IMapper* HIDL_FETCH_IMapper(const char* /* name */) {
    const hw_module_t* module = nullptr;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        ALOGE("failed to get gralloc module");
        return nullptr;
    }

    uint8_t major = (module->module_api_version >> 8) & 0xff;
    switch (major) {
        case 1:
            return new Gralloc1Mapper(module);
        case 0:
            return new Gralloc0Mapper(module);
        default:
            ALOGE("unknown gralloc module major version %d", major);
            return nullptr;
    }
}

} // namespace implementation
} // namespace V2_0
} // namespace mapper
} // namespace graphics
} // namespace hardware
} // namespace android
