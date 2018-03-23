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

#undef LOG_TAG
#define LOG_TAG "Gralloc1On0Adapter"
//#define LOG_NDEBUG 0

#include "Gralloc1On0Adapter.h"
#include "gralloc1-adapter.h"

#include <grallocusage/GrallocUsageConversion.h>

#include <hardware/gralloc.h>

#include <log/log.h>
#include <sync/sync.h>

#include <inttypes.h>

template <typename PFN, typename T>
static gralloc1_function_pointer_t asFP(T function)
{
    static_assert(std::is_same<PFN, T>::value, "Incompatible function pointer");
    return reinterpret_cast<gralloc1_function_pointer_t>(function);
}

namespace android {
namespace hardware {

Gralloc1On0Adapter::Gralloc1On0Adapter(const hw_module_t* module)
  : gralloc1_device_t(),
    mModule(reinterpret_cast<const gralloc_module_t*>(module)),
    mDevice(nullptr)
{
    ALOGV("Constructing");

    int minor = 0;
    mModule->perform(mModule,
            GRALLOC1_ADAPTER_PERFORM_GET_REAL_MODULE_API_VERSION_MINOR,
            &minor);
    mMinorVersion = minor;

    common.tag = HARDWARE_DEVICE_TAG,
    common.version = HARDWARE_DEVICE_API_VERSION(0, 0),
    common.module = const_cast<struct hw_module_t*>(module),
    common.close = closeHook,

    getCapabilities = getCapabilitiesHook;
    getFunction = getFunctionHook;
    int error = ::gralloc_open(&(mModule->common), &mDevice);
    if (error) {
        ALOGE("Failed to open gralloc0 module: %d", error);
    }
    ALOGV("Opened gralloc0 device %p", mDevice);
}

Gralloc1On0Adapter::~Gralloc1On0Adapter()
{
    ALOGV("Destructing");
    if (mDevice) {
        ALOGV("Closing gralloc0 device %p", mDevice);
        ::gralloc_close(mDevice);
    }
}

void Gralloc1On0Adapter::doGetCapabilities(uint32_t* outCount,
                                           int32_t* /*outCapabilities*/) {
    *outCount = 0;
}

gralloc1_function_pointer_t Gralloc1On0Adapter::doGetFunction(
        int32_t intDescriptor)
{
    constexpr auto lastDescriptor =
            static_cast<int32_t>(GRALLOC1_LAST_FUNCTION);
    if (intDescriptor < 0 || intDescriptor > lastDescriptor) {
        ALOGE("Invalid function descriptor");
        return nullptr;
    }

    auto descriptor =
            static_cast<gralloc1_function_descriptor_t>(intDescriptor);
    switch (descriptor) {
        case GRALLOC1_FUNCTION_DUMP:
            return asFP<GRALLOC1_PFN_DUMP>(dumpHook);
        case GRALLOC1_FUNCTION_CREATE_DESCRIPTOR:
            return asFP<GRALLOC1_PFN_CREATE_DESCRIPTOR>(createDescriptorHook);
        case GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR:
            return asFP<GRALLOC1_PFN_DESTROY_DESCRIPTOR>(destroyDescriptorHook);
        case GRALLOC1_FUNCTION_SET_CONSUMER_USAGE:
            return asFP<GRALLOC1_PFN_SET_CONSUMER_USAGE>(setConsumerUsageHook);
        case GRALLOC1_FUNCTION_SET_DIMENSIONS:
            return asFP<GRALLOC1_PFN_SET_DIMENSIONS>(setDimensionsHook);
        case GRALLOC1_FUNCTION_SET_FORMAT:
            return asFP<GRALLOC1_PFN_SET_FORMAT>(setFormatHook);
        case GRALLOC1_FUNCTION_SET_LAYER_COUNT:
            return asFP<GRALLOC1_PFN_SET_LAYER_COUNT>(setLayerCountHook);
        case GRALLOC1_FUNCTION_SET_PRODUCER_USAGE:
            return asFP<GRALLOC1_PFN_SET_PRODUCER_USAGE>(setProducerUsageHook);
        case GRALLOC1_FUNCTION_GET_BACKING_STORE:
            return asFP<GRALLOC1_PFN_GET_BACKING_STORE>(
                    bufferHook<decltype(&Buffer::getBackingStore),
                    &Buffer::getBackingStore, gralloc1_backing_store_t*>);
        case GRALLOC1_FUNCTION_GET_CONSUMER_USAGE:
            return asFP<GRALLOC1_PFN_GET_CONSUMER_USAGE>(getConsumerUsageHook);
        case GRALLOC1_FUNCTION_GET_DIMENSIONS:
            return asFP<GRALLOC1_PFN_GET_DIMENSIONS>(
                    bufferHook<decltype(&Buffer::getDimensions),
                    &Buffer::getDimensions, uint32_t*, uint32_t*>);
        case GRALLOC1_FUNCTION_GET_FORMAT:
            return asFP<GRALLOC1_PFN_GET_FORMAT>(
                    bufferHook<decltype(&Buffer::getFormat),
                    &Buffer::getFormat, int32_t*>);
        case GRALLOC1_FUNCTION_GET_LAYER_COUNT:
            return asFP<GRALLOC1_PFN_GET_LAYER_COUNT>(
                    bufferHook<decltype(&Buffer::getLayerCount),
                    &Buffer::getLayerCount, uint32_t*>);
        case GRALLOC1_FUNCTION_GET_PRODUCER_USAGE:
            return asFP<GRALLOC1_PFN_GET_PRODUCER_USAGE>(getProducerUsageHook);
        case GRALLOC1_FUNCTION_GET_STRIDE:
            return asFP<GRALLOC1_PFN_GET_STRIDE>(
                    bufferHook<decltype(&Buffer::getStride),
                    &Buffer::getStride, uint32_t*>);
        case GRALLOC1_FUNCTION_ALLOCATE:
            if (mDevice != nullptr) {
                return asFP<GRALLOC1_PFN_ALLOCATE>(allocateHook);
            } else {
                return nullptr;
            }
        case GRALLOC1_FUNCTION_RETAIN:
            return asFP<GRALLOC1_PFN_RETAIN>(retainHook);
        case GRALLOC1_FUNCTION_RELEASE:
            return asFP<GRALLOC1_PFN_RELEASE>(releaseHook);
        case GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES:
            return asFP<GRALLOC1_PFN_GET_NUM_FLEX_PLANES>(
                    bufferHook<decltype(&Buffer::getNumFlexPlanes),
                    &Buffer::getNumFlexPlanes, uint32_t*>);
        case GRALLOC1_FUNCTION_LOCK:
            return asFP<GRALLOC1_PFN_LOCK>(
                    lockHook<void*, &Gralloc1On0Adapter::lock>);
        case GRALLOC1_FUNCTION_LOCK_FLEX:
            return asFP<GRALLOC1_PFN_LOCK_FLEX>(
                    lockHook<struct android_flex_layout,
                    &Gralloc1On0Adapter::lockFlex>);
        case GRALLOC1_FUNCTION_UNLOCK:
            return asFP<GRALLOC1_PFN_UNLOCK>(unlockHook);
        case GRALLOC1_FUNCTION_INVALID:
            ALOGE("Invalid function descriptor");
            return nullptr;
    }

    ALOGE("Unknown function descriptor: %d", intDescriptor);
    return nullptr;
}

void Gralloc1On0Adapter::dump(uint32_t* outSize, char* outBuffer)
{
    ALOGV("dump(%u (%p), %p", outSize ? *outSize : 0, outSize, outBuffer);

    if (!mDevice->dump) {
        // dump is optional on gralloc0 implementations
        *outSize = 0;
        return;
    }

    if (!outBuffer) {
        constexpr int32_t BUFFER_LENGTH = 4096;
        char buffer[BUFFER_LENGTH] = {};
        mDevice->dump(mDevice, buffer, BUFFER_LENGTH);
        buffer[BUFFER_LENGTH - 1] = 0; // Ensure the buffer is null-terminated
        size_t actualLength = std::strlen(buffer);
        mCachedDump.resize(actualLength);
        std::copy_n(buffer, actualLength, mCachedDump.begin());
        *outSize = static_cast<uint32_t>(actualLength);
    } else {
        *outSize = std::min(*outSize,
                static_cast<uint32_t>(mCachedDump.size()));
        outBuffer = std::copy_n(mCachedDump.cbegin(), *outSize, outBuffer);
    }
}

gralloc1_error_t Gralloc1On0Adapter::createDescriptor(
        gralloc1_buffer_descriptor_t* outDescriptor)
{
    auto descriptorId = sNextBufferDescriptorId++;
    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    mDescriptors.emplace(descriptorId, std::make_shared<Descriptor>());

    ALOGV("Created descriptor %" PRIu64, descriptorId);

    *outDescriptor = descriptorId;
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::destroyDescriptor(
        gralloc1_buffer_descriptor_t descriptor)
{
    ALOGV("Destroying descriptor %" PRIu64, descriptor);

    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    if (mDescriptors.count(descriptor) == 0) {
        return GRALLOC1_ERROR_BAD_DESCRIPTOR;
    }

    mDescriptors.erase(descriptor);
    return GRALLOC1_ERROR_NONE;
}

Gralloc1On0Adapter::Buffer::Buffer(buffer_handle_t handle,
        gralloc1_backing_store_t store, const Descriptor& descriptor,
        uint32_t stride, uint32_t /* numFlexPlanes */, bool wasAllocated)
  : mHandle(handle),
    mReferenceCount(1),
    mStore(store),
    mDescriptor(descriptor),
    mStride(stride),
    mWasAllocated(wasAllocated) {}

gralloc1_error_t Gralloc1On0Adapter::allocate(
        gralloc1_buffer_descriptor_t id,
        const std::shared_ptr<Descriptor>& descriptor,
        buffer_handle_t* outBufferHandle)
{
    ALOGV("allocate(%" PRIu64 ")", id);

    // If this function is being called, it's because we handed out its function
    // pointer, which only occurs when mDevice has been loaded successfully and
    // we are permitted to allocate

    int usage = android_convertGralloc1To0Usage(
            descriptor->producerUsage, descriptor->consumerUsage);
    buffer_handle_t handle = nullptr;
    int stride = 0;
    ALOGV("Calling alloc(%p, %u, %u, %i, %u)", mDevice, descriptor->width,
            descriptor->height, descriptor->format, usage);
    auto error = mDevice->alloc(mDevice,
            static_cast<int>(descriptor->width),
            static_cast<int>(descriptor->height), descriptor->format,
            usage, &handle, &stride);
    if (error != 0) {
        ALOGE("gralloc0 allocation failed: %d (%s)", error,
                strerror(-error));
        return GRALLOC1_ERROR_NO_RESOURCES;
    }

    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_SET_USAGES,
            handle,
            static_cast<int>(descriptor->producerUsage),
            static_cast<int>(descriptor->consumerUsage));

    uint64_t backingStore = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_BACKING_STORE,
            handle, &backingStore);
    int numFlexPlanes = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_NUM_FLEX_PLANES,
            handle, &numFlexPlanes);

