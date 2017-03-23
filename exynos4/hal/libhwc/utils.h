/*
 * Copyright (C) 2010 Samsung Electronics Co.,LTD., for the ODROID code
 * Copyright (C) 2012 The Android Open Source Project, for the samsung_slsi/exynos5 code
 * Copyright (C) 2013 Paul Kocialkowski, for the V4L2 code
 * Copyright (C) 2017 The NamelessRom Project, for making it work
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

#ifndef ANDROID_HWCOMPOSER_UTILS_H
#define ANDROID_HWCOMPOSER_UTILS_H

#include <math.h>
#include "FimgApi.h"

bool is_yuv_format(unsigned int color_format);
uint8_t format_to_bpp(int format);
enum s3c_fb_pixel_format format_to_s3c_format(int format);
enum s3c_fb_blending blending_to_s3c_blending(int32_t blending);
unsigned int formatValueHAL2G2D(int hal_format,
        color_format *g2d_format,
        pixel_order *g2d_order,
        uint32_t *g2d_bpp);
enum rotation rotateValueHAL2G2D(unsigned char transform);
int rotateValueHAL2PP(unsigned char transform);
int multipleOf2(int number);

// Copied from qcom hwc https://github.com/CyanogenMod/android_hardware_qcom_display/blob/cm-14.1/msm8974/libhwcomposer/hwc_utils.h
inline hwc_rect_t integerizeSourceCrop(const hwc_frect_t& cropF) {
    hwc_rect_t cropI = {0, 0, 0, 0};
    cropI.left = int(ceilf(cropF.left));
    cropI.top = int(ceilf(cropF.top));
    cropI.right = int(floorf(cropF.right));
    cropI.bottom = int(floorf(cropF.bottom));
    return cropI;
}
#endif //ANDROID_HWCOMPOSER_UTILS_H
