/*
 * Copyright (C) 2008 The Android Open Source Project
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

/*!
 * \file      exynos_fimc.c
 * \brief     header file for FIMC HAL
 * \author    ShinWon Lee (shinwon.lee@samsung.com)
 * \date      2012/01/09
 *
 * <b>Revision History: </b>
 * - 2012.01.09 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Create
 *
 * - 2012.02.07 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Change file name to exynos_fimc.h
 *
 * - 2012.02.09 : Sangwoo, Parkk(sw5771.park@samsung.com) \n
 *   Use Multiple FIMC by Multiple Process
 *
 * - 2012.02.20 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Add exynos_fimc_set_rotation() API
 *
 * - 2012.02.20 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Add size constrain
 *
 */

//#define LOG_NDEBUG 0
#include "exynos_fimc_utils.h"
//#include "content_protect.h"
#include "exynos_format.h"
#include "s5p_fimc_v4l2.h"
#include "graphics.h"

static int exynos_fimc_m2m_wait_frame_done(void *handle);
static int exynos_fimc_m2m_stop(void *handle);

int HAL_PIXEL_FORMAT_2_V4L2_PIX(
    int hal_pixel_format)
{
    int v4l2_pixel_format = -1;

    switch (hal_pixel_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB32;
        break;

    case HAL_PIXEL_FORMAT_RGB_888:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB24;
        break;

    case HAL_PIXEL_FORMAT_RGB_565:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB565;
        break;

    case HAL_PIXEL_FORMAT_BGRA_8888:
        v4l2_pixel_format = V4L2_PIX_FMT_BGR32;
        break;

    case HAL_PIXEL_FORMAT_RGBA_5551:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB555X;
        break;
 case HAL_PIXEL_FORMAT_RGBA_4444:
        v4l2_pixel_format = V4L2_PIX_FMT_RGB444;
        break;

    case HAL_PIXEL_FORMAT_EXYNOS_YV12:
        v4l2_pixel_format = V4L2_PIX_FMT_YVU420M;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_P:
        v4l2_pixel_format = V4L2_PIX_FMT_YUV420M;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV61;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV12M;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_YUYV;
        break;

    case HAL_PIXEL_FORMAT_YCbCr_422_P:
        v4l2_pixel_format = V4L2_PIX_FMT_YUV422P;
        break;

    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_UYVY;
        break;

    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV16;
        break;

    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        v4l2_pixel_format = V4L2_PIX_FMT_NV21M;
        break;

case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        v4l2_pixel_format = V4L2_PIX_FMT_NV12MT_16X16;
        break;

    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        v4l2_pixel_format = V4L2_PIX_FMT_NV12MT_16X16;
        break;

   case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_YVYU;
        break;

   case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        v4l2_pixel_format = V4L2_PIX_FMT_VYUY;
        break;

    default:
        ALOGE("%s::unmatched HAL_PIXEL_FORMAT color_space(0x%x)\n",
                __func__, hal_pixel_format);
        break;
    }

    return v4l2_pixel_format;
}


int V4L2_PIX_2_YUV_INFO(unsigned int v4l2_pixel_format, unsigned int * bpp, unsigned int * planes)
{
    switch (v4l2_pixel_format) {
    case V4L2_PIX_FMT_NV12:
        *bpp    = 12;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21X:
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV21M:
    case V4L2_PIX_FMT_NV12MT_16X16:
        *bpp    = 12;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420M:
        *bpp    = 12;
        *planes = 3;
        break;
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
        *bpp    = 16;
        *planes = 1;
        break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
    case V4L2_PIX_FMT_NV16X:
    case V4L2_PIX_FMT_NV61X:
        *bpp    = 16;
        *planes = 2;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *bpp    = 16;
        *planes = 3;
        break;
    default:
        return -1;
        break;
    }

    return 0;
}

unsigned int get_yuv_bpp(unsigned int v4l2_pixel_format)
{
    unsigned int bpp, planes;

    if (V4L2_PIX_2_YUV_INFO(v4l2_pixel_format, &bpp, &planes) < 0)
        bpp = -1;

    return bpp;
}

unsigned int get_yuv_planes(unsigned int v4l2_pixel_format)
{
    unsigned int bpp, planes;

    if (V4L2_PIX_2_YUV_INFO(v4l2_pixel_format, &bpp, &planes) < 0)
        planes = -1;

    return planes;
}

static unsigned int m_fimc_get_plane_count(
    int v4l_pixel_format)
{
    int plane_count = 0;

    switch (v4l_pixel_format) {
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
        plane_count = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT_16X16:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
        plane_count = 2;
        break;
    case V4L2_PIX_FMT_YVU420M:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_YUV420M:
        plane_count = 3;
        break;
    default:
        ALOGE("%s::unmatched v4l_pixel_format color_space(0x%x)\n",
             __func__, v4l_pixel_format);
        plane_count = -1;
        break;
    }

    return plane_count;
}

static unsigned int m_fimc_get_plane_size(
    unsigned int *plane_size,
    unsigned int  width,
    unsigned int  height,
    int           v4l_pixel_format)
{
    switch (v4l_pixel_format) {
    /* 1 plane */
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
        plane_size[0] = width * height * 4;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_RGB24:
        plane_size[0] = width * height * 3;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
        plane_size[0] = width * height * 2;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    /* 2 planes */
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
        plane_size[0] = width * height;
        plane_size[1] = width * (height / 2);
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
        plane_size[0] = width * height * 2;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_NV12MT_16X16:
        plane_size[0] = ALIGN(width, 16) * ALIGN(height, 16);
        plane_size[1] = ALIGN(width, 16) * ALIGN(height / 2, 8);
        plane_size[2] = 0;
        break;
    /* 3 planes */
    case V4L2_PIX_FMT_YVU420M:
    case V4L2_PIX_FMT_YUV422P:
        plane_size[0] = width * height;
        plane_size[1] = (width / 2) * (height / 2);
        plane_size[2] = (width / 2) * (height / 2);
        break;
    default:
        ALOGE("%s::unmatched v4l_pixel_format color_space(0x%x)\n",
             __func__, v4l_pixel_format);
        return -1;
        break;
    }

    return 0;
}

static int m_exynos_fimc_multiple_of_n(
    int number, int N)
{
    int result = number;
    switch (N) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 32:
    case 64:
    case 128:
    case 256:
        result = (number - (number & (N-1)));
        break;
    default:
        result = number - (number % N);
        break;
    }
    return result;
}

static bool m_exynos_fimc_check_src_size(
    unsigned int *w,      unsigned int *h,
    unsigned int *crop_x, unsigned int *crop_y,
    unsigned int *crop_w, unsigned int *crop_h,
    int v4l2_colorformat)
{
    if (*w < FIMC_MIN_W_SIZE || *h < FIMC_MIN_H_SIZE) {
        ALOGE("%s::too small size (w : %d < %d) (h : %d < %d)",
            __func__, FIMC_MIN_W_SIZE, *w, FIMC_MIN_H_SIZE, *h);
        return false;
    }

    if (*crop_w < FIMC_MIN_W_SIZE || *crop_h < FIMC_MIN_H_SIZE) {
        ALOGE("%s::too small size (w : %d < %d) (h : %d < %d)",
            __func__, FIMC_MIN_W_SIZE,* crop_w, FIMC_MIN_H_SIZE, *crop_h);
        return false;
    }

    switch (v4l2_colorformat) {
    // YUV420
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420M:
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
        *w = (*w + 15) & ~15;
        *h = (*h + 15) & ~15;
        //*w      = m_exynos_fimc_multiple_of_n(*w, 16);
        //*h      = m_exynos_fimc_multiple_of_n(*h, 16);
        *crop_w = m_exynos_fimc_multiple_of_n(*crop_w, 4);
        *crop_h = m_exynos_fimc_multiple_of_n(*crop_h, 4);
        break;
    // YUV422
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_VYUY:
        *h = (*h + 7) & ~7;
        //*h      = m_exynos_fimc_multiple_of_n(*h, 8);
        *crop_w = m_exynos_fimc_multiple_of_n(*crop_w, 4);
        *crop_h = m_exynos_fimc_multiple_of_n(*crop_h, 2);
        break;
    // RGB
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    default:
        *h = (*h + 7) & ~7;
        //*h      = m_exynos_fimc_multiple_of_n(*h, 8);
        *crop_w = m_exynos_fimc_multiple_of_n(*crop_w, 2);
        *crop_h = m_exynos_fimc_multiple_of_n(*crop_h, 2);
        break;
    }

    return true;
}

