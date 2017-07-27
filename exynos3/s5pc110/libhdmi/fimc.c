/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

#include <hardware/hwcomposer.h>

#include <cutils/log.h>

#include "fimc.h"

typedef struct sec_img  sec_img;
typedef struct sec_rect sec_rect;

int fimc_v4l2_set_src(int fd, unsigned int hw_ver, s5p_fimc_img_info *src)
{
    struct v4l2_format  fmt;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;

    /*
     * To set size & format for source image (DMA-INPUT)
     */
    fmt.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width       = src->full_width;
    fmt.fmt.pix.height      = src->full_height;
    fmt.fmt.pix.pixelformat = src->color_space;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl (fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("VIDIOC_S_FMT failed : errno=%d (%s) : fd=%d", errno,
                strerror(errno), fd);
        return -1;
    }

    /*
     * crop input size
     */
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (0x50 == hw_ver) {
        crop.c.left   = src->start_x;
        crop.c.top    = src->start_y;
    } else {
        crop.c.left   = 0;
        crop.c.top    = 0;
    }
    crop.c.width  = src->width;
    crop.c.height = src->height;
    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        ALOGE("Error in video VIDIOC_S_CROP (%d, %d, %d, %d)",
                crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        return -1;
    }

    /*
     * input buffer type
     */
    req.count       = 1;
    req.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory      = V4L2_MEMORY_USERPTR;

    if (ioctl (fd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Error in VIDIOC_REQBUFS");
        return -1;
    }

    return 0;
}

int fimc_v4l2_set_dst(int fd,
                      s5p_fimc_img_info *dst,
                      int rotation,
                      int flag_h_flip,
                      int flag_v_flip,
                      unsigned int addr)
{
    struct v4l2_format      fmt;
    struct v4l2_control     vc;
    struct v4l2_framebuffer fbuf;

    /*
     * set rotation configuration
     */
    vc.id = V4L2_CID_HFLIP;
    vc.value = flag_h_flip;
    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("Error in video VIDIOC_S_CTRL - flag_h_flip (%d)", flag_h_flip);
        return -1;
    }

    vc.id = V4L2_CID_VFLIP;
    vc.value = flag_v_flip;
    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("Error in video VIDIOC_S_CTRL - flag_v_flip (%d)", flag_v_flip);
        return -1;
    }

    vc.id = V4L2_CID_ROTATION;
    vc.value = rotation;
    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("Error in video VIDIOC_S_CTRL - rotation (%d)", rotation);
        return -1;
    }

    /*
     * set size, format & address for destination image (DMA-OUTPUT)
     */
    if (ioctl (fd, VIDIOC_G_FBUF, &fbuf) < 0) {
        ALOGE("Error in video VIDIOC_G_FBUF");
        return -1;
    }

    fbuf.base            = (void *)addr;
    fbuf.fmt.width       = dst->full_width;
    fbuf.fmt.height      = dst->full_height;
    fbuf.fmt.pixelformat = dst->color_space;
    if (ioctl (fd, VIDIOC_S_FBUF, &fbuf) < 0) {
        ALOGE("Error in video VIDIOC_S_FBUF 0x%x %d %d %d",
                (void *)addr, dst->full_width, dst->full_height,
                dst->color_space);
        return -1;
    }

    /*
     * set destination window
     */
    fmt.type             = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    fmt.fmt.win.w.left   = dst->start_x;
    fmt.fmt.win.w.top    = dst->start_y;
    fmt.fmt.win.w.width  = dst->width;
    fmt.fmt.win.w.height = dst->height;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ALOGE("Error in video VIDIOC_S_FMT %d %d %d %d",
                dst->start_x, dst->start_y, dst->width, dst->height);
        return -1;
    }

    return 0;
}

static int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
    if (ioctl (fd, VIDIOC_STREAMON, &type) < 0) {
        ALOGE("Error in VIDIOC_STREAMON");
        return -1;
    }

    return 0;
}

static int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf)
{
    struct v4l2_buffer buf;

    buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.m.userptr   = (unsigned long)fimc_buf;
    buf.length      = 0;
    buf.index       = 0;

    if (ioctl (fd, VIDIOC_QBUF, &buf) < 0) {
        ALOGE("Error in VIDIOC_QBUF");
        return -1;
    }

    return 0;
}

