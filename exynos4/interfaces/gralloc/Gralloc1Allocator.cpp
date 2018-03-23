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

#define LOG_TAG "Gralloc1Allocator"

#include "Gralloc1Allocator.h"
#include "GrallocBufferDescriptor.h"

#include <vector>

#include <string.h>

#include <log/log.h>

#include <grallocusage/GrallocUsageConversion.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::common::V1_0::BufferUsage;
using android::hardware::graphics::mapper::V2_0::implementation::
    grallocDecodeBufferDescriptor;

Gralloc1Allocator::Gralloc1Allocator(const hw_module_t* module)
    : mDevice(nullptr), mCapabilities(), mDispatch() {
    int result = gralloc1_open(module, &mDevice);
    if (result) {
        LOG_ALWAYS_FATAL("failed to open gralloc1 device: %s",
                         strerror(-result));
    }

    initCapabilities();
    initDispatch();
}

Gralloc1Allocator::~Gralloc1Allocator() {
    gralloc1_close(mDevice);
}

void Gralloc1Allocator::initCapabilities() {
    uint32_t count = 0;
    mDevice->getCapabilities(mDevice, &count, nullptr);

    std::vector<int32_t> capabilities(count);
    mDevice->getCapabilities(mDevice, &count, capabilities.data());
    capabilities.resize(count);

    for (auto capability : capabilities) {
        if (capability == GRALLOC1_CAPABILITY_LAYERED_BUFFERS) {
            mCapabilities.layeredBuffers = true;
            break;
        }
    }
}

template <typename T>
void Gralloc1Allocator::initDispatch(gralloc1_function_descriptor_t desc,
                                     T* outPfn) {
    auto pfn = mDevice->getFunction(mDevice, desc);
    if (!pfn) {
        LOG_ALWAYS_FATAL("failed to get gralloc1 function %d", desc);
    }

    *outPfn = reinterpret_cast<T>(pfn);
}

void Gralloc1Allocator::initDispatch() {
    initDispatch(GRALLOC1_FUNCTION_DUMP, &mDispatch.dump);
    initDispatch(GRALLOC1_FUNCTION_CREATE_DESCRIPTOR,
                 &mDispatch.createDescriptor);
    initDispatch(GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR,
                 &mDispatch.destroyDescriptor);
    initDispatch(GRALLOC1_FUNCTION_SET_DIMENSIONS, &mDispatch.setDimensions);
    initDispatch(GRALLOC1_FUNCTION_SET_FORMAT, &mDispatch.setFormat);
    if (mCapabilities.layeredBuffers) {
        initDispatch(GRALLOC1_FUNCTION_SET_LAYER_COUNT,
                     &mDispatch.setLayerCount);
    }
    initDispatch(GRALLOC1_FUNCTION_SET_CONSUMER_USAGE,
                 &mDispatch.setConsumerUsage);
    initDispatch(GRALLOC1_FUNCTION_SET_PRODUCER_USAGE,
                 &mDispatch.setProducerUsage);
    initDispatch(GRALLOC1_FUNCTION_GET_STRIDE, &mDispatch.getStride);
    initDispatch(GRALLOC1_FUNCTION_ALLOCATE, &mDispatch.allocate);
    initDispatch(GRALLOC1_FUNCTION_RELEASE, &mDispatch.release);
}

Return<void> Gralloc1Allocator::dumpDebugInfo(dumpDebugInfo_cb hidl_cb) {
    uint32_t len = 0;
    mDispatch.dump(mDevice, &len, nullptr);

    std::vector<char> buf(len + 1);
    mDispatch.dump(mDevice, &len, buf.data());
    buf.resize(len + 1);
    buf[len] = '\0';

    hidl_string reply;
    reply.setToExternal(buf.data(), len);
    hidl_cb(reply);

    return Void();
}

Return<void> Gralloc1Allocator::allocate(const BufferDescriptor& descriptor,
                                         uint32_t count, allocate_cb hidl_cb) {
    IMapper::BufferDescriptorInfo descriptorInfo;
    if (!grallocDecodeBufferDescriptor(descriptor, &descriptorInfo)) {
        hidl_cb(Error::BAD_DESCRIPTOR, 0, hidl_vec<hidl_handle>());
        return Void();
    }

    gralloc1_buffer_descriptor_t desc;
    Error error = createDescriptor(descriptorInfo, &desc);
    if (error != Error::NONE) {
        hidl_cb(error, 0, hidl_vec<hidl_handle>());
        return Void();
    }

    uint32_t stride = 0;
    std::vector<hidl_handle> buffers;
    buffers.reserve(count);

    // allocate the buffers
    for (uint32_t i = 0; i < count; i++) {
        buffer_handle_t tmpBuffer;
        uint32_t tmpStride;
        error = allocateOne(desc, &tmpBuffer, &tmpStride);
        if (error != Error::NONE) {
            break;
        }

        if (stride == 0) {
            stride = tmpStride;
        } else if (stride != tmpStride) {
            // non-uniform strides
            mDispatch.release(mDevice, tmpBuffer);
            stride = 0;
            error = Error::UNSUPPORTED;
            break;
        }

        buffers.emplace_back(hidl_handle(tmpBuffer));
    }

    mDispatch.destroyDescriptor(mDevice, desc);

    // return the buffers
    hidl_vec<hidl_handle> hidl_buffers;
    if (error == Error::NONE) {
        hidl_buffers.setToExternal(buffers.data(), buffers.size());
    }
    hidl_cb(error, stride, hidl_buffers);

    // free the buffers
    for (const auto& buffer : buffers) {
        mDispatch.release(mDevice, buffer.getNativeHandle());
    }

    return Void();
}