static bool m_exynos_fimc_check_dst_size(
    unsigned int *w,      unsigned int *h,
    unsigned int *crop_x, unsigned int *crop_y,
    unsigned int *crop_w, unsigned int *crop_h,
    int v4l2_colorformat,
    int rotation)
{
    unsigned int *new_w;
    unsigned int *new_h;
    unsigned int *new_crop_w;
    unsigned int *new_crop_h;

        new_w = w;
        new_h = h;
        new_crop_w = crop_w;
        new_crop_h = crop_h;

    if (*w < FIMC_MIN_W_SIZE || *h < FIMC_MIN_H_SIZE) {
        ALOGE("%s::too small size (w : %d < %d) (h : %d < %d)",
            __func__, FIMC_MIN_W_SIZE, *w, FIMC_MIN_H_SIZE, *h);
        return false;
    }

    if (*crop_w < FIMC_MIN_W_SIZE || *crop_h < FIMC_MIN_H_SIZE) {
        ALOGE("%s::too small size (w : %d < %d) (h : %d < %d)",
            __func__, FIMC_MIN_W_SIZE,* crop_w, FIMC_MIN_H_SIZE, *crop_h);
        return false;
    }

    switch (v4l2_colorformat) {
    // YUV420
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV21M:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YVU420M:
        *new_w = m_exynos_fimc_multiple_of_n(*new_w, 2);
        *new_h = m_exynos_fimc_multiple_of_n(*new_h, 2);
        break;
    // YUV422
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_VYUY:
        *new_w = m_exynos_fimc_multiple_of_n(*new_w, 2);
        break;
    // RGB
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    default:
        break;
    }

    return true;
}

static int m_exynos_fimc_output_create(
    struct FIMC_HANDLE *fimc_handle,
    int dev_num,
    int out_mode)
{
    struct media_device *media0;
    struct media_entity *fimc_sd_entity;
    struct media_entity *fimc_vd_entity;
    struct media_entity *sink_sd_entity;
    struct media_link *links;
    char node[32];
    char devname[32];
    unsigned int cap;
    int         i;
    int         fd = 0;

    Exynos_fimc_In();

    if ((out_mode != FIMC_OUT_FIMD) &&
        (out_mode != FIMC_OUT_TV))
        return -1;

    fimc_handle->out_mode = out_mode;
    /* FIMCX => FIMD_WINX : arbitrary linking is not allowed */
    if ((out_mode == FIMC_OUT_FIMD) &&
        (dev_num > 2))
        return -1;

    /* media0 */
    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    media0 = exynos_media_open(node);
    if (media0 == NULL) {
        ALOGE("%s::exynos_media_open failed (node=%s)", __func__, node);
        return false;
    }

    /* Get the sink subdev entity by name and make the node of sink subdev*/
    if (out_mode == FIMC_OUT_FIMD)
        sprintf(devname, PFX_FIMD_ENTITY, dev_num);
    else
        sprintf(devname, PFX_MXR_ENTITY, 0);

    sink_sd_entity = exynos_media_get_entity_by_name(media0, devname, strlen(devname));
    sink_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if ( sink_sd_entity->fd < 0) {
        ALOGE("%s:: failed to open sink subdev node", __func__);
        goto fimc_output_err;
    }

    /* get FIMC video dev & sub dev entity by name*/
    sprintf(devname, PFX_FIMC_VIDEODEV_ENTITY, dev_num);
    fimc_vd_entity= exynos_media_get_entity_by_name(media0, devname, strlen(devname));

    sprintf(devname, PFX_FIMC_SUBDEV_ENTITY, dev_num);
    fimc_sd_entity= exynos_media_get_entity_by_name(media0, devname, strlen(devname));

    /* fimc sub-dev open */
    sprintf(devname, PFX_FIMC_SUBDEV_ENTITY, dev_num);
    fimc_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);

    /* setup link : FIMC : video device --> sub device */
    for (i = 0; i < (int) fimc_vd_entity->num_links; i++) {
        links = &fimc_vd_entity->links[i];

        if (links == NULL ||
            links->source->entity != fimc_vd_entity ||
            links->sink->entity   != fimc_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media0,  links->source,  links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* setup link : FIMC: sub device --> sink device */
    for (i = 0; i < (int) fimc_sd_entity->num_links; i++) {
        links = &fimc_sd_entity->links[i];

        if (links == NULL || links->source->entity != fimc_sd_entity ||
                             links->sink->entity   != sink_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media0,  links->source,  links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* fimc video-dev open */
    sprintf(devname, PFX_FIMC_VIDEODEV_ENTITY, dev_num);
    fimc_vd_entity->fd = exynos_v4l2_open_devname(devname, O_RDWR);
    cap = V4L2_CAP_STREAMING |
          V4L2_CAP_VIDEO_OUTPUT_MPLANE;

    if (exynos_v4l2_querycap(fimc_vd_entity->fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        goto fimc_output_err;
    }
    fimc_handle->fimc_sd_entity = fimc_sd_entity;
    fimc_handle->fimc_vd_entity = fimc_vd_entity;
    fimc_handle->sink_sd_entity = sink_sd_entity;
    fimc_handle->media0 = media0;

    Exynos_fimc_Out();

    return 0;

fimc_output_err:
    /* to do */
    return -1;

}

static int m_exynos_fimc_m2m_create(
    int dev)
{
    int          fd = 0;
    int          video_node_num;
    unsigned int cap = 0;
    char         node[32];

    Exynos_fimc_In();

    switch(dev) {
    case 0:
        video_node_num = NODE_NUM_FIMC_0;
	cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE;
        break;
    case 1:
        video_node_num = NODE_NUM_FIMC_1;
	cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE;
        break;
    case 2:
        video_node_num = NODE_NUM_FIMC_2;
        break;
    case 3:
        video_node_num = NODE_NUM_FIMC_3;
        break;
    default:
        ALOGE("%s::unexpected dev(%d) fail", __func__, dev);
        return -1;
        break;
    }

    sprintf(node, "%s%d", PFX_NODE_FIMC, video_node_num);
    fd = exynos_v4l2_open(node, O_RDWR);
    if (fd < 0) {
        ALOGE("%s::exynos_v4l2_open(%s) fail", __func__, node);
        return -1;
    }

    cap = V4L2_CAP_STREAMING |
          V4L2_CAP_VIDEO_OUTPUT_MPLANE |
          cap;

    if (exynos_v4l2_querycap(fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        if (0 < fd)
            close(fd);
        fd = 0;
        return -1;
    }

    Exynos_fimc_Out();

    return fd;
}


static bool m_exynos_fimc_out_destroy(struct FIMC_HANDLE *fimc_handle)
{
    struct media_link * links;
    int i;

    Exynos_fimc_In();

    if (fimc_handle == NULL) {
        ALOGE("%s::fimc_handle is NULL", __func__);
        return false;
    }

    if (fimc_handle->src.stream_on == true) {
        if (exynos_fimc_out_stop((void *)fimc_handle) < 0)
            ALOGE("%s::exynos_fimc_out_stop() fail", __func__);

            fimc_handle->src.stream_on = false;
    }

    /* unlink : fimc-out --> fimd */
        for (i = 0; i < (int) fimc_handle->fimc_sd_entity->num_links; i++) {
            links = &fimc_handle->fimc_sd_entity->links[i];

            if (links == NULL || links->source->entity != fimc_handle->fimc_sd_entity ||
                                 links->sink->entity   != fimc_handle->sink_sd_entity) {
                continue;
            } else if (exynos_media_setup_link(fimc_handle->media0,  links->source,
                                                                        links->sink, 0) < 0) {
                ALOGE("%s::exynos_media_setup_unlink [src.entity=%d->sink.entity=%d] failed",
                      __func__, links->source->entity->info.id, links->sink->entity->info.id);
            }
        }

        close(fimc_handle->fimc_vd_entity->fd);
        close(fimc_handle->fimc_sd_entity->fd);
        fimc_handle->fimc_vd_entity->fd = -1;
        fimc_handle->fimc_vd_entity->fd = -1;

        Exynos_fimc_Out();

        return true;

}

static bool m_exynos_fimc_destroy(
    struct FIMC_HANDLE *fimc_handle)
{
    Exynos_fimc_In();

    /* just in case, we call stop here because we cannot afford to leave
     * secure side protection on if things failed.
     */
    exynos_fimc_m2m_stop(fimc_handle);

    if (0 < fimc_handle->fimc_fd)
        close(fimc_handle->fimc_fd);
    fimc_handle->fimc_fd = 0;

    Exynos_fimc_Out();

    return true;
}

bool m_exynos_fimc_find_and_trylock_and_create(
    struct FIMC_HANDLE *fimc_handle)
{
    int          i                 = 0;
    bool         flag_find_new_fimc = false;
    unsigned int total_sleep_time  = 0;

    Exynos_fimc_In();

    do {
        for (i = 0; i < NUM_OF_FIMC_HW; i++) {
            // HACK : HWComposer, HDMI uses fimc with their own code.
            //        So, This obj_mutex cannot defense their open()
            if (i == 0 || i == 3)
                continue;

            if (exynos_mutex_trylock(fimc_handle->obj_mutex[i]) == true) {

                // destroy old one.
                m_exynos_fimc_destroy(fimc_handle);

                // create new one.
                fimc_handle->fimc_id = i;
                fimc_handle->fimc_fd = m_exynos_fimc_m2m_create(i);
                if (fimc_handle->fimc_fd < 0) {
                    fimc_handle->fimc_fd = 0;
                    exynos_mutex_unlock(fimc_handle->obj_mutex[i]);
                    continue;
                }

                if (fimc_handle->cur_obj_mutex)
                    exynos_mutex_unlock(fimc_handle->cur_obj_mutex);

                fimc_handle->cur_obj_mutex = fimc_handle->obj_mutex[i];

                flag_find_new_fimc = true;
                break;
            }
        }

        // waiting for another process doesn't use fimc.
        // we need to make decision how to do.
        if (flag_find_new_fimc == false) {
            usleep(FIMC_WAITING_TIME_FOR_TRYLOCK);
            total_sleep_time += FIMC_WAITING_TIME_FOR_TRYLOCK;
            ALOGV("%s::waiting for anthere process doens't use fimc", __func__);
        }

    } while(   flag_find_new_fimc == false
            && total_sleep_time < MAX_FIMC_WAITING_TIME_FOR_TRYLOCK);

    if (flag_find_new_fimc == false)
        ALOGE("%s::we don't have no available fimc.. fail", __func__);

    Exynos_fimc_Out();

    return flag_find_new_fimc;
}

static bool m_exynos_fimc_set_format(
    int              fd,
    struct fimc_info *info)
{
    Exynos_fimc_In();

    struct v4l2_requestbuffers req_buf;
    int                        plane_count;

    plane_count = m_fimc_get_plane_count(info->v4l2_colorformat);
    if (plane_count < 0) {
        ALOGE("%s::not supported v4l2_colorformat", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_ROTATE, info->rotation) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_ROTATE) fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_VFLIP, info->flip_horizontal) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_VFLIP) fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_HFLIP, info->flip_vertical) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_HFLIP) fail", __func__);
        return false;
    }
#if 0
    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_CSC_RANGE, info->csc_range) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_CSC_RANGE) fail", __func__);
        return false;
    }
