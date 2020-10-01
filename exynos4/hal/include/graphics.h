/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * pixel format definitions
 */

enum {
    /*
     * "linear" color pixel formats:
     *
     * The pixel formats below contain sRGB data but are otherwise treated
     * as linear formats, i.e.: no special operation is performed when
     * reading or writing into a buffer in one of these formats
     */
    HAL_PIXEL_FORMAT_RGBA_5551          = 6,
    HAL_PIXEL_FORMAT_RGBA_4444          = 7,
};

#ifdef __cplusplus
}
#endif

/* LEGACY_SYSTEM_CORE_INCLUDE_ANDROID_GRAPHICS_H */
