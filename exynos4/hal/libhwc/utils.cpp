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

#include "hwcomposer.h"

bool is_yuv_format(unsigned int color_format) {
    switch (color_format) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_422_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_I:
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CbYCrY_420_I:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        return true;
    default:
        return false;
    }
}

uint8_t format_to_bpp(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return 32;

    case HAL_PIXEL_FORMAT_RGB_888:
        return 24;

    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP: //taken from sec_utils_v4l2.h
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_422_P:
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        return 16;

    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_I:
    case HAL_PIXEL_FORMAT_CbYCrY_420_I:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        return 12;

    default:
        ALOGW("%s: unrecognized pixel format %u", __FUNCTION__, format);
        return 0;
    }
}

enum s3c_fb_pixel_format format_to_s3c_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return S3C_FB_PIXEL_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return S3C_FB_PIXEL_FORMAT_RGBX_8888;
    case HAL_PIXEL_FORMAT_RGB_888:
        return S3C_FB_PIXEL_FORMAT_RGB_888;
    case HAL_PIXEL_FORMAT_RGB_565:
        return S3C_FB_PIXEL_FORMAT_RGB_565;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return S3C_FB_PIXEL_FORMAT_BGRA_8888;
    default:
        return S3C_FB_PIXEL_FORMAT_MAX;
    }
}

enum s3c_fb_blending blending_to_s3c_blending(int32_t blending)
{
    switch (blending) {
    case HWC_BLENDING_NONE:
        return S3C_FB_BLENDING_NONE;
    case HWC_BLENDING_PREMULT:
        return S3C_FB_BLENDING_PREMULT;
    case HWC_BLENDING_COVERAGE:
        return S3C_FB_BLENDING_COVERAGE;
    default:
        return S3C_FB_BLENDING_MAX;
    }
}

unsigned int formatValueHAL2G2D(int hal_format,
        color_format *g2d_format,
        pixel_order *g2d_order,
        uint32_t *g2d_bpp)
{
    *g2d_format = MSK_FORMAT_END;
    *g2d_order  = ARGB_ORDER_END;
    *g2d_bpp    = 0;

    switch (hal_format) {
    /* 16bpp */
    case HAL_PIXEL_FORMAT_RGB_565: //
        *g2d_format = CF_RGB_565;
        *g2d_order  = AX_RGB;
        *g2d_bpp    = 2;
        break;
    case HAL_PIXEL_FORMAT_RGBA_4444:
        *g2d_format = CF_ARGB_4444;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 2;
        break;
        /* 32bpp */
    case HAL_PIXEL_FORMAT_RGBX_8888: //
        *g2d_format = CF_XRGB_8888;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 4;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888: //
        *g2d_format = CF_ARGB_8888;
        *g2d_order  = AX_RGB;
        *g2d_bpp    = 4;
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888: //
        *g2d_format = CF_ARGB_8888;
        *g2d_order  = AX_BGR;
        *g2d_bpp    = 4;
        break;
        /* 12bpp */
    case HAL_PIXEL_FORMAT_YCbCr_420_SP: //
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP: //
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        *g2d_format = CF_YCBCR_420;
        *g2d_order  = P2_CBCR;
        *g2d_bpp    = 1;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP: //
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
        *g2d_format = CF_YCBCR_420;
        *g2d_order  = P2_CRCB;
        *g2d_bpp    = 1;
        break;
    default:
        ALOGE("%s: no matching color format(0x%x): failed", __FUNCTION__, hal_format);
        return -1;
        break;
    }
    return 0;
}

enum rotation rotateValueHAL2G2D(unsigned char transform)
{
    switch (transform) {
    case HWC_TRANSFORM_ROT_90:  return ROT_90;
    case HWC_TRANSFORM_ROT_180: return ROT_180;
    case HWC_TRANSFORM_ROT_270: return ROT_270;
    case HWC_TRANSFORM_FLIP_H_ROT_90: return ORIGIN;
    case HWC_TRANSFORM_FLIP_V_ROT_90: return ORIGIN;
    default: return ORIGIN;
    }
}

int rotateValueHAL2PP(unsigned char transform)
{
    switch (transform) {
    case HWC_TRANSFORM_ROT_90:  return 90;
    case HWC_TRANSFORM_ROT_180: return 180;
    case HWC_TRANSFORM_ROT_270: return 270;
    default: return 0;
    }
}

int multipleOf2(int number)
{
    if(number % 2 == 1)
        return (number - 1);
    else
        return number;
}
