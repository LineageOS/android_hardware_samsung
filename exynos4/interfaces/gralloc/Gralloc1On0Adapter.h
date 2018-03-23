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

#ifndef ANDROID_HARDWARE_GRALLOC_1_ON_0_ADAPTER_H
#define ANDROID_HARDWARE_GRALLOC_1_ON_0_ADAPTER_H

#include <hardware/gralloc1.h>
#include <log/log.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

struct gralloc_module_t;
struct alloc_device_t;

namespace android {
namespace hardware {

class Gralloc1On0Adapter : public gralloc1_device_t
{
public:
    Gralloc1On0Adapter(const hw_module_t* module);
    ~Gralloc1On0Adapter();

    gralloc1_device_t* getDevice() {
        return static_cast<gralloc1_device_t*>(this);
    }

private:
    static inline Gralloc1On0Adapter* getAdapter(gralloc1_device_t* device) {
        return static_cast<Gralloc1On0Adapter*>(device);
    }

    static int closeHook(struct hw_device_t* device) {
        delete getAdapter(reinterpret_cast<gralloc1_device_t*>(device));
        return 0;
    }

    // getCapabilities

    void doGetCapabilities(uint32_t* outCount,
            int32_t* /*gralloc1_capability_t*/ outCapabilities);
    static void getCapabilitiesHook(gralloc1_device_t* device,
            uint32_t* outCount,
            int32_t* /*gralloc1_capability_t*/ outCapabilities) {
        getAdapter(device)->doGetCapabilities(outCount, outCapabilities);
    }

    // getFunction

    gralloc1_function_pointer_t doGetFunction(
            int32_t /*gralloc1_function_descriptor_t*/ descriptor);
    static gralloc1_function_pointer_t getFunctionHook(
            gralloc1_device_t* device,
            int32_t /*gralloc1_function_descriptor_t*/ descriptor) {
        return getAdapter(device)->doGetFunction(descriptor);
    }

    // dump

    void dump(uint32_t* outSize, char* outBuffer);
    static void dumpHook(gralloc1_device_t* device, uint32_t* outSize,
            char* outBuffer) {
        return getAdapter(device)->dump(outSize, outBuffer);
    }
    std::string mCachedDump;

    // Buffer descriptor lifecycle functions

    struct Descriptor;

    gralloc1_error_t createDescriptor(
            gralloc1_buffer_descriptor_t* outDescriptor);
    static int32_t createDescriptorHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t* outDescriptor) {
        auto error = getAdapter(device)->createDescriptor(outDescriptor);
        return static_cast<int32_t>(error);
    }

    gralloc1_error_t destroyDescriptor(gralloc1_buffer_descriptor_t descriptor);
    static int32_t destroyDescriptorHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptor) {
        auto error = getAdapter(device)->destroyDescriptor(descriptor);
        return static_cast<int32_t>(error);
    }

    // Buffer descriptor modification functions

    struct Descriptor : public std::enable_shared_from_this<Descriptor> {
        Descriptor()
          : width(0),
            height(0),
            format(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED),
            layerCount(1),
            producerUsage(GRALLOC1_PRODUCER_USAGE_NONE),
            consumerUsage(GRALLOC1_CONSUMER_USAGE_NONE) {}

        gralloc1_error_t setDimensions(uint32_t w, uint32_t h) {
            width = w;
            height = h;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t setFormat(int32_t f) {
            format = f;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t setLayerCount(uint32_t lc) {
            layerCount = lc;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t setProducerUsage(gralloc1_producer_usage_t usage) {
            producerUsage = usage;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t setConsumerUsage(gralloc1_consumer_usage_t usage) {
            consumerUsage = usage;
            return GRALLOC1_ERROR_NONE;
        }

        uint32_t width;
        uint32_t height;
        int32_t format;
        uint32_t layerCount;
        gralloc1_producer_usage_t producerUsage;
        gralloc1_consumer_usage_t consumerUsage;
    };

    template <typename ...Args>
    static int32_t callDescriptorFunction(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId,
            gralloc1_error_t (Descriptor::*member)(Args...), Args... args) {
        auto descriptor = getAdapter(device)->getDescriptor(descriptorId);
        if (!descriptor) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_DESCRIPTOR);
        }
        auto error = ((*descriptor).*member)(std::forward<Args>(args)...);
        return static_cast<int32_t>(error);
    }

    static int32_t setConsumerUsageHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId, uint64_t intUsage) {
        auto usage = static_cast<gralloc1_consumer_usage_t>(intUsage);
        return callDescriptorFunction(device, descriptorId,
                &Descriptor::setConsumerUsage, usage);
    }

    static int32_t setDimensionsHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId, uint32_t width,
            uint32_t height) {
        return callDescriptorFunction(device, descriptorId,
                &Descriptor::setDimensions, width, height);
    }