static int fimc_v4l2_dequeue(int fd)
{
    struct v4l2_buffer buf;

    buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory      = V4L2_MEMORY_USERPTR;

    if (ioctl (fd, VIDIOC_DQBUF, &buf) < 0) {
        ALOGE("Error in VIDIOC_DQBUF");
        return -1;
    }

    return buf.index;
}

static int fimc_v4l2_stream_off(int fd)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (ioctl (fd, VIDIOC_STREAMOFF, &type) < 0) {
        ALOGE("Error in VIDIOC_STREAMOFF");
        return -1;
    }

    return 0;
}

static int fimc_v4l2_clr_buf(int fd)
{
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory  = V4L2_MEMORY_USERPTR;

    if (ioctl (fd, VIDIOC_REQBUFS, &req) < 0) {
        ALOGE("Error in VIDIOC_REQBUFS");
    }

    return 0;
}

static int fimc_handle_oneshot(int fd, struct fimc_buf *fimc_buf)
{
    int ret =0;

    if (fimc_v4l2_stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT) < 0) {
        ALOGE("Fail : v4l2_stream_on()");
        return -1;
    }

    if (fimc_v4l2_queue(fd, fimc_buf) < 0) {
        ALOGE("Fail : v4l2_queue()");
        ret = -1;
        goto stream_off;
    }

    if (fimc_v4l2_dequeue(fd) < 0) {
        ALOGE("Fail : v4l2_dequeue()");
        ret = -1;
        goto stream_off;
    }

stream_off:
    if (fimc_v4l2_stream_off(fd) < 0) {
        ALOGE("Fail : v4l2_stream_off()");
        return -1;
    }

    if (fimc_v4l2_clr_buf(fd) < 0) {
        ALOGE("Fail : v4l2_clr_buf()");
        return -1;
    }

    return ret;
}

static int get_src_phys_addr(s5p_fimc_t *fimc,
                             sec_img *src_img,
                             unsigned int *phyAddr)
{
   if(src_img->mem_type == FIMC_MEM_TYPE_PHYS) {
        switch(src_img->format) {
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            fimc->params.src.buf_addr_phy_rgb_y = phyAddr[0];
            fimc->params.src.buf_addr_phy_cb    = phyAddr[1];
            break;
        default:
            ALOGE("%s format error (format=0x%x)", __func__,
                    src_img->format);
            return -1;
        }
    } else {
        ALOGE("%s mem_type error (mem_type=%d)", __func__, src_img->mem_type);
        return -1;
    }

    return 0;
}

static int get_dst_phys_addr(s5p_fimc_t *fimc,
                             sec_img *dst_img)
{
    unsigned int dst_phys_addr  = 0;

    if (FIMC_MEM_TYPE_PHYS == dst_img->mem_type && 0 != dst_img->base)
        dst_phys_addr = dst_img->base;
    else {
        ALOGE("%s::get_dst_phys_addr fail ", __func__);
        dst_phys_addr = 0;
    }
    return dst_phys_addr;
}

static inline int rotateValueHAL2PP(unsigned char transform,
                                    int *flag_h_flip,
                                    int *flag_v_flip)
{
    int rotate_result = 0;
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:
        rotate_result = 90;
        break;
    case HAL_TRANSFORM_ROT_180:
        rotate_result = 180;
        break;
    case HAL_TRANSFORM_ROT_270:
        rotate_result = 270;
        break;
    }

    switch (rotate_flag) {
    case HAL_TRANSFORM_FLIP_H:
        *flag_h_flip = 1;
        *flag_v_flip = 0;
        break;
    case HAL_TRANSFORM_FLIP_V:
        *flag_h_flip = 0;
        *flag_v_flip = 1;
        break;
    default:
        *flag_h_flip = 0;
        *flag_v_flip = 0;
        break;
    }

    return rotate_result;
}

static inline int multipleOfN(int number, int N)
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

