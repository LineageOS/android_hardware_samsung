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

#ifndef ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC0MAPPER_H
#define ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC0MAPPER_H

#include "GrallocMapper.h"

#include <hardware/gralloc.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

class Gralloc0Mapper : public GrallocMapper {
   public:
    Gralloc0Mapper(const hw_module_t* module);

   private:
    Error registerBuffer(buffer_handle_t bufferHandle) override;
    void unregisterBuffer(buffer_handle_t bufferHandle) override;
    Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                     const IMapper::Rect& accessRegion, int fenceFd,
                     void** outData) override;
    Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                     const IMapper::Rect& accessRegion, int fenceFd,
                     YCbCrLayout* outLayout) override;
    Error unlockBuffer(buffer_handle_t bufferHandle, int* outFenceFd) override;

    const gralloc_module_t* mModule;
    uint8_t mMinor;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC0MAPPER_H
