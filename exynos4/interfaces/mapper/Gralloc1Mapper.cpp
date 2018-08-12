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

#define LOG_TAG "Gralloc1Mapper"

#include "Gralloc1Mapper.h"

#include <vector>

#include <log/log.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::common::V1_0::BufferUsage;

Gralloc1Mapper::Gralloc1Mapper(const hw_module_t* module)
    : mDevice(nullptr), mDispatch() {
    int result = gralloc1_open(module, &mDevice);
    if (result) {
        LOG_ALWAYS_FATAL("failed to open gralloc1 device: %s",
                         strerror(-result));
    }

    initCapabilities();
    initDispatch();
}

Gralloc1Mapper::~Gralloc1Mapper() {
    gralloc1_close(mDevice);
}

void Gralloc1Mapper::initCapabilities() {
    mCapabilities.highUsageBits = true;
    mCapabilities.layeredBuffers = false;
    mCapabilities.unregisterImplyDelete = false;

    uint32_t count = 0;
    mDevice->getCapabilities(mDevice, &count, nullptr);

    std::vector<int32_t> capabilities(count);
    mDevice->getCapabilities(mDevice, &count, capabilities.data());
    capabilities.resize(count);

    for (auto capability : capabilities) {
        switch (capability) {
            case GRALLOC1_CAPABILITY_LAYERED_BUFFERS:
                mCapabilities.layeredBuffers = true;
                break;
            case GRALLOC1_CAPABILITY_RELEASE_IMPLY_DELETE:
                mCapabilities.unregisterImplyDelete = true;
                break;
        }
    }
}

template <typename T>
void Gralloc1Mapper::initDispatch(gralloc1_function_descriptor_t desc,
                                  T* outPfn) {
    auto pfn = mDevice->getFunction(mDevice, desc);
    if (!pfn) {
        LOG_ALWAYS_FATAL("failed to get gralloc1 function %d", desc);
    }

    *outPfn = reinterpret_cast<T>(pfn);
}

void Gralloc1Mapper::initDispatch() {
    initDispatch(GRALLOC1_FUNCTION_RETAIN, &mDispatch.retain);
    initDispatch(GRALLOC1_FUNCTION_RELEASE, &mDispatch.release);
    initDispatch(GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES,
                 &mDispatch.getNumFlexPlanes);
    initDispatch(GRALLOC1_FUNCTION_LOCK, &mDispatch.lock);
    initDispatch(GRALLOC1_FUNCTION_LOCK_FLEX, &mDispatch.lockFlex);
    initDispatch(GRALLOC1_FUNCTION_UNLOCK, &mDispatch.unlock);
}

Error Gralloc1Mapper::toError(int32_t error) {
    switch (error) {
        case GRALLOC1_ERROR_NONE:
            return Error::NONE;
        case GRALLOC1_ERROR_BAD_DESCRIPTOR:
            return Error::BAD_DESCRIPTOR;
        case GRALLOC1_ERROR_BAD_HANDLE:
            return Error::BAD_BUFFER;
        case GRALLOC1_ERROR_BAD_VALUE:
            return Error::BAD_VALUE;
        case GRALLOC1_ERROR_NOT_SHARED:
            return Error::NONE;  // this is fine
        case GRALLOC1_ERROR_NO_RESOURCES:
            return Error::NO_RESOURCES;
        case GRALLOC1_ERROR_UNDEFINED:
        case GRALLOC1_ERROR_UNSUPPORTED:
        default:
            return Error::UNSUPPORTED;
    }
}

bool Gralloc1Mapper::toYCbCrLayout(const android_flex_layout& flex,
                                   YCbCrLayout* outLayout) {
    // must be YCbCr
    if (flex.format != FLEX_FORMAT_YCbCr || flex.num_planes < 3) {
        return false;
    }

    for (int i = 0; i < 3; i++) {
        const auto& plane = flex.planes[i];
        // must have 8-bit depth
        if (plane.bits_per_component != 8 || plane.bits_used != 8) {
            return false;
        }

        if (plane.component == FLEX_COMPONENT_Y) {
            // Y must not be interleaved
            if (plane.h_increment != 1) {
                return false;
            }
        } else {
            // Cb and Cr can be interleaved
            if (plane.h_increment != 1 && plane.h_increment != 2) {
                return false;
            }
        }

        if (!plane.v_increment) {
            return false;
        }
    }

    if (flex.planes[0].component != FLEX_COMPONENT_Y ||
        flex.planes[1].component != FLEX_COMPONENT_Cb ||
        flex.planes[2].component != FLEX_COMPONENT_Cr) {
        return false;
    }

    const auto& y = flex.planes[0];
    const auto& cb = flex.planes[1];
    const auto& cr = flex.planes[2];

    if (cb.h_increment != cr.h_increment || cb.v_increment != cr.v_increment) {
        return false;
    }

    outLayout->y = y.top_left;
    outLayout->cb = cb.top_left;
    outLayout->cr = cr.top_left;
    outLayout->yStride = y.v_increment;
    outLayout->cStride = cb.v_increment;
    outLayout->chromaStep = cb.h_increment;

    return true;
}