    static int32_t setFormatHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId, int32_t format) {
        return callDescriptorFunction(device, descriptorId,
                &Descriptor::setFormat, format);
    }

    static int32_t setLayerCountHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId, uint32_t layerCount) {
        return callDescriptorFunction(device, descriptorId,
                &Descriptor::setLayerCount, layerCount);
    }

    static int32_t setProducerUsageHook(gralloc1_device_t* device,
            gralloc1_buffer_descriptor_t descriptorId, uint64_t intUsage) {
        auto usage = static_cast<gralloc1_producer_usage_t>(intUsage);
        return callDescriptorFunction(device, descriptorId,
                &Descriptor::setProducerUsage, usage);
    }

    // Buffer handle query functions
    class Buffer {
    public:
        Buffer(buffer_handle_t handle, gralloc1_backing_store_t store,
                const Descriptor& descriptor, uint32_t stride,
                uint32_t numFlexPlanes, bool wasAllocated);

        buffer_handle_t getHandle() const { return mHandle; }

        void retain() { ++mReferenceCount; }

        // Returns true if the reference count has dropped to 0, indicating that
        // the buffer needs to be released
        bool release() { return --mReferenceCount == 0; }

        bool wasAllocated() const { return mWasAllocated; }

        gralloc1_error_t getBackingStore(
                gralloc1_backing_store_t* outStore) const {
            *outStore = mStore;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getConsumerUsage(
                gralloc1_consumer_usage_t* outUsage) const {
            *outUsage = mDescriptor.consumerUsage;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getDimensions(uint32_t* outWidth,
                uint32_t* outHeight) const {
            *outWidth = mDescriptor.width;
            *outHeight = mDescriptor.height;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getFormat(int32_t* outFormat) const {
            *outFormat = mDescriptor.format;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getLayerCount(uint32_t* outLayerCount) const {
            *outLayerCount = mDescriptor.layerCount;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getNumFlexPlanes(uint32_t* outNumPlanes) const {
            *outNumPlanes = 4;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getProducerUsage(
                gralloc1_producer_usage_t* outUsage) const {
            *outUsage = mDescriptor.producerUsage;
            return GRALLOC1_ERROR_NONE;
        }

        gralloc1_error_t getStride(uint32_t* outStride) const {
            *outStride = mStride;
            return GRALLOC1_ERROR_NONE;
        }

    private:

        const buffer_handle_t mHandle;
        size_t mReferenceCount;

        const gralloc1_backing_store_t mStore;
        const Descriptor mDescriptor;
        const uint32_t mStride;

        // Whether this buffer allocated in this process (as opposed to just
        // being retained here), which determines whether to free or unregister
        // the buffer when this Buffer is released
        const bool mWasAllocated;
    };

    template <typename ...Args>
    static int32_t callBufferFunction(gralloc1_device_t* device,
            buffer_handle_t bufferHandle,
            gralloc1_error_t (Buffer::*member)(Args...) const, Args... args) {
        auto buffer = getAdapter(device)->getBuffer(bufferHandle);
        if (!buffer) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_HANDLE);
        }
        auto error = ((*buffer).*member)(std::forward<Args>(args)...);
        return static_cast<int32_t>(error);
    }

    template <typename MF, MF memFunc, typename ...Args>
    static int32_t bufferHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle, Args... args) {
        return Gralloc1On0Adapter::callBufferFunction(device, bufferHandle,
                memFunc, std::forward<Args>(args)...);
    }

    static int32_t getConsumerUsageHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle, uint64_t* outUsage) {
        auto usage = GRALLOC1_CONSUMER_USAGE_NONE;
        auto error = callBufferFunction(device, bufferHandle,
                &Buffer::getConsumerUsage, &usage);
        if (error == GRALLOC1_ERROR_NONE) {
            *outUsage = static_cast<uint64_t>(usage);
        }
        return error;
    }

    static int32_t getProducerUsageHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle, uint64_t* outUsage) {
        auto usage = GRALLOC1_PRODUCER_USAGE_NONE;
        auto error = callBufferFunction(device, bufferHandle,
                &Buffer::getProducerUsage, &usage);
        if (error == GRALLOC1_ERROR_NONE) {
            *outUsage = static_cast<uint64_t>(usage);
        }
        return error;
    }

    // Buffer management functions

    gralloc1_error_t allocate(
            gralloc1_buffer_descriptor_t id,
            const std::shared_ptr<Descriptor>& descriptor,
            buffer_handle_t* outBufferHandle);
    static int32_t allocateHook(gralloc1_device* device,
            uint32_t numDescriptors,
            const gralloc1_buffer_descriptor_t* descriptors,
            buffer_handle_t* outBuffers);

    gralloc1_error_t retain(const std::shared_ptr<Buffer>& buffer);
    gralloc1_error_t retain(buffer_handle_t bufferHandle);
    static int32_t retainHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle)
    {
        auto adapter = getAdapter(device);
        return adapter->retain(bufferHandle);
    }

    gralloc1_error_t release(const std::shared_ptr<Buffer>& buffer);
    static int32_t releaseHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle) {
        auto adapter = getAdapter(device);

        auto buffer = adapter->getBuffer(bufferHandle);
        if (!buffer) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_HANDLE);
        }

        auto error = adapter->release(buffer);
        return static_cast<int32_t>(error);
    }

    // Buffer access functions

    gralloc1_error_t lock(const std::shared_ptr<Buffer>& buffer,
            gralloc1_producer_usage_t producerUsage,
            gralloc1_consumer_usage_t consumerUsage,
            const gralloc1_rect_t& accessRegion, void** outData,
            int acquireFence);
    gralloc1_error_t lockFlex(const std::shared_ptr<Buffer>& buffer,
            gralloc1_producer_usage_t producerUsage,
            gralloc1_consumer_usage_t consumerUsage,
            const gralloc1_rect_t& accessRegion,
            struct android_flex_layout* outFlex,
            int acquireFence);

    template <typename OUT, gralloc1_error_t (Gralloc1On0Adapter::*member)(
            const std::shared_ptr<Buffer>&, gralloc1_producer_usage_t,
            gralloc1_consumer_usage_t, const gralloc1_rect_t&, OUT*,
            int)>
    static int32_t lockHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle,
            uint64_t /*gralloc1_producer_usage_t*/ uintProducerUsage,
            uint64_t /*gralloc1_consumer_usage_t*/ uintConsumerUsage,
            const gralloc1_rect_t* accessRegion, OUT* outData,
            int32_t acquireFenceFd) {
        auto adapter = getAdapter(device);

        // Exactly one of producer and consumer usage must be *_USAGE_NONE,
        // but we can't check this until the upper levels of the framework
        // correctly distinguish between producer and consumer usage
        /*
        bool hasProducerUsage =
                uintProducerUsage != GRALLOC1_PRODUCER_USAGE_NONE;
        bool hasConsumerUsage =
                uintConsumerUsage != GRALLOC1_CONSUMER_USAGE_NONE;
        if (hasProducerUsage && hasConsumerUsage ||
                !hasProducerUsage && !hasConsumerUsage) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_VALUE);
        }
        */

        auto producerUsage =
                static_cast<gralloc1_producer_usage_t>(uintProducerUsage);
        auto consumerUsage =
                static_cast<gralloc1_consumer_usage_t>(uintConsumerUsage);

        if (!outData) {
            const auto producerCpuUsage = GRALLOC1_PRODUCER_USAGE_CPU_READ |
                    GRALLOC1_PRODUCER_USAGE_CPU_WRITE;
            if ((producerUsage & producerCpuUsage) != 0) {
                return static_cast<int32_t>(GRALLOC1_ERROR_BAD_VALUE);
            }
            if ((consumerUsage & GRALLOC1_CONSUMER_USAGE_CPU_READ) != 0) {
                return static_cast<int32_t>(GRALLOC1_ERROR_BAD_VALUE);
            }
        }

        auto buffer = adapter->getBuffer(bufferHandle);
        if (!buffer) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_HANDLE);
        }

        if (!accessRegion) {
            ALOGE("accessRegion is null");
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_VALUE);
        }

        auto error = ((*adapter).*member)(buffer, producerUsage, consumerUsage,
                *accessRegion, outData, acquireFenceFd);
        return static_cast<int32_t>(error);
    }

    gralloc1_error_t unlock(const std::shared_ptr<Buffer>& buffer,
            int* outReleaseFence);
    static int32_t unlockHook(gralloc1_device_t* device,
            buffer_handle_t bufferHandle, int32_t* outReleaseFenceFd) {
        auto adapter = getAdapter(device);

        auto buffer = adapter->getBuffer(bufferHandle);
        if (!buffer) {
            return static_cast<int32_t>(GRALLOC1_ERROR_BAD_HANDLE);
        }

        int releaseFence = -1;
        auto error = adapter->unlock(buffer, &releaseFence);
        if (error == GRALLOC1_ERROR_NONE) {
            *outReleaseFenceFd = releaseFence;
        }
        return static_cast<int32_t>(error);
    }

    // Adapter internals
    const gralloc_module_t* mModule;
    uint8_t mMinorVersion;
    alloc_device_t* mDevice;

    std::shared_ptr<Descriptor> getDescriptor(
            gralloc1_buffer_descriptor_t descriptorId);
    std::shared_ptr<Buffer> getBuffer(buffer_handle_t bufferHandle);

    static std::atomic<gralloc1_buffer_descriptor_t> sNextBufferDescriptorId;
    std::mutex mDescriptorMutex;
    std::unordered_map<gralloc1_buffer_descriptor_t,
            std::shared_ptr<Descriptor>> mDescriptors;
    std::mutex mBufferMutex;
    std::unordered_map<buffer_handle_t, std::shared_ptr<Buffer>> mBuffers;
};

} // namespace hardware
} // namespace android

#endif // ANDROID_HARDWARE_GRALLOC_1_ON_0_ADAPTER_H
