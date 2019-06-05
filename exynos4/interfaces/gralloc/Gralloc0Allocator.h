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

#ifndef ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC0ALLOCATOR_H
#define ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC0ALLOCATOR_H

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <hardware/gralloc.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

using android::hardware::graphics::mapper::V2_0::IMapper;
using android::hardware::graphics::mapper::V2_0::BufferDescriptor;
using android::hardware::graphics::mapper::V2_0::Error;

class Gralloc0Allocator : public IAllocator {
   public:
    Gralloc0Allocator(const hw_module_t* module);
    virtual ~Gralloc0Allocator();

    // IAllocator interface
    Return<void> dumpDebugInfo(dumpDebugInfo_cb hidl_cb) override;
    Return<void> allocate(const BufferDescriptor& descriptor, uint32_t count,
                          allocate_cb hidl_cb) override;

   private:
    Error allocateOne(const IMapper::BufferDescriptorInfo& info,
                      buffer_handle_t* outBuffer, uint32_t* outStride);

    alloc_device_t* mDevice;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace allocator
}  // namespace graphics
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC0ALLOCATOR_H