    *outBufferHandle = handle;
    auto buffer = std::make_shared<Buffer>(handle, backingStore,
            *descriptor, stride, numFlexPlanes, true);

    std::lock_guard<std::mutex> lock(mBufferMutex);
    mBuffers.emplace(handle, std::move(buffer));

    return GRALLOC1_ERROR_NONE;
}

int32_t Gralloc1On0Adapter::allocateHook(gralloc1_device* device,
        uint32_t numDescriptors,
        const gralloc1_buffer_descriptor_t* descriptors,
        buffer_handle_t* outBuffers)
{
    if (!outBuffers) {
        return GRALLOC1_ERROR_UNDEFINED;
    }

    auto adapter = getAdapter(device);

    gralloc1_error_t error = GRALLOC1_ERROR_NONE;
    uint32_t i;
    for (i = 0; i < numDescriptors; i++) {
        auto descriptor = adapter->getDescriptor(descriptors[i]);
        if (!descriptor) {
            error = GRALLOC1_ERROR_BAD_DESCRIPTOR;
            break;
        }

        buffer_handle_t bufferHandle = nullptr;
        error = adapter->allocate(descriptors[i], descriptor, &bufferHandle);
        if (error != GRALLOC1_ERROR_NONE) {
            break;
        }

        outBuffers[i] = bufferHandle;
    }

    if (error == GRALLOC1_ERROR_NONE) {
        if (numDescriptors > 1) {
            error = GRALLOC1_ERROR_NOT_SHARED;
        }
    } else {
        for (uint32_t j = 0; j < i; j++) {
            adapter->release(adapter->getBuffer(outBuffers[j]));
            outBuffers[j] = nullptr;
        }
    }

    return error;
}