#endif
    info->format.type = info->buf_type;
    info->format.fmt.pix_mp.width       = info->width;
    info->format.fmt.pix_mp.height      = info->height;
    info->format.fmt.pix_mp.pixelformat = info->v4l2_colorformat;
    info->format.fmt.pix_mp.field       = V4L2_FIELD_ANY;
    info->format.fmt.pix_mp.num_planes  = plane_count;

    if (exynos_v4l2_s_fmt(fd, &info->format) < 0) {
        ALOGE("%s::exynos_v4l2_s_fmt() fail", __func__);
        return false;
    }

    info->crop.type     = info->buf_type;
    info->crop.c.left   = info->crop_left;
    info->crop.c.top    = info->crop_top;
    info->crop.c.width  = info->crop_width;
    info->crop.c.height = info->crop_height;

    if (exynos_v4l2_s_crop(fd, &info->crop) < 0) {
        ALOGE("%s::exynos_v4l2_s_crop() fail", __func__);
        return false;
    }
#if 0
    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_CACHEABLE, info->cacheable) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl() fail", __func__);
        return false;
    }
#endif
    req_buf.count  = 1;
    req_buf.type   = info->buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs() fail", __func__);
        return false;
    }

    Exynos_fimc_Out();

    return true;
}

static bool m_exynos_fimc_set_addr(
    int              fd,
    struct fimc_info *info)
{
    unsigned int i;
    unsigned int plane_size[NUM_OF_FIMC_PLANES];

    m_fimc_get_plane_size(plane_size,
                         info->width,
                         info->height,
                         info->v4l2_colorformat);

    info->buffer.index    = 0;
    info->buffer.flags    = V4L2_BUF_FLAG_USE_SYNC;
    info->buffer.type     = info->buf_type;
    info->buffer.memory   = V4L2_MEMORY_USERPTR;
    info->buffer.m.planes = info->planes;
    info->buffer.length   = info->format.fmt.pix_mp.num_planes;
    info->buffer.reserved = info->acquireFenceFd;

    for (i = 0; i < info->format.fmt.pix_mp.num_planes; i++) {
        info->buffer.m.planes[i].m.userptr = (int)info->addr[i];
        info->buffer.m.planes[i].length    = plane_size[i];
        info->buffer.m.planes[i].bytesused = 0;
    }

    if (exynos_v4l2_qbuf(fd, &info->buffer) < 0) {
        ALOGE("%s::exynos_v4l2_qbuf() fail", __func__);
        return false;
    }
    info->buffer_queued = true;

    info->releaseFenceFd = info->buffer.reserved;

    return true;
}

void *exynos_fimc_create(
    void)
{
    int i     = 0;
    int op_id = 0;
    char mutex_name[32];

    Exynos_fimc_In();

    struct FIMC_HANDLE *fimc_handle = (struct FIMC_HANDLE *)malloc(sizeof(struct FIMC_HANDLE));
    if (fimc_handle == NULL) {
        ALOGE("%s::malloc(struct FIMC_HANDLE) fail", __func__);
        goto err;
    }

    fimc_handle->fimc_fd = 0;
    memset(&fimc_handle->src, 0, sizeof(struct fimc_info));
    memset(&fimc_handle->dst, 0, sizeof(struct fimc_info));

    fimc_handle->src.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fimc_handle->dst.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    fimc_handle->op_mutex = NULL;
    for (i = 0; i < NUM_OF_FIMC_HW; i++)
        fimc_handle->obj_mutex[i] = NULL;

    fimc_handle->cur_obj_mutex = NULL;
    fimc_handle->flag_local_path = false;
    fimc_handle->flag_exclusive_open = false;

    srand(time(NULL));
    op_id = rand() % 1000000; // just make random id
    sprintf(mutex_name, "%sOp%d", LOG_TAG, op_id);
    fimc_handle->op_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_PRIVATE, mutex_name);
    if (fimc_handle->op_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    // check if it is available
    for (i = 0; i < NUM_OF_FIMC_HW; i++) {
        sprintf(mutex_name, "%sObject%d", LOG_TAG, i);

        fimc_handle->obj_mutex[i] = exynos_mutex_create(EXYNOS_MUTEX_TYPE_SHARED, mutex_name);
        if (fimc_handle->obj_mutex[i] == NULL) {
            ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
            goto err;
        }
    }

    if (m_exynos_fimc_find_and_trylock_and_create(fimc_handle) == false) {
        ALOGE("%s::m_exynos_fimc_find_and_trylock_and_create() fail", __func__);
        goto err;
    }

    exynos_mutex_unlock(fimc_handle->cur_obj_mutex);
    exynos_mutex_unlock(fimc_handle->op_mutex);

    return (void *)fimc_handle;

err:
    if (fimc_handle) {
        m_exynos_fimc_destroy(fimc_handle);

        if (fimc_handle->cur_obj_mutex)
            exynos_mutex_unlock(fimc_handle->cur_obj_mutex);

        for (i = 0; i < NUM_OF_FIMC_HW; i++) {
            if ((fimc_handle->obj_mutex[i] != NULL) &&
                (exynos_mutex_get_created_status(fimc_handle->obj_mutex[i]) == true)) {
                if (exynos_mutex_destroy(fimc_handle->obj_mutex[i]) == false)
                    ALOGE("%s::exynos_mutex_destroy() fail", __func__);
            }
        }

        if (fimc_handle->op_mutex)
            exynos_mutex_unlock(fimc_handle->op_mutex);

        if (exynos_mutex_destroy(fimc_handle->op_mutex) == false)
            ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

        free(fimc_handle);
    }

    Exynos_fimc_Out();

    return NULL;
}

