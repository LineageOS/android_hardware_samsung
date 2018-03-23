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

#define LOG_TAG "GrallocPassthrough"

#include "Gralloc.h"
#include "Gralloc0Allocator.h"
#include "Gralloc1Allocator.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace graphics {
namespace allocator {
namespace V2_0 {
namespace implementation {

IAllocator* HIDL_FETCH_IAllocator(const char* /* name */) {
    const hw_module_t* module = nullptr;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err) {
        ALOGE("failed to get gralloc module");
        return nullptr;
    }

    uint8_t major = (module->module_api_version >> 8) & 0xff;
    switch (major) {
        case 1:
            return new Gralloc1Allocator(module);
        case 0:
            return new Gralloc0Allocator(module);
        default:
            ALOGE("unknown gralloc module major version %d", major);
            return nullptr;
    }
}

} // namespace implementation
} // namespace V2_0
} // namespace allocator
} // namespace graphics
} // namespace hardware
} // namespace android