gralloc1_error_t Gralloc1On0Adapter::retain(
        const std::shared_ptr<Buffer>& buffer)
{
    std::lock_guard<std::mutex> lock(mBufferMutex);
    buffer->retain();
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::release(
        const std::shared_ptr<Buffer>& buffer)
{
    std::lock_guard<std::mutex> lock(mBufferMutex);
    if (!buffer->release()) {
        return GRALLOC1_ERROR_NONE;
    }

    buffer_handle_t handle = buffer->getHandle();
    if (buffer->wasAllocated()) {
        ALOGV("Calling free(%p)", handle);
        int result = mDevice->free(mDevice, handle);
        if (result != 0) {
            ALOGE("gralloc0 free failed: %d", result);
        }
    } else {
        ALOGV("Calling unregisterBuffer(%p)", handle);
        int result = mModule->unregisterBuffer(mModule, handle);
        if (result != 0) {
            ALOGE("gralloc0 unregister failed: %d", result);
        }
    }

    mBuffers.erase(handle);
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::retain(buffer_handle_t bufferHandle)
{
    ALOGV("retain(%p)", bufferHandle);

    std::lock_guard<std::mutex> lock(mBufferMutex);

    if (mBuffers.count(bufferHandle) != 0) {
        mBuffers[bufferHandle]->retain();
        return GRALLOC1_ERROR_NONE;
    }

    ALOGV("Calling registerBuffer(%p)", bufferHandle);
    int result = mModule->registerBuffer(mModule, bufferHandle);
    if (result != 0) {
        ALOGE("gralloc0 register failed: %d", result);
        return GRALLOC1_ERROR_NO_RESOURCES;
    }

    uint64_t backingStore = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_BACKING_STORE,
            bufferHandle, &backingStore);

    int numFlexPlanes = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_NUM_FLEX_PLANES,
            bufferHandle, &numFlexPlanes);

    int stride = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_STRIDE,
            bufferHandle, &stride);

    int width = 0;
    int height = 0;
    int format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    int producerUsage = 0;
    int consumerUsage = 0;
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_DIMENSIONS,
            bufferHandle, &width, &height);
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_FORMAT,
            bufferHandle, &format);
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_PRODUCER_USAGE,
            bufferHandle, &producerUsage);
    mModule->perform(mModule, GRALLOC1_ADAPTER_PERFORM_GET_CONSUMER_USAGE,
            bufferHandle, &consumerUsage);

    Descriptor descriptor;
    descriptor.setDimensions(width, height);
    descriptor.setFormat(format);
    descriptor.setProducerUsage(
            static_cast<gralloc1_producer_usage_t>(producerUsage));
    descriptor.setConsumerUsage(
            static_cast<gralloc1_consumer_usage_t>(consumerUsage));

    auto buffer = std::make_shared<Buffer>(bufferHandle, backingStore,
            descriptor, stride, numFlexPlanes, false);
    mBuffers.emplace(bufferHandle, std::move(buffer));
    return GRALLOC1_ERROR_NONE;
}