void *exynos_fimc_reserve(int dev_num)
{
    char mutex_name[32];
    unsigned int total_sleep_time  = 0;
    bool    fimc_flag = false;

    if ((dev_num < 0) || (dev_num >= NUM_OF_FIMC_HW)) {
        ALOGE("%s::fail:: dev_num is not valid(%d) ", __func__, dev_num);
        return NULL;
    }

    struct FIMC_HANDLE *fimc_handle = (struct FIMC_HANDLE *)malloc(sizeof(struct FIMC_HANDLE));
    if (fimc_handle == NULL) {
        ALOGE("%s::malloc(struct FIMC_HANDLE) fail", __func__);
        goto err;
    }

    fimc_handle->fimc_fd = -1;
    fimc_handle->op_mutex = NULL;
    fimc_handle->cur_obj_mutex = NULL;

    sprintf(mutex_name, "%sObject%d", LOG_TAG, dev_num);
    fimc_handle->cur_obj_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_SHARED, mutex_name);
    if (fimc_handle->cur_obj_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    do {
        if (exynos_mutex_trylock(fimc_handle->cur_obj_mutex) == true) {
            fimc_flag = true;
            break;
        }
        usleep(FIMC_WAITING_TIME_FOR_TRYLOCK);
        total_sleep_time += FIMC_WAITING_TIME_FOR_TRYLOCK;
        ALOGV("%s::waiting for another process to release the requested fimc", __func__);
    } while(total_sleep_time < MAX_FIMC_WAITING_TIME_FOR_TRYLOCK);

    if (fimc_flag == true)
         return (void *)fimc_handle;

err:
    if (fimc_handle) {
        free(fimc_handle);
    }

    return NULL;
}

void exynos_fimc_release(void *handle)
{
    struct FIMC_HANDLE *fimc_handle = (struct FIMC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return;
    }

    exynos_mutex_unlock(fimc_handle->cur_obj_mutex);
    exynos_mutex_destroy(fimc_handle->cur_obj_mutex);
    free(fimc_handle);
    return;
}

void *exynos_fimc_create_exclusive(
    int dev_num,
    int mode,
    int out_mode,
    int allow_drm)
{
    int i     = 0;
    int op_id = 0;
    char mutex_name[32];
    unsigned int total_sleep_time  = 0;
    bool    fimc_flag = false;
    int ret = 0;

    Exynos_fimc_In();

    if ((dev_num < 0) || (dev_num >= NUM_OF_FIMC_HW)) {
        ALOGE("%s::fail:: dev_num is not valid(%d) ", __func__, dev_num);
        return NULL;
    }

    if ((mode < 0) || (mode >= NUM_OF_FIMC_HW)) {
        ALOGE("%s::fail:: mode is not valid(%d) ", __func__, mode);
        return NULL;
    }

    /* currently only fimcs 0 and 3 are DRM capable */
    if (allow_drm && (dev_num != 0 && dev_num != 3)) {
        ALOGE("%s::fail:: fimc %d does not support drm\n", __func__,
              dev_num);
        return NULL;
    }

    struct FIMC_HANDLE *fimc_handle = (struct FIMC_HANDLE *)malloc(sizeof(struct FIMC_HANDLE));
    if (fimc_handle == NULL) {
        ALOGE("%s::malloc(struct FIMC_HANDLE) fail", __func__);
        goto err;
    }
    memset(fimc_handle, 0, sizeof(struct FIMC_HANDLE));
    fimc_handle->fimc_fd = -1;
    fimc_handle->fimc_mode = mode;
    fimc_handle->fimc_id = dev_num;
    fimc_handle->allow_drm = allow_drm;

    fimc_handle->src.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fimc_handle->dst.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    fimc_handle->op_mutex = NULL;
    for (i = 0; i < NUM_OF_FIMC_HW; i++)
        fimc_handle->obj_mutex[i] = NULL;

    fimc_handle->cur_obj_mutex = NULL;
    fimc_handle->flag_local_path = false;
    fimc_handle->flag_exclusive_open = true;

    srand(time(NULL));
    op_id = rand() % 1000000; // just make random id
    sprintf(mutex_name, "%sOp%d", LOG_TAG, op_id);
    fimc_handle->op_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_PRIVATE, mutex_name);
    if (fimc_handle->op_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    sprintf(mutex_name, "%sObject%d", LOG_TAG, dev_num);
    fimc_handle->cur_obj_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_SHARED, mutex_name);
    if (fimc_handle->cur_obj_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    do {
        if (exynos_mutex_trylock(fimc_handle->cur_obj_mutex) == true) {
            if (mode == FIMC_M2M_MODE) {
                fimc_handle->fimc_fd = m_exynos_fimc_m2m_create(dev_num);
                if (fimc_handle->fimc_fd < 0) {
                    ALOGE("%s::m_exynos_fimc_m2m_create(%i) fail", __func__, dev_num);
                    goto err;
                }
            } else if (mode == FIMC_OUTPUT_MODE) {
                ret = m_exynos_fimc_output_create(fimc_handle, dev_num, out_mode);
                if (ret < 0) {
                    ALOGE("%s::m_exynos_fimc_output_create(%i) fail", __func__, dev_num);
                    goto err;
                }
            }
            /*else
                fimc_handle->fimc_fd = m_exynos_fimc_capture_create(dev_num);*/

            fimc_flag = true;
            break;
        }
        usleep(FIMC_WAITING_TIME_FOR_TRYLOCK);
        total_sleep_time += FIMC_WAITING_TIME_FOR_TRYLOCK;
        ALOGV("%s::waiting for another process doesn't use fimc", __func__);
    } while(total_sleep_time < MAX_FIMC_WAITING_TIME_FOR_TRYLOCK);

    exynos_mutex_unlock(fimc_handle->op_mutex);
    if (fimc_flag == true) {
        Exynos_fimc_Out();
        return (void *)fimc_handle;
        }

err:
    if (fimc_handle) {
        m_exynos_fimc_destroy(fimc_handle);

        if (fimc_handle->cur_obj_mutex)
            exynos_mutex_unlock(fimc_handle->cur_obj_mutex);

        for (i = 0; i < NUM_OF_FIMC_HW; i++) {
            if ((fimc_handle->obj_mutex[i] != NULL) &&
                (exynos_mutex_get_created_status(fimc_handle->obj_mutex[i]) == true)) {
                if (exynos_mutex_destroy(fimc_handle->obj_mutex[i]) == false)
                    ALOGE("%s::exynos_mutex_destroy() fail", __func__);
            }
        }

        if (fimc_handle->op_mutex)
            exynos_mutex_unlock(fimc_handle->op_mutex);

        if (exynos_mutex_destroy(fimc_handle->op_mutex) == false)
            ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

        free(fimc_handle);
    }

    Exynos_fimc_Out();

    return NULL;
}

void exynos_fimc_destroy(
    void *handle)
{
    int i = 0;
    struct FIMC_HANDLE *fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    if (fimc_handle->flag_exclusive_open == false)
        exynos_mutex_lock(fimc_handle->cur_obj_mutex);

    if (fimc_handle->fimc_mode == FIMC_OUTPUT_MODE)
        m_exynos_fimc_out_destroy(fimc_handle);
    else
        m_exynos_fimc_destroy(fimc_handle);

    exynos_mutex_unlock(fimc_handle->cur_obj_mutex);

    for (i = 0; i < NUM_OF_FIMC_HW; i++) {
        if ((fimc_handle->obj_mutex[i] != NULL) &&
            (exynos_mutex_get_created_status(fimc_handle->obj_mutex[i]) == true)) {
            if (exynos_mutex_destroy(fimc_handle->obj_mutex[i]) == false)
                ALOGE("%s::exynos_mutex_destroy(obj_mutex) fail", __func__);
        }
    }

    exynos_mutex_unlock(fimc_handle->op_mutex);

    if (exynos_mutex_destroy(fimc_handle->op_mutex) == false)
        ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

    if (fimc_handle)
        free(fimc_handle);

    Exynos_fimc_Out();

}

int exynos_fimc_set_src_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm)
{
    Exynos_fimc_In();

    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->src.width            = width;
    fimc_handle->src.height           = height;
    fimc_handle->src.crop_left        = crop_left;
    fimc_handle->src.crop_top         = crop_top;
    fimc_handle->src.crop_width       = crop_width;
    fimc_handle->src.crop_height      = crop_height;
    fimc_handle->src.v4l2_colorformat = v4l2_colorformat;
    fimc_handle->src.cacheable        = cacheable;
    fimc_handle->src.mode_drm         = mode_drm;
    fimc_handle->src.dirty            = true;


    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();

    return 0;
}

int exynos_fimc_set_dst_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm,
    unsigned int narrowRgb)
{
    Exynos_fimc_In();

    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->dst.width            = width;
    fimc_handle->dst.height           = height;
    fimc_handle->dst.crop_left        = crop_left;
    fimc_handle->dst.crop_top         = crop_top;
    fimc_handle->dst.crop_width       = crop_width;
    fimc_handle->dst.crop_height      = crop_height;
    fimc_handle->dst.v4l2_colorformat = v4l2_colorformat;
    fimc_handle->dst.cacheable        = cacheable;
    fimc_handle->dst.mode_drm         = mode_drm;
    fimc_handle->dst.dirty            = true;
    fimc_handle->dst.csc_range        = !narrowRgb;

    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();
    return 0;
}

