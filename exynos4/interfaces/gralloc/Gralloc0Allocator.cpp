/*
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#define LOG_TAG "Gralloc0Allocator"

#include "Gralloc0Allocator.h"
#include "GrallocBufferDescriptor.h"

#include <vector>

#include <string.h>

#include <log/log.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::mapper::V2_0::implementation::
    grallocDecodeBufferDescriptor;

Gralloc0Allocator::Gralloc0Allocator(const hw_module_t* module) {
    int result = gralloc_open(module, &mDevice);
    if (result) {
        LOG_ALWAYS_FATAL("failed to open gralloc0 device: %s",
                         strerror(-result));
    }
}

Gralloc0Allocator::~Gralloc0Allocator() {
    gralloc_close(mDevice);
}

Return<void> Gralloc0Allocator::dumpDebugInfo(dumpDebugInfo_cb hidl_cb) {
    char buf[4096] = {};
    if (mDevice->dump) {
        mDevice->dump(mDevice, buf, sizeof(buf));
        buf[sizeof(buf) - 1] = '\0';
    }

    hidl_cb(hidl_string(buf));

    return Void();
}

Return<void> Gralloc0Allocator::allocate(const BufferDescriptor& descriptor,
                                         uint32_t count, allocate_cb hidl_cb) {
    IMapper::BufferDescriptorInfo descriptorInfo;
    if (!grallocDecodeBufferDescriptor(descriptor, &descriptorInfo)) {
        hidl_cb(Error::BAD_DESCRIPTOR, 0, hidl_vec<hidl_handle>());
        return Void();
    }

    Error error = Error::NONE;
    uint32_t stride = 0;
    std::vector<hidl_handle> buffers;
    buffers.reserve(count);

    // allocate the buffers
    for (uint32_t i = 0; i < count; i++) {
        buffer_handle_t tmpBuffer;
        uint32_t tmpStride;
        error = allocateOne(descriptorInfo, &tmpBuffer, &tmpStride);
        if (error != Error::NONE) {
            break;
        }

        if (stride == 0) {
            stride = tmpStride;
        } else if (stride != tmpStride) {
            // non-uniform strides
            mDevice->free(mDevice, tmpBuffer);
            stride = 0;
            error = Error::UNSUPPORTED;
            break;
        }

        buffers.emplace_back(hidl_handle(tmpBuffer));
    }

    // return the buffers
    hidl_vec<hidl_handle> hidl_buffers;
    if (error == Error::NONE) {
        hidl_buffers.setToExternal(buffers.data(), buffers.size());
    }
    hidl_cb(error, stride, hidl_buffers);

    // free the buffers
    for (const auto& buffer : buffers) {
        mDevice->free(mDevice, buffer.getNativeHandle());
    }

    return Void();
}

Error Gralloc0Allocator::allocateOne(const IMapper::BufferDescriptorInfo& info,
                                     buffer_handle_t* outBuffer,
                                     uint32_t* outStride) {
    if (info.layerCount > 1 || (info.usage >> 32) != 0) {
        return Error::BAD_VALUE;
    }

    buffer_handle_t buffer = nullptr;
    int stride = 0;
    int result = mDevice->alloc(mDevice, info.width, info.height,
                                static_cast<int>(info.format), info.usage,
                                &buffer, &stride);
    if (result) {
        switch (result) {
            case -EINVAL:
                return Error::BAD_VALUE;
            default:
                return Error::NO_RESOURCES;
        }
    }

    *outBuffer = buffer;
    *outStride = stride;

    return Error::NONE;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android