Error Gralloc1Allocator::toError(int32_t error) {
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

uint64_t Gralloc1Allocator::toProducerUsage(uint64_t usage) {
    // this is potentially broken as we have no idea which private flags
    // should be filtered out
    uint64_t producerUsage =
        usage &
        ~static_cast<uint64_t>(BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK |
                               BufferUsage::GPU_DATA_BUFFER);

    switch (usage & BufferUsage::CPU_WRITE_MASK) {
        case static_cast<uint64_t>(BufferUsage::CPU_WRITE_RARELY):
            producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
            break;
        case static_cast<uint64_t>(BufferUsage::CPU_WRITE_OFTEN):
            producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN;
            break;
        default:
            break;
    }

    switch (usage & BufferUsage::CPU_READ_MASK) {
        case static_cast<uint64_t>(BufferUsage::CPU_READ_RARELY):
            producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_READ;
            break;
        case static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN):
            producerUsage |= GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN;
            break;
        default:
            break;
    }

    // BufferUsage::GPU_DATA_BUFFER is always filtered out

    return producerUsage;
}

uint64_t Gralloc1Allocator::toConsumerUsage(uint64_t usage) {
    // this is potentially broken as we have no idea which private flags
    // should be filtered out
    uint64_t consumerUsage =
        usage &
        ~static_cast<uint64_t>(BufferUsage::CPU_READ_MASK | BufferUsage::CPU_WRITE_MASK |
                               BufferUsage::SENSOR_DIRECT_DATA | BufferUsage::GPU_DATA_BUFFER);

    switch (usage & BufferUsage::CPU_READ_MASK) {
        case static_cast<uint64_t>(BufferUsage::CPU_READ_RARELY):
            consumerUsage |= GRALLOC1_CONSUMER_USAGE_CPU_READ;
            break;
        case static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN):
            consumerUsage |= GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN;
            break;
        default:
            break;
    }

    // BufferUsage::SENSOR_DIRECT_DATA is always filtered out

    if (usage & BufferUsage::GPU_DATA_BUFFER) {
        consumerUsage |= GRALLOC1_CONSUMER_USAGE_GPU_DATA_BUFFER;
    }

    return consumerUsage;
}

Error Gralloc1Allocator::createDescriptor(
    const IMapper::BufferDescriptorInfo& info,
    gralloc1_buffer_descriptor_t* outDescriptor) {
    gralloc1_buffer_descriptor_t descriptor;

    int32_t error = mDispatch.createDescriptor(mDevice, &descriptor);

    if (error == GRALLOC1_ERROR_NONE) {
        error = mDispatch.setDimensions(mDevice, descriptor, info.width,
                                        info.height);
    }
    if (error == GRALLOC1_ERROR_NONE) {
        error = mDispatch.setFormat(mDevice, descriptor,
                                    static_cast<int32_t>(info.format));
    }
    if (error == GRALLOC1_ERROR_NONE) {
        if (mCapabilities.layeredBuffers) {
            error =
                mDispatch.setLayerCount(mDevice, descriptor, info.layerCount);
        } else if (info.layerCount > 1) {
            error = GRALLOC1_ERROR_UNSUPPORTED;
        }
    }
    uint64_t producerUsage = 0;
    uint64_t consumerUsage = 0;

    android_convertGralloc0To1Usage(static_cast<uint32_t>(info.usage),
        &producerUsage, &consumerUsage);

    if (error == GRALLOC1_ERROR_NONE) {
        error = mDispatch.setProducerUsage(mDevice, descriptor,
                                           producerUsage);
    }
    if (error == GRALLOC1_ERROR_NONE) {
        error = mDispatch.setConsumerUsage(mDevice, descriptor,
                                           consumerUsage);
    }

    if (error == GRALLOC1_ERROR_NONE) {
        *outDescriptor = descriptor;
    } else {
        mDispatch.destroyDescriptor(mDevice, descriptor);
    }

    return toError(error);
}

Error Gralloc1Allocator::allocateOne(gralloc1_buffer_descriptor_t descriptor,
                                     buffer_handle_t* outBuffer,
                                     uint32_t* outStride) {
    buffer_handle_t buffer = nullptr;
    int32_t error = mDispatch.allocate(mDevice, 1, &descriptor, &buffer);
    if (error != GRALLOC1_ERROR_NONE && error != GRALLOC1_ERROR_NOT_SHARED) {
        return toError(error);
    }

    uint32_t stride = 0;
    error = mDispatch.getStride(mDevice, buffer, &stride);
    if (error != GRALLOC1_ERROR_NONE && error != GRALLOC1_ERROR_UNDEFINED) {
        mDispatch.release(mDevice, buffer);
        return toError(error);
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