int exynos_fimc_set_rotation(
    void *handle,
    int   rotation,
    int   flip_horizontal,
    int   flip_vertical)
{
    int ret = -1;
    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return ret;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    int new_rotation = rotation % 360;

    if (new_rotation % 90 != 0) {
        ALOGE("%s::rotation(%d) cannot be acceptable fail", __func__, rotation);
        goto done;
    }

    if(new_rotation < 0)
        new_rotation = -new_rotation;

    fimc_handle->dst.rotation        = new_rotation;
    fimc_handle->dst.flip_horizontal = flip_horizontal;
    fimc_handle->dst.flip_vertical   = flip_vertical;

    ret = 0;
done:
    exynos_mutex_unlock(fimc_handle->op_mutex);

    return ret;
}

int exynos_fimc_set_src_addr(
    void *handle,
    void *addr[3],
    int acquireFenceFd)
{
    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->src.addr[0] = addr[0];
    fimc_handle->src.addr[1] = addr[1];
    fimc_handle->src.addr[2] = addr[2];
    fimc_handle->src.acquireFenceFd = acquireFenceFd;

    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();

    return 0;
}

int exynos_fimc_set_dst_addr(
    void *handle,
    void *addr[3],
    int acquireFenceFd)
{
    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;
    int ret = 0;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->dst.addr[0] = addr[0];
    fimc_handle->dst.addr[1] = addr[1];
    fimc_handle->dst.addr[2] = addr[2];
    fimc_handle->dst.acquireFenceFd = acquireFenceFd;


    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();

    return ret;
}

static void rotateValueHAL2FIMC(unsigned int transform,
    unsigned int *rotate,
    unsigned int *hflip,
    unsigned int *vflip)
{
    int rotate_flag = transform & 0x7;
    *rotate = 0;
    *hflip = 0;
    *vflip = 0;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        break;
    case HAL_TRANSFORM_ROT_180:
        *rotate = 180;
        break;
    case HAL_TRANSFORM_ROT_270:
        *rotate = 270;
        break;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        *vflip = 1; /* set vflip to compensate the rot & flip order. */
        break;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        *hflip = 1; /* set hflip to compensate the rot & flip order. */
        break;
    case HAL_TRANSFORM_FLIP_H:
        *hflip = 1;
         break;
    case HAL_TRANSFORM_FLIP_V:
        *vflip = 1;
         break;
    default:
        break;
    }
}

static bool get_plane_size(int V4L2_PIX,
    unsigned int * size,
    unsigned int frame_size,
    int src_planes)
{
    unsigned int frame_ratio = 1;
    int src_bpp    = get_yuv_bpp(V4L2_PIX);

    src_planes = (src_planes == -1) ? 1 : src_planes;
    frame_ratio = 8 * (src_planes -1) / (src_bpp - 8);

    switch (src_planes) {
    case 1:
        switch (V4L2_PIX) {
        case V4L2_PIX_FMT_BGR32:
        case V4L2_PIX_FMT_RGB32:
            size[0] = frame_size << 2;
            break;
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
        case V4L2_PIX_FMT_YVYU:
            size[0] = frame_size << 1;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV21M:
            size[0] = (frame_size * 3) >> 1;
            break;
        default:
            ALOGE("%s::invalid color type", __func__);
            return false;
            break;
        }
        size[1] = 0;
        size[2] = 0;
        break;
    case 2:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = 0;
        break;
    case 3:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = frame_size / frame_ratio;
        break;
    default:
        ALOGE("%s::invalid color foarmt", __func__);
        return false;
        break;
    }

    return true;
}

int exynos_fimc_m2m_config(void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img)
{
    struct FIMC_HANDLE *fimc_handle;
    int32_t      src_color_space;
    int32_t      dst_color_space;
    int ret;
    unsigned int rotate;
    unsigned int hflip;
    unsigned int vflip;

    Exynos_fimc_In();

    fimc_handle = (struct FIMC_HANDLE *)handle;
    if (fimc_handle == NULL) {
        ALOGE("%s::fimc_handle == NULL() fail", __func__);
        return -1;
    }

    if ((src_img->drmMode && !fimc_handle->allow_drm) ||
        (src_img->drmMode != dst_img->drmMode)) {
        ALOGE("%s::invalid drm state request for fimc%d (s=%d d=%d)",
              __func__, fimc_handle->fimc_id,
              src_img->drmMode, dst_img->drmMode);
        return -1;
    }

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    rotateValueHAL2FIMC(dst_img->rot, &rotate, &hflip, &vflip);
    exynos_fimc_set_rotation(fimc_handle, rotate, hflip, vflip);

    ret = exynos_fimc_set_src_format(fimc_handle,  src_img->fw, src_img->fh,
                                  src_img->x, src_img->y, src_img->w, src_img->h,
                                  src_color_space, src_img->cacheable, src_img->drmMode);
    if (ret < 0) {
        ALOGE("%s: fail: exynos_fimc_set_src_format [fw %d fh %d x %d y %d w %d h %d f %x rot %d]",
            __func__, src_img->fw, src_img->fh, src_img->x, src_img->y, src_img->w, src_img->h,
            src_color_space, src_img->rot);
        return -1;
    }

    ret = exynos_fimc_set_dst_format(fimc_handle, dst_img->fw, dst_img->fh,
                                  dst_img->x, dst_img->y, dst_img->w, dst_img->h,
                                  dst_color_space, dst_img->cacheable, dst_img->drmMode,
                                  dst_img->narrowRgb);
    if (ret < 0) {
        ALOGE("%s: fail: exynos_fimc_set_dst_format [fw %d fh %d x %d y %d w %d h %d f %x rot %d]",
            __func__, dst_img->fw, dst_img->fh, dst_img->x, dst_img->y, dst_img->w, dst_img->h,
            src_color_space, dst_img->rot);
        return -1;
    }

    Exynos_fimc_Out();

    return 0;
}

