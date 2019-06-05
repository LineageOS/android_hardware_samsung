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

#ifndef ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC1ALLOCATOR_H
#define ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC1ALLOCATOR_H

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <hardware/gralloc1.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::mapper::V2_0::IMapper;
using android::hardware::graphics::mapper::V2_0::BufferDescriptor;
using android::hardware::graphics::mapper::V2_0::Error;

class Gralloc1Allocator : public IAllocator {
   public:
    Gralloc1Allocator(const hw_module_t* module);
    virtual ~Gralloc1Allocator();

    // IAllocator interface
    Return<void> dumpDebugInfo(dumpDebugInfo_cb hidl_cb) override;
    Return<void> allocate(const BufferDescriptor& descriptor, uint32_t count,
                          allocate_cb hidl_cb) override;

   private:
    void initCapabilities();

    template <typename T>
    void initDispatch(gralloc1_function_descriptor_t desc, T* outPfn);
    void initDispatch();

    static Error toError(int32_t error);
    static uint64_t toProducerUsage(uint64_t usage);
    static uint64_t toConsumerUsage(uint64_t usage);

    Error createDescriptor(const IMapper::BufferDescriptorInfo& info,
                           gralloc1_buffer_descriptor_t* outDescriptor);
    Error allocateOne(gralloc1_buffer_descriptor_t descriptor,
                      buffer_handle_t* outBuffer, uint32_t* outStride);

    gralloc1_device_t* mDevice;

    struct {
        bool layeredBuffers;
    } mCapabilities;

    struct {
        GRALLOC1_PFN_DUMP dump;
        GRALLOC1_PFN_CREATE_DESCRIPTOR createDescriptor;
        GRALLOC1_PFN_DESTROY_DESCRIPTOR destroyDescriptor;
        GRALLOC1_PFN_SET_DIMENSIONS setDimensions;
        GRALLOC1_PFN_SET_FORMAT setFormat;
        GRALLOC1_PFN_SET_LAYER_COUNT setLayerCount;
        GRALLOC1_PFN_SET_CONSUMER_USAGE setConsumerUsage;
        GRALLOC1_PFN_SET_PRODUCER_USAGE setProducerUsage;
        GRALLOC1_PFN_GET_STRIDE getStride;
        GRALLOC1_PFN_ALLOCATE allocate;
        GRALLOC1_PFN_RELEASE release;
    } mDispatch;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC1ALLOCATOR_H