static void syncWaitForever(int fd, const char* logname)
{
    if (fd < 0) {
        return;
    }

    const int warningTimeout = 3500;
    const int error = sync_wait(fd, warningTimeout);
    if (error < 0 && errno == ETIME) {
        ALOGE("%s: fence %d didn't signal in %u ms", logname, fd,
                warningTimeout);
        sync_wait(fd, -1);
    }
}

gralloc1_error_t Gralloc1On0Adapter::lock(
        const std::shared_ptr<Buffer>& buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t& accessRegion, void** outData,
        int acquireFence)
{
    if (mMinorVersion >= 3) {
        int result = mModule->lockAsync(mModule, buffer->getHandle(),
                android_convertGralloc1To0Usage(producerUsage, consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData, acquireFence);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    } else {
        syncWaitForever(acquireFence, "Gralloc1On0Adapter::lock");

        int result = mModule->lock(mModule, buffer->getHandle(),
                android_convertGralloc1To0Usage(producerUsage, consumerUsage),
                accessRegion.left, accessRegion.top, accessRegion.width,
                accessRegion.height, outData);
        ALOGV("gralloc0 lock returned %d", result);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        } else if (acquireFence >= 0) {
            close(acquireFence);
        }
    }
    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::lockFlex(
        const std::shared_ptr<Buffer>& buffer,
        gralloc1_producer_usage_t producerUsage,
        gralloc1_consumer_usage_t consumerUsage,
        const gralloc1_rect_t& accessRegion,
        struct android_flex_layout* outFlex,
        int acquireFence)
{
    if (mMinorVersion >= 3) {
        int result = mModule->perform(mModule,
                GRALLOC1_ADAPTER_PERFORM_LOCK_FLEX,
                buffer->getHandle(),
                static_cast<int>(producerUsage),
                static_cast<int>(consumerUsage),
                accessRegion.left,
                accessRegion.top,
                accessRegion.width,
                accessRegion.height,
                outFlex, acquireFence);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        }
    } else {
        syncWaitForever(acquireFence, "Gralloc1On0Adapter::lockFlex");

        int result = mModule->perform(mModule,
                GRALLOC1_ADAPTER_PERFORM_LOCK_FLEX,
                buffer->getHandle(),
                static_cast<int>(producerUsage),
                static_cast<int>(consumerUsage),
                accessRegion.left,
                accessRegion.top,
                accessRegion.width,
                accessRegion.height,
                outFlex, -1);
        if (result != 0) {
            return GRALLOC1_ERROR_UNSUPPORTED;
        } else if (acquireFence >= 0) {
            close(acquireFence);
        }
    }

    return GRALLOC1_ERROR_NONE;
}