static inline int widthOfPP(unsigned int ver,
                            int pp_color_format,
                            int number)
{
    if (0x50 == ver) {
        switch(pp_color_format) {
        /* 422 1/2/3 plane */
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_YUV422P:

        /* 420 2/3 plane */
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_YUV420:
            return multipleOfN(number, 2);

        default :
            return number;
        }
    } else {
        switch(pp_color_format) {
        case V4L2_PIX_FMT_RGB565:
            return multipleOfN(number, 8);

        case V4L2_PIX_FMT_RGB32:
            return multipleOfN(number, 4);

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
            return multipleOfN(number, 4);

        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_NV16:
            return multipleOfN(number, 8);

        case V4L2_PIX_FMT_YUV422P:
            return multipleOfN(number, 16);

        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
            return multipleOfN(number, 8);

        case V4L2_PIX_FMT_YUV420:
            return multipleOfN(number, 16);

        default :
            return number;
        }
    }
    return number;
}

static inline int heightOfPP(int pp_color_format,
                             int number)
{
    switch(pp_color_format) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        return multipleOfN(number, 2);

    default :
        return number;
    }
    return number;
}

static int fimc_core(s5p_fimc_t *fimc,
                     sec_img *src_img,
                     sec_rect *src_rect,
                     uint32_t src_color_space,
                     unsigned int dst_phys_addr,
                     sec_img *dst_img,
                     sec_rect *dst_rect,
                     uint32_t dst_color_space,
                     int transform)
{
    s5p_fimc_params_t * params = &(fimc->params);

    unsigned int    frame_size = 0;
    struct fimc_buf fimc_src_buf;

    int src_bpp, src_planes;
    int flag_h_flip = 0;
    int flag_v_flip = 0;
    int rotate_value = rotateValueHAL2PP(transform, &flag_h_flip, &flag_v_flip);

    /* set post processor configuration */
    params->src.full_width  = src_img->w;
    params->src.full_height = src_img->h;
    params->src.start_x     = src_rect->x;
    params->src.start_y     = src_rect->y;
    params->src.width       = widthOfPP(fimc->hw_ver, src_color_space, src_rect->w);
    params->src.height      = heightOfPP(src_color_space, src_rect->h);
    params->src.color_space = src_color_space;


    /* check minimum */
    if (src_rect->w < 16 || src_rect->h < 8) {
        ALOGE("%s src size is not supported by fimc : f_w=%d f_h=%d x=%d y=%d \
            	w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
				params->src.full_width, params->src.full_height,
				params->src.start_x, params->src.start_y, params->src.width,
				params->src.height, src_rect->w, src_rect->h,
				params->src.color_space);
        return -1;
    }

    switch (rotate_value) {
    case 0:
        params->dst.full_width  = dst_img->w;
        params->dst.full_height = dst_img->h;

        params->dst.start_x     = dst_rect->x;
        params->dst.start_y     = dst_rect->y;

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);
        params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);
        break;
    case 90:
        params->dst.full_width  = dst_img->h;
        params->dst.full_height = dst_img->w;

        params->dst.start_x     = dst_rect->y;
        params->dst.start_y     = dst_img->w - (dst_rect->x + dst_rect->w);

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->h);
        params->dst.height      =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);

        if (0x50 > fimc->hw_ver)
            params->dst.start_y += (dst_rect->w - params->dst.height);
        break;
    case 180:
        params->dst.full_width  = dst_img->w;
        params->dst.full_height = dst_img->h;

        params->dst.start_x     = dst_img->w - (dst_rect->x + dst_rect->w);
        params->dst.start_y     = dst_img->h - (dst_rect->y + dst_rect->h);

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);
        params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);
        break;
    case 270:
        params->dst.full_width  = dst_img->h;
        params->dst.full_height = dst_img->w;

        params->dst.start_x     = dst_img->h - (dst_rect->y + dst_rect->h);
        params->dst.start_y     = dst_rect->x;

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->h);
        params->dst.height      =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);

        if (0x50 > fimc->hw_ver)
            params->dst.start_y += (dst_rect->w - params->dst.height);
        break;
    }

    params->dst.color_space = dst_color_space;

    /* check minimum */
    if (dst_rect->w  < 8 || dst_rect->h < 4) {
        ALOGE("%s dst size is not supported by fimc : \
				f_w=%d f_h=%d x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x",
				__func__, params->dst.full_width, params->dst.full_height,
				params->dst.start_x, params->dst.start_y, params->dst.width,
				params->dst.height, dst_rect->w, dst_rect->h,
				params->dst.color_space);
        return -1;
    }

    /* check scaling limit
     * the scaling limie must not be more than MAX_RESIZING_RATIO_LIMIT
     */
    if (((src_rect->w > dst_rect->w) &&
        ((src_rect->w / dst_rect->w) > MAX_RESIZING_RATIO_LIMIT)) ||
        ((dst_rect->w > src_rect->w) &&
        ((dst_rect->w / src_rect->w) > MAX_RESIZING_RATIO_LIMIT))) {
        ALOGE("%s over scaling limit : src.w=%d dst.w=%d (limit=%d)",
            	__func__, src_rect->w, dst_rect->w, MAX_RESIZING_RATIO_LIMIT);
        return -1;
    }


    /* set configuration related to destination (DMA-OUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */
    if (fimc_v4l2_set_dst(fimc->dev_fd,
                          &params->dst,
                          rotate_value,
                          flag_h_flip,
                          flag_v_flip,
                          dst_phys_addr) < 0) {
        return -1;
    }

    /* set configuration related to source (DMA-INPUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */
    if (fimc_v4l2_set_src(fimc->dev_fd, fimc->hw_ver, &params->src) < 0)
        return -1;

    /* set input dma address (Y/RGB, Cb, Cr) */
    switch (src_img->format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        /* for video display zero copy case */
        fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
        fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
        break;

    default:
        /* set source image */
        fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
        break;
    }

    if (fimc_handle_oneshot(fimc->dev_fd, &fimc_src_buf) < 0) {
        fimc_v4l2_clr_buf(fimc->dev_fd);
        return -1;
    }

    return 0;
}