int exynos_fimc_out_config(void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img)
{
    struct FIMC_HANDLE *fimc_handle;
    struct v4l2_format  fmt;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_subdev_format sd_fmt;
    struct v4l2_subdev_crop   sd_crop;
    int i;
    unsigned int rotate;
    unsigned int hflip;
    unsigned int vflip;
    unsigned int plane_size[NUM_OF_FIMC_PLANES];
    bool rgb;
    int csc_range = !dst_img->narrowRgb;

    struct v4l2_rect dst_rect;
    int32_t      src_color_space;
    int32_t      dst_color_space;
    int32_t      src_planes;

    fimc_handle = (struct FIMC_HANDLE *)handle;
     if (fimc_handle == NULL) {
        ALOGE("%s::fimc_handle == NULL() fail", __func__);
        return -1;
    }

    Exynos_fimc_In();

     if (fimc_handle->src.stream_on != false) {
        ALOGE("Error: Src is already streamed on !!!!");
        return -1;
     }

    memcpy(&fimc_handle->src_img, src_img, sizeof(exynos_fimc_img));
    memcpy(&fimc_handle->dst_img, dst_img, sizeof(exynos_fimc_img));
    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    src_planes = get_yuv_planes(src_color_space);
    src_planes = (src_planes == -1) ? 1 : src_planes;
    rgb = get_yuv_planes(dst_color_space) == -1;
    rotateValueHAL2FIMC(dst_img->rot, &rotate, &hflip, &vflip);

    if (m_exynos_fimc_check_src_size(&fimc_handle->src_img.fw, &fimc_handle->src_img.fh,
                                        &fimc_handle->src_img.x, &fimc_handle->src_img.y,
                                        &fimc_handle->src_img.w, &fimc_handle->src_img.h,
                                        src_color_space) == false) {
            ALOGE("%s::m_exynos_fimc_check_src_size() fail", __func__);
            return -1;
    }

    if (m_exynos_fimc_check_dst_size(&fimc_handle->dst_img.fw, &fimc_handle->dst_img.fh,
                                        &fimc_handle->dst_img.x, &fimc_handle->dst_img.y,
                                        &fimc_handle->dst_img.w, &fimc_handle->dst_img.h,
                                        dst_color_space,
                                        rotate) == false) {
            ALOGE("%s::m_exynos_fimc_check_dst_size() fail", __func__);
            return -1;
    }

    /*set: src v4l2_buffer*/
    fimc_handle->src.src_buf_idx = 0;
    fimc_handle->src.qbuf_cnt = 0;
    /* set format: src pad of FIMC sub-dev*/
    sd_fmt.pad   = FIMC_SUBDEV_PAD_SOURCE;
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (fimc_handle->out_mode == FIMC_OUT_FIMD) {
        sd_fmt.format.width  = fimc_handle->dst_img.fw;
        sd_fmt.format.height = fimc_handle->dst_img.fh;
    } else {
        sd_fmt.format.width  = fimc_handle->dst_img.w;
        sd_fmt.format.height = fimc_handle->dst_img.h;
    }
    sd_fmt.format.code   = rgb ? V4L2_MBUS_FMT_XRGB8888_4X8_LE :
                                    V4L2_MBUS_FMT_YUV8_1X24;
    if (exynos_subdev_s_fmt(fimc_handle->fimc_sd_entity->fd, &sd_fmt) < 0) {
            ALOGE("%s::FIMC subdev set format failed", __func__);
            return -1;
    }

    /* set crop: src crop of FIMC sub-dev*/
    sd_crop.pad   = FIMC_SUBDEV_PAD_SOURCE;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (fimc_handle->out_mode == FIMC_OUT_FIMD) {
        sd_crop.rect.left   = fimc_handle->dst_img.x;
        sd_crop.rect.top    = fimc_handle->dst_img.y;
        sd_crop.rect.width  = fimc_handle->dst_img.w;
        sd_crop.rect.height = fimc_handle->dst_img.h;
    } else {
        sd_crop.rect.left   = 0;
        sd_crop.rect.top    = 0;
        sd_crop.rect.width  = fimc_handle->dst_img.w;
        sd_crop.rect.height = fimc_handle->dst_img.h;
    }
    if (exynos_subdev_s_crop(fimc_handle->fimc_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::FIMC subdev set crop failed", __func__);
            return -1;
    }

    /* sink pad is connected to FIMC out */
    /*  set format: sink sub-dev */
    if (fimc_handle->out_mode == FIMC_OUT_FIMD) {
        sd_fmt.pad   = FIMD_SUBDEV_PAD_SINK;
        sd_fmt.format.width  = fimc_handle->dst_img.w;
        sd_fmt.format.height = fimc_handle->dst_img.h;
    } else {
        sd_fmt.pad   = MIXER_V_SUBDEV_PAD_SINK;
        sd_fmt.format.width  = fimc_handle->dst_img.w + fimc_handle->dst_img.x*2;
        sd_fmt.format.height = fimc_handle->dst_img.h + fimc_handle->dst_img.y*2;
    }

    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.code   = rgb ? V4L2_MBUS_FMT_XRGB8888_4X8_LE :
                                    V4L2_MBUS_FMT_YUV8_1X24;
    if (exynos_subdev_s_fmt(fimc_handle->sink_sd_entity->fd, &sd_fmt) < 0) {
        ALOGE("%s::sink:set format failed (PAD=%d)", __func__, sd_fmt.pad);
        return -1;
    }

    /*  set crop: sink sub-dev */
    if (fimc_handle->out_mode == FIMC_OUT_FIMD)
        sd_crop.pad   = FIMD_SUBDEV_PAD_SINK;
    else
        sd_crop.pad   = MIXER_V_SUBDEV_PAD_SINK;

    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (fimc_handle->out_mode == FIMC_OUT_FIMD) {
        sd_crop.rect.left   = fimc_handle->dst_img.x;
        sd_crop.rect.top    = fimc_handle->dst_img.y;
        sd_crop.rect.width  = fimc_handle->dst_img.w;
        sd_crop.rect.height = fimc_handle->dst_img.h;
    } else {
        sd_crop.rect.left   = 0;
        sd_crop.rect.top    = 0;
        sd_crop.rect.width  = fimc_handle->dst_img.w;
        sd_crop.rect.height = fimc_handle->dst_img.h;
    }
    if (exynos_subdev_s_crop(fimc_handle->sink_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::sink: subdev set crop failed(PAD=%d)", __func__, sd_crop.pad);
            return -1;
    }

    if (fimc_handle->out_mode != FIMC_OUT_FIMD) {
        sd_fmt.pad   = MIXER_V_SUBDEV_PAD_SOURCE;
        sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        sd_fmt.format.width  = fimc_handle->dst_img.w + fimc_handle->dst_img.x*2;
        sd_fmt.format.height = fimc_handle->dst_img.h + fimc_handle->dst_img.y*2;
        sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
        if (exynos_subdev_s_fmt(fimc_handle->sink_sd_entity->fd, &sd_fmt) < 0) {
            ALOGE("%s::sink:set format failed (PAD=%d)", __func__, sd_fmt.pad);
            return -1;
        }

        sd_fmt.pad   = MIXER_V_SUBDEV_PAD_SOURCE;
        sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        sd_crop.rect.left   = fimc_handle->dst_img.x;
        sd_crop.rect.top    = fimc_handle->dst_img.y;
        sd_crop.rect.width  = fimc_handle->dst_img.w;
        sd_crop.rect.height = fimc_handle->dst_img.h;
        if (exynos_subdev_s_crop(fimc_handle->sink_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::sink: subdev set crop failed(PAD=%d)", __func__, sd_crop.pad);
            return -1;
        }
    }

    /*set FIMC ctrls */
    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd, V4L2_CID_ROTATE, rotate) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_ROTATE: %d) failed", __func__,  rotate);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd, V4L2_CID_HFLIP, hflip) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_HFLIP: %d) failed", __func__,  hflip);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd, V4L2_CID_VFLIP, vflip) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_VFLIP: %d) failed", __func__,  vflip);
        return -1;
    }

     if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd, V4L2_CID_CACHEABLE, 1) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_CACHEABLE: 1) failed", __func__);
        return -1;
    }
#if 0
    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd,
        V4L2_CID_CONTENT_PROTECTION, fimc_handle->src_img.drmMode) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_CONTENT_PROTECTION) fail", __func__);
        return -1;
    }
#endif
    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_vd_entity->fd, V4L2_CID_CSC_RANGE,
            csc_range)) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_CSC_RANGE: %d) fail", __func__,
                csc_range);
        return -1;
    }

      /* set src format  :FIMC video dev*/
    fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width            = fimc_handle->src_img.fw;
    fmt.fmt.pix_mp.height           = fimc_handle->src_img.fh;
    fmt.fmt.pix_mp.pixelformat    = src_color_space;
    fmt.fmt.pix_mp.field              = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes   = src_planes;

    if (exynos_v4l2_s_fmt(fimc_handle->fimc_vd_entity->fd, &fmt) < 0) {
            ALOGE("%s::videodev set format failed", __func__);
            return -1;
    }

    /* set src crop info :FIMC video dev*/
    crop.type     = fmt.type;
    crop.c.left    = fimc_handle->src_img.x;
    crop.c.top     = fimc_handle->src_img.y;
    crop.c.width  = fimc_handle->src_img.w;
    crop.c.height = fimc_handle->src_img.h;

    if (exynos_v4l2_s_crop(fimc_handle->fimc_vd_entity->fd, &crop) < 0) {
        ALOGE("%s::videodev set crop failed", __func__);
        return -1;
    }

    reqbuf.type   = fmt.type;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = MAX_BUFFERS_FIMC_OUT;

    if (exynos_v4l2_reqbufs(fimc_handle->fimc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }

    Exynos_fimc_Out();

    return 0;
}

