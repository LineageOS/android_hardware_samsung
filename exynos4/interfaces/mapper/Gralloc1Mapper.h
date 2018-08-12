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

#ifndef ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC1MAPPER_H
#define ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC1MAPPER_H

#include "GrallocMapper.h"

#include <hardware/gralloc1.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

class Gralloc1Mapper : public GrallocMapper {
   public:
    Gralloc1Mapper(const hw_module_t* module);
    ~Gralloc1Mapper();

   private:
    void initCapabilities();

    template <typename T>
    void initDispatch(gralloc1_function_descriptor_t desc, T* outPfn);
    void initDispatch();

    static Error toError(int32_t error);
    static bool toYCbCrLayout(const android_flex_layout& flex,
                              YCbCrLayout* outLayout);
    static gralloc1_rect_t asGralloc1Rect(const IMapper::Rect& rect);

    Error registerBuffer(buffer_handle_t bufferHandle) override;
    void unregisterBuffer(buffer_handle_t bufferHandle) override;
    Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                     const IMapper::Rect& accessRegion, int fenceFd,
                     void** outData) override;
    Error lockBuffer(buffer_handle_t bufferHandle, uint64_t cpuUsage,
                     const IMapper::Rect& accessRegion, int fenceFd,
                     YCbCrLayout* outLayout) override;
    Error unlockBuffer(buffer_handle_t bufferHandle, int* outFenceFd) override;

    gralloc1_device_t* mDevice;

    struct {
        GRALLOC1_PFN_RETAIN retain;
        GRALLOC1_PFN_RELEASE release;
        GRALLOC1_PFN_GET_NUM_FLEX_PLANES getNumFlexPlanes;
        GRALLOC1_PFN_LOCK lock;
        GRALLOC1_PFN_LOCK_FLEX lockFlex;
        GRALLOC1_PFN_UNLOCK unlock;
    } mDispatch;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GRAPHICS_MAPPER_V2_0_GRALLOC1MAPPER_H