static
void* fimc_get_reserved_mem_addr(s5p_fimc_t *fimc)
{
    int ret;
    struct v4l2_control vc;

    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;

    ret = ioctl(fimc->dev_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Err(%s) in video VIDIOC_G_CTRL (%d)",ret);
        return NULL;
    }

    return vc.value;
}

int fimc_open(s5p_fimc_t *fimc, const char* dev)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_control vc;

    /* open device file */
    if(fimc->dev_fd < 0) {
        fimc->dev_fd = open(dev, O_RDWR);
        if (fimc->dev_fd < 0) {
            ALOGE("%s::Post processor open error (%d)", __func__, errno);
            goto err;
        }
    }

    /* check capability */
    if (ioctl(fimc->dev_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("VIDIOC_QUERYCAP failed");
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%d has no streaming support", fimc->dev_fd);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        ALOGE("%d is no video output", fimc->dev_fd);
        goto err;
    }

    /*
     * malloc fimc_outinfo structure
     */
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(fimc->dev_fd, VIDIOC_G_FMT, &fmt) < 0) {
        ALOGE("%s::Error in video VIDIOC_G_FMT", __func__);
        goto err;
    }

    fimc->out_buf.phys_addr = fimc_get_reserved_mem_addr(fimc);

    vc.id = V4L2_CID_FIMC_VERSION;
    vc.value = 0;

    if (ioctl(fimc->dev_fd, VIDIOC_G_CTRL, &vc) < 0) {
        ALOGE("%s::Error in video VIDIOC_G_CTRL", __func__);
        goto err;
    }
    fimc->hw_ver = vc.value;

    return 0;

err:
    if (0 <= fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd = -1;

    return -1;
}

void fimc_close(s5p_fimc_t *fimc)
{
    /* close */
    if (0 <= fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd = -1;
}

int fimc_flush(s5p_fimc_t *fimc,
               struct sec_img *src_img,
               struct sec_rect *src_rect,
               struct sec_img *dst_img,
               struct sec_rect *dst_rect,
               unsigned int *phyAddr,
               uint32_t transform)
{
    unsigned int dst_phys_addr = 0;
    int32_t      src_color_space;
    int32_t      dst_color_space;

    /* 1 : source address and size */

    if(0 > get_src_phys_addr(fimc, src_img, phyAddr))
        return -1;

    /* 2 : destination address and size */
    if(0 == (dst_phys_addr = get_dst_phys_addr(fimc, dst_img)))
        return -2;

    /* check whether fimc supports the src format */
    if (0 > (src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format)))
        return -3;

    if (0 > (dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format)))
        return -4;

    if(fimc_core(fimc, src_img, src_rect, (uint32_t)src_color_space,
            dst_phys_addr, dst_img, dst_rect, (uint32_t)dst_color_space, transform) < 0)
        return -5;

    return 0;
}