static int exynos_fimc_out_run(void *handle,
    exynos_fimc_img *src_img)
{
    struct FIMC_HANDLE *fimc_handle;
    struct v4l2_plane  planes[NUM_OF_FIMC_PLANES];
    struct v4l2_buffer buf;
    int32_t      src_color_space;
    int32_t      src_planes;
    int             i;
    unsigned int plane_size[NUM_OF_FIMC_PLANES];

    fimc_handle = (struct FIMC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    /* All buffers have been queued, dequeue one */
    if (fimc_handle->src.qbuf_cnt == MAX_BUFFERS_FIMC_OUT) {
        memset(&buf, 0, sizeof(struct v4l2_buffer));
        for (i = 0; i < MAX_BUFFERS_FIMC_OUT; i++)
            memset(&planes[i], 0, sizeof(struct v4l2_plane));

        buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory   = V4L2_MEMORY_USERPTR;
        buf.length   = src_planes;
        buf.m.planes = planes;

        if (exynos_v4l2_dqbuf(fimc_handle->fimc_vd_entity->fd, &buf) < 0) {
            ALOGE("%s::dequeue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
                fimc_handle->src.src_buf_idx, MAX_BUFFERS_FIMC_OUT);
            return -1;
        }
        fimc_handle->src.qbuf_cnt--;
    }

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_FIMC_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(fimc_handle->src_img.format);
    src_planes = get_yuv_planes(src_color_space);
    src_planes = (src_planes == -1) ? 1 : src_planes;

    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.flags    = V4L2_BUF_FLAG_USE_SYNC;
    buf.length   = src_planes;
    buf.index    = fimc_handle->src.src_buf_idx;
    buf.m.planes = planes;
    buf.reserved = src_img->acquireFenceFd;

    fimc_handle->src.addr[0] = (void*)src_img->yaddr;
    fimc_handle->src.addr[1] = (void*)src_img->uaddr;
    fimc_handle->src.addr[2] = (void*)src_img->vaddr;

    if (get_plane_size(src_color_space, plane_size,
        fimc_handle->src_img.fw * fimc_handle->src_img.fh, src_planes) != true) {
        ALOGE("%s:get_plane_size:fail", __func__);
        return -1;
    }

    for (i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.fd = (int)fimc_handle->src.addr[i];
        buf.m.planes[i].length    = plane_size[i];
        buf.m.planes[i].bytesused = plane_size[i];
    }

    /* Queue the buf */
    if (exynos_v4l2_qbuf(fimc_handle->fimc_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::queue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            fimc_handle->src.src_buf_idx, MAX_BUFFERS_FIMC_OUT);
        return -1;
    }
    fimc_handle->src.src_buf_idx++;
    fimc_handle->src.src_buf_idx = fimc_handle->src.src_buf_idx % MAX_BUFFERS_FIMC_OUT;
    fimc_handle->src.qbuf_cnt++;

    if (fimc_handle->src.stream_on == false) {
        if (exynos_v4l2_streamon(fimc_handle->fimc_vd_entity->fd, buf.type) < 0) {
            ALOGE("%s::stream on failed", __func__);
            return -1;
        }
        fimc_handle->src.stream_on = true;
    }

    src_img->releaseFenceFd = buf.reserved;
    return 0;
}

int exynos_fimc_out_stop(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_buffer buf;
    struct v4l2_plane  planes[NUM_OF_FIMC_PLANES];
    int i;

    Exynos_fimc_In();

    fimc_handle = (struct FIMC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (fimc_handle->src.stream_on == true) {
        if (exynos_v4l2_streamoff(fimc_handle->fimc_vd_entity->fd,
                                V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
            ALOGE("%s::stream off failed", __func__);
            return -1;
        }
        fimc_handle->src.stream_on = false;
    }

    fimc_handle->src.src_buf_idx = 0;
    fimc_handle->src.qbuf_cnt = 0;

    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = 0;

    if (exynos_v4l2_reqbufs(fimc_handle->fimc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }

    Exynos_fimc_Out();

    return 0;
}

static int exynos_fimc_m2m_run_core(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    bool is_dirty;
    bool is_drm;

    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    is_dirty = fimc_handle->src.dirty || fimc_handle->dst.dirty;
    is_drm = fimc_handle->src.mode_drm;

    if (is_dirty && (fimc_handle->src.mode_drm != fimc_handle->dst.mode_drm)) {
        ALOGE("%s: drm mode mismatch between src and dst, fimc%d (s=%d d=%d)",
              __func__, fimc_handle->fimc_id, fimc_handle->src.mode_drm,
              fimc_handle->dst.mode_drm);
        return -1;
    } else if (is_drm && !fimc_handle->allow_drm) {
        ALOGE("%s: drm mode is not supported on fimc%d", __func__,
              fimc_handle->fimc_id);
        return -1;
    }

    if (m_exynos_fimc_check_src_size(&fimc_handle->src.width, &fimc_handle->src.height,
                                    &fimc_handle->src.crop_left, &fimc_handle->src.crop_top,
                                    &fimc_handle->src.crop_width, &fimc_handle->src.crop_height,
                                    fimc_handle->src.v4l2_colorformat) == false) {
        ALOGE("%s::m_exynos_fimc_check_src_size() fail", __func__);
        return -1;
    }

    if (m_exynos_fimc_check_dst_size(&fimc_handle->dst.width, &fimc_handle->dst.height,
                                    &fimc_handle->dst.crop_left, &fimc_handle->dst.crop_top,
                                    &fimc_handle->dst.crop_width, &fimc_handle->dst.crop_height,
                                    fimc_handle->dst.v4l2_colorformat,
                                    fimc_handle->dst.rotation) == false) {
        ALOGE("%s::m_exynos_fimc_check_dst_size() fail", __func__);
        return -1;
    }

    /* dequeue buffers from previous work if necessary */
    if (fimc_handle->src.stream_on == true) {
        if (exynos_fimc_m2m_wait_frame_done(handle) < 0) {
            ALOGE("%s::exynos_fimc_m2m_wait_frame_done fail", __func__);
            return -1;
        }
    }
#if 0
    /*
     * need to set the content protection flag before doing reqbufs
     * in set_format
     */
    if (is_dirty && fimc_handle->allow_drm && is_drm) {
        if (exynos_v4l2_s_ctrl(fimc_handle->fimc_fd,
                               V4L2_CID_CONTENT_PROTECTION, is_drm) < 0) {
            ALOGE("%s::exynos_v4l2_s_ctrl() fail", __func__);
            return -1;
        }
    }
#endif
    /*
     * from this point on, we have to ensure to call stop to clean up whatever
     * state we have set.
     */

    if (fimc_handle->src.dirty) {
        if (m_exynos_fimc_set_format(fimc_handle->fimc_fd, &fimc_handle->src) == false) {
            ALOGE("%s::m_exynos_fimc_set_format(src) fail", __func__);
            goto done;
        }
        fimc_handle->src.dirty = false;
    }

    if (fimc_handle->dst.dirty) {
        if (m_exynos_fimc_set_format(fimc_handle->fimc_fd, &fimc_handle->dst) == false) {
            ALOGE("%s::m_exynos_fimc_set_format(dst) fail", __func__);
            goto done;
        }
        fimc_handle->dst.dirty = false;
    }
#if 0
    /* if we are enabling drm, make sure to enable hw protection.
     * Need to do this before queuing buffers so that the mmu is reserved
     * and power domain is kept on.
     */
    if (is_dirty && fimc_handle->allow_drm && is_drm) {
        unsigned int protect_id = 0;

        if (fimc_handle->fimc_id == 0) {
            protect_id = CP_PROTECT_FIMC0;
        } else if (fimc_handle->fimc_id == 3) {
            protect_id = CP_PROTECT_FIMC3;
        } else {
            ALOGE("%s::invalid fimc id %d for content protection", __func__,
                  fimc_handle->fimc_id);
            goto done;
        }

        if (CP_Enable_Path_Protection(protect_id) != 0) {
            ALOGE("%s::CP_Enable_Path_Protection failed", __func__);
            goto done;
        }
        fimc_handle->protection_enabled = true;
    }
#endif

    if (m_exynos_fimc_set_addr(fimc_handle->fimc_fd, &fimc_handle->src) == false) {
        ALOGE("%s::m_exynos_fimc_set_addr(src) fail", __func__);
        goto done;
    }

    if (m_exynos_fimc_set_addr(fimc_handle->fimc_fd, &fimc_handle->dst) == false) {
        ALOGE("%s::m_exynos_fimc_set_addr(dst) fail", __func__);
        goto done;
    }

    if (fimc_handle->src.stream_on == false) {
        if (exynos_v4l2_streamon(fimc_handle->fimc_fd, fimc_handle->src.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamon(src) fail", __func__);
            goto done;
        }
        fimc_handle->src.stream_on = true;
    }

    if (fimc_handle->dst.stream_on == false) {
        if (exynos_v4l2_streamon(fimc_handle->fimc_fd, fimc_handle->dst.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamon(dst) fail", __func__);
            goto done;
        }
        fimc_handle->dst.stream_on = true;
    }

    Exynos_fimc_Out();

    return 0;

done:
    exynos_fimc_m2m_stop(handle);
    return -1;
}

static int exynos_fimc_m2m_wait_frame_done(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;

    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if ((fimc_handle->src.stream_on == false) || (fimc_handle->dst.stream_on == false)) {
        ALOGE("%s:: src_strean_on or dst_stream_on are false", __func__);
        return -1;
    }

    if (fimc_handle->src.buffer_queued) {
        if (exynos_v4l2_dqbuf(fimc_handle->fimc_fd, &fimc_handle->src.buffer) < 0) {
            ALOGE("%s::exynos_v4l2_dqbuf(src) fail", __func__);
            return -1;
        }
        fimc_handle->src.buffer_queued = false;
    }

    if (fimc_handle->dst.buffer_queued) {
        if (exynos_v4l2_dqbuf(fimc_handle->fimc_fd, &fimc_handle->dst.buffer) < 0) {
            ALOGE("%s::exynos_v4l2_dqbuf(dst) fail", __func__);
            return -1;
        }
        fimc_handle->dst.buffer_queued = false;
    }

    Exynos_fimc_Out();

    return 0;
}

static int exynos_fimc_m2m_stop(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    struct v4l2_requestbuffers req_buf;
    int ret = 0;

    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (!fimc_handle->src.stream_on && !fimc_handle->dst.stream_on) {
        /* wasn't streaming, return success */
        return 0;
    } else if (fimc_handle->src.stream_on != fimc_handle->dst.stream_on) {
        ALOGE("%s: invalid state, queue stream state doesn't match (%d != %d)",
              __func__, fimc_handle->src.stream_on, fimc_handle->dst.stream_on);
        ret = -1;
    }

    /*
     * we need to plow forward on errors below to make sure that if we had
     * turned on content protection on secure side, we turn it off.
     *
     * also, if we only failed to turn on one of the streams, we'll turn
     * the other one off correctly.
     */
    if (fimc_handle->src.stream_on == true) {
        if (exynos_v4l2_streamoff(fimc_handle->fimc_fd, fimc_handle->src.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamoff(src) fail", __func__);
            ret = -1;
        }
        fimc_handle->src.stream_on = false;
    }


    if (fimc_handle->dst.stream_on == true) {
        if (exynos_v4l2_streamoff(fimc_handle->fimc_fd, fimc_handle->dst.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamoff(dst) fail", __func__);
            ret = -1;
        }
        fimc_handle->dst.stream_on = false;
    }
#if 0
    /* if drm is enabled */
    if (fimc_handle->allow_drm && fimc_handle->protection_enabled) {
        unsigned int protect_id = 0;

        if (fimc_handle->fimc_id == 0)
            protect_id = CP_PROTECT_FIMC0;
        else if (fimc_handle->fimc_id == 3)
            protect_id = CP_PROTECT_FIMC3;

        CP_Disable_Path_Protection(protect_id);
        fimc_handle->protection_enabled = false;
    }

    if (exynos_v4l2_s_ctrl(fimc_handle->fimc_fd,
                           V4L2_CID_CONTENT_PROTECTION, 0) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_CONTENT_PROTECTION) fail",
              __func__);
        ret = -1;
    }
#endif
    /* src: clear_buf */
    req_buf.count  = 0;
    req_buf.type   = fimc_handle->src.buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(fimc_handle->fimc_fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs():src: fail", __func__);
        ret = -1;
    }

    /* dst: clear_buf */
    req_buf.count  = 0;
    req_buf.type   = fimc_handle->dst.buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(fimc_handle->fimc_fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs():dst: fail", __func__);
        ret = -1;
    }

    Exynos_fimc_Out();

    return ret;
}

int exynos_fimc_convert(
    void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    int ret    = -1;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    if (fimc_handle->flag_local_path == true) {
        ALOGE("%s::this exynos_fimc is connected by another hw internaly. So, don't call exynos_fimc_convert()", __func__);
            goto done;
        }

    if (exynos_fimc_m2m_run_core(handle) < 0) {
        ALOGE("%s::exynos_fimc_run_core fail", __func__);
            goto done;
        }

    if (exynos_fimc_m2m_wait_frame_done(handle) < 0) {
        ALOGE("%s::exynos_fimc_m2m_wait_frame_done", __func__);
        goto done;
    }

    if (fimc_handle->src.releaseFenceFd >= 0) {
        close(fimc_handle->src.releaseFenceFd);
        fimc_handle->src.releaseFenceFd = -1;
    }

    if (fimc_handle->dst.releaseFenceFd >= 0) {
        close(fimc_handle->dst.releaseFenceFd);
        fimc_handle->dst.releaseFenceFd = -1;
    }

    if (exynos_fimc_m2m_stop(handle) < 0) {
        ALOGE("%s::exynos_fimc_m2m_stop", __func__);
        goto done;
    }

    ret = 0;

done:
    if (fimc_handle->flag_exclusive_open == false) {
        if (fimc_handle->flag_local_path == false)
            exynos_mutex_unlock(fimc_handle->cur_obj_mutex);
    }

    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();

    return ret;
}

int exynos_fimc_m2m_run(void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img)
{
    struct FIMC_HANDLE *fimc_handle = handle;
    void *addr[3] = {NULL, NULL, NULL};
    int ret = 0;

    Exynos_fimc_In();

    addr[0] = (void *)src_img->yaddr;
    addr[1] = (void *)src_img->uaddr;
    addr[2] = (void *)src_img->vaddr;
    ret = exynos_fimc_set_src_addr(handle, addr, src_img->acquireFenceFd);
    if (ret < 0) {
        ALOGE("%s::fail: exynos_fimc_set_src_addr[%x %x %x]", __func__,
            (unsigned int)addr[0], (unsigned int)addr[1], (unsigned int)addr[2]);
        return -1;
    }

    addr[0] = (void *)dst_img->yaddr;
    addr[1] = (void *)dst_img->uaddr;
    addr[2] = (void *)dst_img->vaddr;
    ret = exynos_fimc_set_dst_addr(handle, addr, dst_img->acquireFenceFd);
    if (ret < 0) {
        ALOGE("%s::fail: exynos_fimc_set_dst_addr[%x %x %x]", __func__,
            (unsigned int)addr[0], (unsigned int)addr[1], (unsigned int)addr[2]);
        return -1;
    }

    ret = exynos_fimc_m2m_run_core(handle);
     if (ret < 0) {
        ALOGE("%s::fail: exynos_fimc_m2m_run_core", __func__);
        return -1;
    }

    if (src_img->acquireFenceFd >= 0) {
        close(src_img->acquireFenceFd);
        src_img->acquireFenceFd = -1;
    }

    if (dst_img->acquireFenceFd >= 0) {
        close(dst_img->acquireFenceFd);
        dst_img->acquireFenceFd = -1;
    }

    src_img->releaseFenceFd = fimc_handle->src.releaseFenceFd;
    dst_img->releaseFenceFd = fimc_handle->dst.releaseFenceFd;

    Exynos_fimc_Out();

    return 0;
}

int exynos_fimc_config_exclusive(void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img)
{

    Exynos_fimc_In();

     struct FIMC_HANDLE *fimc_handle;
    int ret = 0;
    fimc_handle = (struct FIMC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (fimc_handle->fimc_mode) {
    case FIMC_M2M_MODE:
        ret = exynos_fimc_m2m_config(handle, src_img, dst_img);
        break;
    case FIMC_OUTPUT_MODE:
        ret = exynos_fimc_out_config(handle, src_img, dst_img);
        break;
    case  FIMC_CAPTURE_MODE:
        //to do
        break;
    default:
        break;
    }

    Exynos_fimc_Out();

    return ret;

}

int exynos_fimc_run_exclusive(void *handle,
    exynos_fimc_img *src_img,
    exynos_fimc_img *dst_img)
{
    struct FIMC_HANDLE *fimc_handle;
    int ret = 0;

    Exynos_fimc_In();

    fimc_handle = (struct FIMC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (fimc_handle->fimc_mode) {
    case FIMC_M2M_MODE:
        ret = exynos_fimc_m2m_run(handle, src_img, dst_img);
        break;
    case FIMC_OUTPUT_MODE:
        ret = exynos_fimc_out_run(handle, src_img);
        break;
    case  FIMC_CAPTURE_MODE:
        //to do
        break;
    default:
        break;
    }

    Exynos_fimc_Out();

    return ret;
}

int exynos_fimc_wait_frame_done_exclusive(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    int ret = 0;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (fimc_handle->fimc_mode == FIMC_M2M_MODE)
        ret = exynos_fimc_m2m_wait_frame_done(handle);

    Exynos_fimc_Out();

    return ret;
}

int exynos_fimc_stop_exclusive(void *handle)
{
    struct FIMC_HANDLE *fimc_handle;
    int ret = 0;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (fimc_handle->fimc_mode) {
    case FIMC_M2M_MODE:
        ret = exynos_fimc_m2m_stop(handle);
        break;
    case FIMC_OUTPUT_MODE:
        ret = exynos_fimc_out_stop(handle);
        break;
    case  FIMC_CAPTURE_MODE:
        //to do
        break;
    default:
        break;
    }

    Exynos_fimc_Out();

    return ret;
}

int exynos_fimc_connect(
    void *handle,
    void *hw)
{
    struct FIMC_HANDLE *fimc_handle;
    int ret    = -1;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->flag_local_path = true;

    if (exynos_mutex_trylock(fimc_handle->cur_obj_mutex) == false) {
        if (m_exynos_fimc_find_and_trylock_and_create(fimc_handle) == false) {
            ALOGE("%s::m_exynos_fimc_find_and_trylock_and_create() fail", __func__);
            goto done;
        }
    }

    ret = 0;

    Exynos_fimc_Out();

done:
    exynos_mutex_unlock(fimc_handle->op_mutex);

    return ret;
}

int exynos_fimc_disconnect(
    void *handle,
    void *hw)
{
    struct FIMC_HANDLE *fimc_handle;
    fimc_handle = (struct FIMC_HANDLE *)handle;

    Exynos_fimc_In();

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(fimc_handle->op_mutex);

    fimc_handle->flag_local_path = false;

    exynos_mutex_unlock(fimc_handle->cur_obj_mutex);

    exynos_mutex_unlock(fimc_handle->op_mutex);

    Exynos_fimc_Out();

    return 0;
}