gralloc1_rect_t Gralloc1Mapper::asGralloc1Rect(const IMapper::Rect& rect) {
    return gralloc1_rect_t{rect.left, rect.top, rect.width, rect.height};
}

Error Gralloc1Mapper::registerBuffer(buffer_handle_t bufferHandle) {
    return toError(mDispatch.retain(mDevice, bufferHandle));
}

void Gralloc1Mapper::unregisterBuffer(buffer_handle_t bufferHandle) {
    mDispatch.release(mDevice, bufferHandle);
}

Error Gralloc1Mapper::lockBuffer(buffer_handle_t bufferHandle,
                                 uint64_t cpuUsage,
                                 const IMapper::Rect& accessRegion, int fenceFd,
                                 void** outData) {
    // Dup fenceFd as it is going to be owned by gralloc.  Note that it is
    // gralloc's responsibility to close it, even on locking errors.
    if (fenceFd >= 0) {
        fenceFd = dup(fenceFd);
        if (fenceFd < 0) {
            return Error::NO_RESOURCES;
        }
    }

    const uint64_t consumerUsage =
        cpuUsage & ~static_cast<uint64_t>(BufferUsage::CPU_WRITE_MASK);
    const auto accessRect = asGralloc1Rect(accessRegion);
    void* data = nullptr;
    int32_t error = mDispatch.lock(mDevice, bufferHandle, cpuUsage,
                                   consumerUsage, &accessRect, &data, fenceFd);

    if (error == GRALLOC1_ERROR_NONE) {
        *outData = data;
    }

    return toError(error);
}

Error Gralloc1Mapper::lockBuffer(buffer_handle_t bufferHandle,
                                 uint64_t cpuUsage,
                                 const IMapper::Rect& accessRegion, int fenceFd,
                                 YCbCrLayout* outLayout) {
    // prepare flex layout
    android_flex_layout flex = {};
    int32_t error =
        mDispatch.getNumFlexPlanes(mDevice, bufferHandle, &flex.num_planes);
    if (error != GRALLOC1_ERROR_NONE) {
        return toError(error);
    }
    std::vector<android_flex_plane_t> flexPlanes(flex.num_planes);
    flex.planes = flexPlanes.data();

    // Dup fenceFd as it is going to be owned by gralloc.  Note that it is
    // gralloc's responsibility to close it, even on locking errors.
    if (fenceFd >= 0) {
        fenceFd = dup(fenceFd);
        if (fenceFd < 0) {
            return Error::NO_RESOURCES;
        }
    }

    const uint64_t consumerUsage =
        cpuUsage & ~static_cast<uint64_t>(BufferUsage::CPU_WRITE_MASK);
    const auto accessRect = asGralloc1Rect(accessRegion);
    error = mDispatch.lockFlex(mDevice, bufferHandle, cpuUsage, consumerUsage,
                               &accessRect, &flex, fenceFd);
    if (error == GRALLOC1_ERROR_NONE && !toYCbCrLayout(flex, outLayout)) {
        ALOGD("unable to convert android_flex_layout to YCbCrLayout");

        // undo the lock
        fenceFd = -1;
        mDispatch.unlock(mDevice, bufferHandle, &fenceFd);
        if (fenceFd >= 0) {
            close(fenceFd);
        }

        error = GRALLOC1_ERROR_BAD_HANDLE;
    }

    return toError(error);
}

Error Gralloc1Mapper::unlockBuffer(buffer_handle_t bufferHandle,
                                   int* outFenceFd) {
    int fenceFd = -1;
    int32_t error = mDispatch.unlock(mDevice, bufferHandle, &fenceFd);

    if (error == GRALLOC1_ERROR_NONE) {
        *outFenceFd = fenceFd;
    } else if (fenceFd >= 0) {
        // we always own the fenceFd even when unlock failed
        close(fenceFd);
    }

    return toError(error);
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android
