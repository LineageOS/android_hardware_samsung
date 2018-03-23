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

#ifndef ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC_H
#define ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC_H

#include <android/hardware/graphics/allocator/2.0/IAllocator.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

extern "C" IAllocator* HIDL_FETCH_IAllocator(const char* name);

} // namespace implementation
} // namespace V2_0
} // namespace allocator
} // namespace graphics
} // namespace hardware
} // namespace android

#endif // ANDROID_HARDWARE_GRAPHICS_ALLOCATOR_V2_0_GRALLOC_H
