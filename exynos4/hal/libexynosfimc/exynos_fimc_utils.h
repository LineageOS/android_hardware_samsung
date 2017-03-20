/*
 * Copyright@ Samsung Electronics Co. LTD
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

#ifndef EXYNOS_FIMC_H_
#define EXYNOS_FIMC_H_

#ifdef __cplusplus
extern "C" {
#endif

//#define LOG_NDEBUG 0
#define LOG_TAG "libexynosfimc"
#include <cutils/log.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <system/graphics.h>
#include "exynos_fimc.h"

#include "exynos_format.h"
#include "ExynosMutex.h"
#include "exynos_v4l2.h"

//#include "ExynosBuffer.h"

#define NUM_OF_FIMC_PLANES           (3)
#define MAX_BUFFERS_FIMC_OUT (3)
#define FIMC_SUBDEV_PAD_SINK     (0)
#define FIMC_SUBDEV_PAD_SOURCE   (1)
#define MIXER_V_SUBDEV_PAD_SINK     (0)
#define MIXER_V_SUBDEV_PAD_SOURCE   (3)
#define FIMD_SUBDEV_PAD_SINK     (0)
#define MAX_BUFFERS                 (6)

#define NUM_OF_FIMC_HW               (4)
#define NODE_NUM_FIMC_0              (0)
#define NODE_NUM_FIMC_1              (2)
#define NODE_NUM_FIMC_2              (4)
#define NODE_NUM_FIMC_3              (6)

#define PFX_NODE_FIMC                "/dev/video"
#define PFX_NODE_MEDIADEV         "/dev/media"
#define PFX_MXR_ENTITY              "s5p-mixer%d"
#define PFX_FIMD_ENTITY             "s3c-fb-window%d"
#define PFX_FIMC_VIDEODEV_ENTITY   "exynos-fimc.%d.output"
#define PFX_FIMC_SUBDEV_ENTITY     "exynos-fimc-sd.%d"
#define PFX_SUB_DEV		"/dev/v4l-subdev%d"
#define FIMC_VD_PAD_SOURCE	0
#define FIMC_SD_PAD_SINK	0
#define FIMC_SD_PAD_SOURCE	1
#define FIMC_OUT_PAD_SINK	0
//#define FIMC_OUT_DMA_BLOCKING
//#define FIMC_OUT_DELAYED_STREAMON

#define FIMC_VERSION FIMC_EVT1

#if (FIMC_VERSION == FIMC_EVT0)
#define FIMC_MIN_W_SIZE (64)
#define FIMC_MIN_H_SIZE (32)
#else
#define FIMC_MIN_W_SIZE (32)
#define FIMC_MIN_H_SIZE (8)
#endif

#define MAX_FIMC_WAITING_TIME_FOR_TRYLOCK (16000) // 16msec
#define FIMC_WAITING_TIME_FOR_TRYLOCK      (8000) //  8msec

struct fimc_info {
    unsigned int       width;
    unsigned int       height;
    unsigned int       crop_left;
    unsigned int       crop_top;
    unsigned int       crop_width;
    unsigned int       crop_height;
    unsigned int       v4l2_colorformat;
    unsigned int       cacheable;
    unsigned int       mode_drm;

    int                rotation;
    int                flip_horizontal;
    int                flip_vertical;
    bool               csc_range;
    bool               dirty;

    void              *addr[NUM_OF_FIMC_PLANES];
    int                acquireFenceFd;
    int                releaseFenceFd;
    bool               stream_on;

    enum v4l2_buf_type buf_type;
    struct v4l2_format format;
    struct v4l2_buffer buffer;
    bool               buffer_queued;
    struct v4l2_plane  planes[NUM_OF_FIMC_PLANES];
    struct v4l2_crop   crop;
    int             src_buf_idx;
    int             qbuf_cnt;
};

struct FIMC_HANDLE {
    int              fimc_fd;
    int              fimc_id;
    struct fimc_info  src;
    struct fimc_info  dst;
    exynos_fimc_img   src_img;
    exynos_fimc_img   dst_img;
    void            *op_mutex;
    void            *obj_mutex[NUM_OF_FIMC_HW];
    void            *cur_obj_mutex;
    bool             flag_local_path;
    bool             flag_exclusive_open;
    struct media_device *media0;
    struct media_entity *fimc_sd_entity;
    struct media_entity *fimc_vd_entity;
    struct media_entity *sink_sd_entity;
    int     fimc_mode;
    int     out_mode;
    bool    allow_drm;
    bool    protection_enabled;
};

extern int exynos_fimc_out_stop(void *handle);
#ifdef __cplusplus
}
#endif

#endif //__EXYNOS_MUTEX_H__
