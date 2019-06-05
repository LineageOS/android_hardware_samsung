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

#include "Gralloc1On0Adapter.h"
#include "gralloc1-adapter.h"

int gralloc1_adapter_device_open(const struct hw_module_t* module,
        const char* id, struct hw_device_t** device)
{
    if (strcmp(id, GRALLOC_HARDWARE_MODULE_ID) != 0) {
        ALOGE("unknown gralloc1 device id: %s", id);
        return -EINVAL;
    }

    auto adapter_device = new android::hardware::Gralloc1On0Adapter(module);
    *device = &adapter_device->common;

    return 0;
}