gralloc1_error_t Gralloc1On0Adapter::unlock(
        const std::shared_ptr<Buffer>& buffer,
        int* outReleaseFence)
{
    if (mMinorVersion >= 3) {
        int fenceFd = -1;
        int result = mModule->unlockAsync(mModule, buffer->getHandle(),
                &fenceFd);
        if (result != 0) {
            close(fenceFd);
            ALOGE("gralloc0 unlockAsync failed: %d", result);
        } else {
            *outReleaseFence = fenceFd;
        }
    } else {
        int result = mModule->unlock(mModule, buffer->getHandle());
        if (result != 0) {
            ALOGE("gralloc0 unlock failed: %d", result);
        } else {
            *outReleaseFence = -1;
        }
    }
    return GRALLOC1_ERROR_NONE;
}

std::shared_ptr<Gralloc1On0Adapter::Descriptor>
Gralloc1On0Adapter::getDescriptor(gralloc1_buffer_descriptor_t descriptorId)
{
    std::lock_guard<std::mutex> lock(mDescriptorMutex);
    if (mDescriptors.count(descriptorId) == 0) {
        return nullptr;
    }

    return mDescriptors[descriptorId];
}

std::shared_ptr<Gralloc1On0Adapter::Buffer> Gralloc1On0Adapter::getBuffer(
        buffer_handle_t bufferHandle)
{
    std::lock_guard<std::mutex> lock(mBufferMutex);
    if (mBuffers.count(bufferHandle) == 0) {
        return nullptr;
    }

    return mBuffers[bufferHandle];
}

std::atomic<gralloc1_buffer_descriptor_t>
        Gralloc1On0Adapter::sNextBufferDescriptorId(1);

} // namespace hardware
} // namespace android
