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

#ifndef ANDROID_HWCOMPOSER_H
#define ANDROID_HWCOMPOSER_H

//#define LOG_NDEBUG 0

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>
#include <utils/Vector.h>

#include <EGL/egl.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sync/sync.h>
#include <system/graphics.h>

#include "gralloc_priv.h"
#include "s3c_lcd.h"
#include "sec_utils.h"
#include "s5p_fimc.h"

#include "videodev2.h"

#include "window.h"
#include "utils.h"
#include "v4l2.h"
#include "FimgApi.h"

#ifndef _VSYNC_PERIOD
#define _VSYNC_PERIOD 1000000000UL
#endif

#ifndef HWC_TRANSFORM_FLIP_H_ROT_90
/* flip source image horizontally, the rotate 90 degrees clock-wise */
#define HWC_TRANSFORM_FLIP_H_ROT_90 (HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90)
#endif

#ifndef HWC_TRANSFORM_FLIP_V_ROT_90
/* flip source image vertically, the rotate 90 degrees clock-wise */
#define HWC_TRANSFORM_FLIP_V_ROT_90 (HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90)
#endif

const size_t WIN_FB0 = 2;
const size_t NUM_HW_WINDOWS = 5;
const size_t NO_FB_NEEDED = NUM_HW_WINDOWS + 1;
const size_t NUM_OF_WIN_BUF = 3;

#define DEBUG 1
#define DEBUG_SPAMMY 0
#define DEBUG_VSYNC 0

#ifdef ALOGD
#undef ALOGD
#endif
#define ALOGD(...) if (DEBUG) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))

#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

#define UNLIKELY( exp )     (__builtin_expect( (exp) != 0, false ))

inline int WIDTH(const hwc_rect &rect) { return rect.right - rect.left; }
inline int HEIGHT(const hwc_rect &rect) { return rect.bottom - rect.top; }

struct sec_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

struct gsc_map_t {
    enum mode {
        NONE = 0,
        FIMG,
        FIMC,
        MAX,
    } mode;
};

struct hwc_win_info_t {
    sec_rect                    rect_info;
    buffer_handle_t             dst_buf[NUM_OF_WIN_BUF];
    int                         dst_buf_fence[NUM_OF_WIN_BUF];
    size_t                      current_buf;

    struct gsc_map_t            gsc;
    struct fimg2d_blit          fimg_cmd; //if geometry, blending hasn't changed, only buffers have to be swapped
    struct fimg2d_image         g2d_src_img;
    struct fimg2d_image         g2d_dst_img;

    int                         blending;
    int                         layer_index;
};

struct hwc_context_t {
    hwc_composer_device_1_t   device;
    /* our private state goes below here */
    hwc_procs_t              *procs;

    struct private_module_t  *gralloc_module;
    alloc_device_t           *alloc_device;

    int                       fb0_fd;
    int                       fb0_size;

    bool                      force_gpu; //value coming from settings
    bool                      force_fb;
    int                       bypass_count;

    struct hwc_win_info_t     win[NUM_HW_WINDOWS];

    // V4L2 info of FIMC device
    int    v4l2_fd;

    int    xres;
    int    yres;
    float  xdpi;
    float  ydpi;

    // vsync info
    int       vsync_period;
    int       vsync_timestamp_fd;
    pthread_t vsync_thread;

    bool         fb_needed;
    size_t       first_fb;
    size_t       last_fb;
    size_t       fb_window;
};

#endif //ANDROID_HWCOMPOSER_H
