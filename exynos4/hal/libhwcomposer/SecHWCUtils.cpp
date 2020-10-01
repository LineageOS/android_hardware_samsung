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

/*
 *    Revision History:
 * - 2011/03/11 : Rama, Meka(v.meka@samsung.com)
 * Initial version
 *
 * - 2011/12/07 : Jeonghee, Kim(jhhhh.kim@samsung.com)
 * Add V4L2_PIX_FMT_YUV420M V4L2_PIX_FMT_NV12M
 *
 */

#include "SecHWCUtils.h"
#define V4L2_BUF_TYPE_OUTPUT V4L2_BUF_TYPE_VIDEO_OUTPUT
#define V4L2_BUF_TYPE_CAPTURE V4L2_BUF_TYPE_VIDEO_CAPTURE

#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

//#define CHECK_FPS
#ifdef CHECK_FPS
#include <sys/time.h>
#include <unistd.h>
#define CHK_FRAME_CNT 57

void check_fps()
{
    static struct timeval tick, tick_old;
    static int total = 0;
    static int cnt = 0;
    int FPS;
    cnt++;
    gettimeofday(&tick, NULL);
    if (cnt > 10) {
        if (tick.tv_sec > tick_old.tv_sec)
            total += ((tick.tv_usec/1000) + (tick.tv_sec - tick_old.tv_sec)*1000 - (tick_old.tv_usec/1000));
        else
            total += ((tick.tv_usec - tick_old.tv_usec)/1000);

        memcpy(&tick_old, &tick, sizeof(timeval));
        if (cnt == (10 + CHK_FRAME_CNT)) {
            FPS = 1000*CHK_FRAME_CNT/total;
            ALOGE("[FPS]:%d\n", FPS);
            total = 0;
            cnt = 10;
        }
    } else {
        memcpy(&tick_old, &tick, sizeof(timeval));
        total = 0;
    }
}
#endif

struct yuv_fmt_list yuv_list[] = {
    { "V4L2_PIX_FMT_NV12",      "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12,     12, 2 },
    { "V4L2_PIX_FMT_NV12T",     "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12T,    12, 2 },
    { "V4L2_PIX_FMT_NV21",      "YUV420/2P/LSB_CRCB",   V4L2_PIX_FMT_NV21,     12, 2 },
    { "V4L2_PIX_FMT_NV21X",     "YUV420/2P/MSB_CBCR",   V4L2_PIX_FMT_NV21X,    12, 2 },
    { "V4L2_PIX_FMT_NV12X",     "YUV420/2P/MSB_CRCB",   V4L2_PIX_FMT_NV12X,    12, 2 },
    { "V4L2_PIX_FMT_YUV420",    "YUV420/3P",            V4L2_PIX_FMT_YUV420,   12, 3 },
    { "V4L2_PIX_FMT_YUYV",      "YUV422/1P/YCBYCR",     V4L2_PIX_FMT_YUYV,     16, 1 },
    { "V4L2_PIX_FMT_YVYU",      "YUV422/1P/YCRYCB",     V4L2_PIX_FMT_YVYU,     16, 1 },
    { "V4L2_PIX_FMT_UYVY",      "YUV422/1P/CBYCRY",     V4L2_PIX_FMT_UYVY,     16, 1 },
    { "V4L2_PIX_FMT_VYUY",      "YUV422/1P/CRYCBY",     V4L2_PIX_FMT_VYUY,     16, 1 },
    { "V4L2_PIX_FMT_UV12",      "YUV422/2P/LSB_CBCR",   V4L2_PIX_FMT_NV16,     16, 2 },
    { "V4L2_PIX_FMT_UV21",      "YUV422/2P/LSB_CRCB",   V4L2_PIX_FMT_NV61,     16, 2 },
    { "V4L2_PIX_FMT_UV12X",     "YUV422/2P/MSB_CBCR",   V4L2_PIX_FMT_NV16X,    16, 2 },
    { "V4L2_PIX_FMT_UV21X",     "YUV422/2P/MSB_CRCB",   V4L2_PIX_FMT_NV61X,    16, 2 },
    { "V4L2_PIX_FMT_YUV422P",   "YUV422/3P",            V4L2_PIX_FMT_YUV422P,  16, 3 },
};

int window_open(struct hwc_win_info_t *win, int id)
{
    int fd = 0;
    char name[64];
    int vsync = 1;
    int real_id = id;

    char const * const device_template = "/dev/graphics/fb%u";
    // window & FB maping
    // fb0 -> win-id : 2
    // fb1 -> win-id : 3
    // fb2 -> win-id : 4
    // fb3 -> win-id : 0
    // fb4 -> win_id : 1
    // it is pre assumed that ...win0 or win1 is used here..

    switch (id) {
    case 0:
        real_id = 3;
        break;
    case 1:
        real_id = 4;
        break;
    case 2:
        real_id = 0;
        break;
    case 3:
        real_id = 1;
        break;
    case 4:
        real_id = 2;
        break;
    default:
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::id(%d) is weird", __func__, id);
        goto error;
}

//  0/10
//    snprintf(name, 64, device_template, id + 3);
//  5/10
//    snprintf(name, 64, device_template, id + 0);
//  0/10
//    snprintf(name, 64, device_template, id + 1);
    snprintf(name, 64, device_template, real_id);

    win->fd = open(name, O_RDWR);
    if (win->fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Failed to open window device (%s) : %s",
                __func__, strerror(errno), name);
        goto error;
    }

#ifdef ENABLE_FIMD_VSYNC
    if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
        goto error;
    }
#endif

    return 0;

error:
    if (0 < win->fd)
        close(win->fd);
    win->fd = 0;

    return -1;
}

int window_close(struct hwc_win_info_t *win)
{
    int ret = 0;

    if (0 < win->fd) {

#ifdef ENABLE_FIMD_VSYNC
        int vsync = 0;
        if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0)
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
#endif
        ret = close(win->fd);
    }
    win->fd = 0;

    return ret;
}

int window_set_pos(struct hwc_win_info_t *win)
{
    struct s3cfb_user_window window;

    //before changing the screen configuration...powerdown the window
    if (window_hide(win) != 0)
        return -1;

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: x(%d), y(%d)",
            __func__, win->rect_info.x, win->rect_info.y);

    win->var_info.xres_virtual = (win->lcd_info.xres + 15) & ~ 15;
    win->var_info.yres_virtual = win->lcd_info.yres * NUM_OF_WIN_BUF;
    win->var_info.xres = win->rect_info.w;
    win->var_info.yres = win->rect_info.h;

    win->var_info.activate &= ~FB_ACTIVATE_MASK;
    win->var_info.activate |= FB_ACTIVATE_FORCE;

    if (ioctl(win->fd, FBIOPUT_VSCREENINFO, &(win->var_info)) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOPUT_VSCREENINFO(%d, %d) fail",
          __func__, win->rect_info.w, win->rect_info.h);
        return -1;
    }

    window.x = win->rect_info.x;
    window.y = win->rect_info.y;

    if (ioctl(win->fd, S3CFB_WIN_POSITION, &window) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_WIN_POSITION(%d, %d) fail",
            __func__, window.x, window.y);
      return -1;
    }

    return 0;
}

int window_get_info(struct hwc_win_info_t *win, int win_num)
{
    int temp_size = 0;

    if (ioctl(win->fd, FBIOGET_FSCREENINFO, &win->fix_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_FSCREENINFO failed : %s",
                strerror(errno));
        goto error;
    }

    win->size = win->fix_info.line_length * win->var_info.yres;

    for (int j = 0; j < NUM_OF_WIN_BUF; j++) {
        temp_size = win->size * j;
        win->addr[j] = win->fix_info.smem_start + temp_size;
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::win-%d add[%d]  %x ",
                __func__, win_num, j,  win->addr[j]);
    }
    return 0;

error:
    win->fix_info.smem_start = 0;

    return -1;
}

int window_pan_display(struct hwc_win_info_t *win)
{
    struct fb_var_screeninfo *lcd_info = &(win->lcd_info);

#ifdef ENABLE_FIMD_VSYNC
    if (ioctl(win->fd, FBIO_WAITFORVSYNC, 0) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIO_WAITFORVSYNC fail(%s)",
                __func__, strerror(errno));
#endif

    lcd_info->yoffset = lcd_info->yres * win->buf_index;

    if (ioctl(win->fd, FBIOPAN_DISPLAY, lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOPAN_DISPLAY(%d / %d / %d) fail(%s)",
            __func__,
            lcd_info->yres,
            win->buf_index, lcd_info->yres_virtual,
            strerror(errno));
        return -1;
    }
    return 0;
}

int window_show(struct hwc_win_info_t *win)
{
    if (win->power_state == 0) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
                __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 1;
    }
    return 0;
}

int window_hide(struct hwc_win_info_t *win)
{
    if (win->power_state == 1) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
             __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 0;
    }
    return 0;
}

int window_get_global_lcd_info(struct hwc_context_t *ctx)
{
    if (ioctl(ctx->global_lcd_win.fd, FBIOGET_VSCREENINFO, &ctx->lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_VSCREENINFO failed : %s",
                strerror(errno));
        return -1;
    }

    if (ctx->lcd_info.xres == 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "ATTENTION: XRES IS 0");
    }

    if (ctx->lcd_info.yres == 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "ATTENTION: YRES IS 0");
    }

    if (ctx->lcd_info.bits_per_pixel == 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "ATTENTION: BPP IS 0");
    }

    return 0;
}

int fimc_v4l2_set_src(int fd, unsigned int hw_ver, s5p_fimc_img_info *src)
{
    struct v4l2_format  fmt;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers req;

    fmt.fmt.pix.width       = src->full_width;
    fmt.fmt.pix.height      = src->full_height;
    fmt.fmt.pix.pixelformat = src->color_space;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;
    fmt.type                = V4L2_BUF_TYPE_OUTPUT;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::VIDIOC_S_FMT failed : errno=%d (%s)"
                " : fd=%d\n", __func__, errno, strerror(errno), fd);
        return -1;
    }

    /* crop input size */
    crop.type = V4L2_BUF_TYPE_OUTPUT;
    crop.c.width  = src->width;
    crop.c.height = src->height;
    if (0x50 <= hw_ver) {
        crop.c.left   = src->start_x;
        crop.c.top    = src->start_y;
    } else {
        crop.c.left   = 0;
        crop.c.top    = 0;
    }

    if (ioctl(fd, VIDIOC_S_CROP, &crop) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_CROP :"
                "crop.c.left : (%d), crop.c.top : (%d), crop.c.width : (%d), crop.c.height : (%d)",
                __func__, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        return -1;
    }

    /* input buffer type */
    req.count       = 1;
    req.memory      = V4L2_MEMORY_USERPTR;
    req.type        = V4L2_BUF_TYPE_OUTPUT;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in VIDIOC_REQBUFS", __func__);
        return -1;
    }

    return 0;
}

int fimc_v4l2_set_dst(int fd, s5p_fimc_img_info *dst,
        int rotation, int hflip, int vflip, unsigned int addr)
{
    struct v4l2_format      sFormat;
    struct v4l2_control     vc;
    struct v4l2_framebuffer fbuf;
    int ret;

    /* set rotation configuration */
    vc.id = V4L2_CID_ROTATION;
    vc.value = rotation;

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - rotation (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    vc.id = V4L2_CID_HFLIP;
    vc.value = hflip;

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - hflip (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    vc.id = V4L2_CID_VFLIP;
    vc.value = vflip;

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::Error in video VIDIOC_S_CTRL - vflip (%d)"
                "vc.id : (%d), vc.value : (%d)", __func__, ret, vc.id, vc.value);
        return -1;
    }

    /* set size, format & address for destination image (DMA-OUTPUT) */
    ret = ioctl(fd, VIDIOC_G_FBUF, &fbuf);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_G_FBUF (%d)", __func__, ret);
        return -1;
    }

    fbuf.base            = (void *)addr;
    fbuf.fmt.width       = dst->full_width;
    fbuf.fmt.height      = dst->full_height;
    fbuf.fmt.pixelformat = dst->color_space;

    ret = ioctl(fd, VIDIOC_S_FBUF, &fbuf);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_FBUF (%d)", __func__, ret);
        return -1;
    }

    /* set destination window */
    sFormat.type             = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    sFormat.fmt.win.w.left   = dst->start_x;
    sFormat.fmt.win.w.top    = dst->start_y;
    sFormat.fmt.win.w.width  = dst->width;
    sFormat.fmt.win.w.height = dst->height;

    ret = ioctl(fd, VIDIOC_S_FMT, &sFormat);
    if (ret < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_S_FMT (%d)", __func__, ret);
        return -1;
    }

    return 0;
}

int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
    if (-1 == ioctl(fd, VIDIOC_STREAMON, &type)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_STREAMON\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf, enum v4l2_buf_type type, int index)
{
    struct v4l2_buffer buf;
    int ret;

    buf.length      = 0;
    buf.m.userptr   = (unsigned long)fimc_buf;
    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.index       = index;
    buf.type        = type;

    ret = ioctl(fd, VIDIOC_QBUF, &buf);
    if (0 > ret) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_QBUF : (%d)", ret);
        return -1;
    }

    return 0;
}

int fimc_v4l2_dequeue(int fd, struct fimc_buf *fimc_buf, enum v4l2_buf_type type)
{
    struct v4l2_buffer          buf;

    buf.memory      = V4L2_MEMORY_USERPTR;
    buf.type        = type;

    if (-1 == ioctl(fd, VIDIOC_DQBUF, &buf)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_DQBUF\n");
        return -1;
    }

    return buf.index;
}

int fimc_v4l2_stream_off(int fd, enum v4l2_buf_type type)
{
    if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_STREAMOFF\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_clr_buf(int fd, enum v4l2_buf_type type)
{
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.memory  = V4L2_MEMORY_USERPTR;
    req.type    = type;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_REQBUFS");
    }

    return 0;
}

int fimc_v4l2_S_ctrl(int fd)
{
    struct v4l2_control vc;

    vc.id = V4L2_CID_CACHEABLE;
    vc.value = 1;

    if (ioctl(fd, VIDIOC_S_CTRL, &vc) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Error in VIDIOC_S_CTRL");
        return -1;
    }

    return 0;
}

int fimc_handle_oneshot(int fd, struct fimc_buf *fimc_src_buf, struct fimc_buf *fimc_dst_buf)
{
#ifdef CHECK_FPS
    check_fps();
#endif

    if (fimc_v4l2_stream_on(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_stream_on()");
        return -5;
    }

    if (fimc_v4l2_queue(fd, fimc_src_buf, V4L2_BUF_TYPE_OUTPUT, 0) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_queue()");
        goto STREAM_OFF;
    }
    if (fimc_v4l2_dequeue(fd, fimc_src_buf, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_dequeue()");
        return -6;
    }
STREAM_OFF:
    if (fimc_v4l2_stream_off(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC  v4l2_stream_off()");
        return -8;
    }
    if (fimc_v4l2_clr_buf(fd, V4L2_BUF_TYPE_OUTPUT) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Fail : SRC v4l2_clr_buf()");
        return -10;
    }
    return 0;
}

static int memcpy_rect(void *dst, void *src, int fullW, int fullH, int realW, int realH, int format)
{
    unsigned char *srcCb, *srcCr;
    unsigned char *dstCb, *dstCr;
    unsigned char *srcY, *dstY;
    int srcCbOffset, srcCrOffset;
    int dstCbOffset, dstFrameOffset, dstCrOffset;
    int cbFullW, cbRealW, cbFullH, cbRealH;
    int ySrcFW, ySrcFH, ySrcRW, ySrcRH;
    int planes;
    int i;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "++memcpy_rect()::"
            "dst(0x%x),src(0x%x),f.w(%d),f.h(%d),r.w(%d),r.h(%d),format(0x%x)",
            (unsigned int)dst, (unsigned int)src, fullW, fullH, realW, realH, format);

// Set dst Y, Cb, Cr address for FIMC
    {
        cbFullW = fullW >> 1;
        cbRealW = realW >> 1;
        cbFullH = fullH >> 1;
        cbRealH = realH >> 1;
        dstFrameOffset = fullW * fullH;
        dstCrOffset = cbFullW * cbFullH;
        dstY = (unsigned char *)dst;
        dstCb = (unsigned char *)dst + dstFrameOffset;
        dstCr = (unsigned char *)dstCb + dstCrOffset;
    }

// Get src Y, Cb, Cr address for source buffer.
// Each address is aligned by 16's multiple for GPU both width and height.
    {
        ySrcFW = fullW;
        ySrcFH = fullH;
        ySrcRW = realW;
        ySrcRH = realH;
        srcCbOffset = EXYNOS4_ALIGN(ySrcRW,16)* EXYNOS4_ALIGN(ySrcRH,16);
        srcCrOffset = EXYNOS4_ALIGN(cbRealW,16)* EXYNOS4_ALIGN(cbRealH,16);
        srcY =  (unsigned char *)src;
        srcCb = (unsigned char *)src + srcCbOffset;
        srcCr = (unsigned char *)srcCb + srcCrOffset;
    }
    SEC_HWC_Log(HWC_LOG_DEBUG,
            "--memcpy_rect()::\n"
            "dstY(0x%x),dstCb(0x%x),dstCr(0x%x) \n"
            "srcY(0x%x),srcCb(0x%x),srcCr(0x%x) \n"
            "cbRealW(%d),cbRealH(%d)",
            (unsigned int)dstY,(unsigned int)dstCb,(unsigned int)dstCr,
            (unsigned int)srcY,(unsigned int)srcCb,(unsigned int)srcCr,
            cbRealW, cbRealH);

    if (format == HAL_PIXEL_FORMAT_YV12) { //YV12(Y,Cr,Cv)
        planes = 3;
//This is code for VE, deleted temporory by SSONG 2011.09.22
// This will be enabled later.
/*
        //as defined in hardware.h, cb & cr full_width should be aligned to 16. ALIGN(y_stride/2, 16).
        ////Alignment is hard coded to 16.
        ////for example...check frameworks/media/libvideoeditor/lvpp/VideoEditorTools.cpp file for UV stride cal
        cbSrcFW = (cbSrcFW + 15) & (~15);
        srcCbOffset = ySrcFW * fullH;
        srcCrOffset = srcCbOffset + ((cbSrcFW * fullH) >> 1);
        srcY =  (unsigned char *)src;
        srcCb = (unsigned char *)src + srcCbOffset;
        srcCr = (unsigned char *)src + srcCrOffset;
*/
    } else if ((format == HAL_PIXEL_FORMAT_YCbCr_420_P)) {
        planes = 3;
    } else if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP || format == HAL_PIXEL_FORMAT_YCrCb_420_SP) {
        planes = 2;
    } else {
        SEC_HWC_Log(HWC_LOG_ERROR, "use default memcpy instead of memcpy_rect");
        return -1;
    }
//#define CHECK_PERF
#ifdef CHECK_PERF
    struct timeval start, end;
    gettimeofday(&start, NULL);
#endif
    for (i = 0; i < realH; i++)
        memcpy(dstY + fullW * i, srcY + ySrcFW * i, ySrcRW);
    if (planes == 2) {
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCb + ySrcFW * i, srcCb + ySrcFW * i, ySrcRW);
    } else if (planes == 3) {
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCb + cbFullW * i, srcCb + cbFullW * i, cbRealW);
        for (i = 0; i < cbRealH; i++)
            memcpy(dstCr + cbFullW * i, srcCr + cbFullW * i, cbRealW);
    }
#ifdef CHECK_PERF
    gettimeofday(&end, NULL);
    SEC_HWC_Log(HWC_LOG_ERROR, "[COPY]=%d,",(end.tv_sec - start.tv_sec)*1000+(end.tv_usec - start.tv_usec)/1000);
#endif

    return 0;
}

/*****************************************************************************/
static int get_src_phys_addr(struct hwc_context_t *ctx,
        sec_img *src_img, sec_rect *src_rect)
{
    s5p_fimc_t *fimc = &ctx->fimc;

    unsigned int src_virt_addr  = 0;
    unsigned int src_phys_addr  = 0;
    unsigned int src_frame_size = 0;

    ADDRS * addr;

    // error check routine
    if (0 == src_img->base && !(src_img->usage & GRALLOC_USAGE_HW_FIMC1)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s invalid src image base\n", __func__);
        return 0;
    }

    switch (src_img->mem_type) {
    case HWC_PHYS_MEM_TYPE:
        src_phys_addr = src_img->base + src_img->offset;
        break;

    case HWC_VIRT_MEM_TYPE:
    case HWC_UNKNOWN_MEM_TYPE:
        switch (src_img->format) {
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
            addr = (ADDRS *)(src_img->base);
            fimc->params.src.buf_addr_phy_rgb_y = addr->addr_y;
            fimc->params.src.buf_addr_phy_cb    = addr->addr_cbcr;

            src_phys_addr = fimc->params.src.buf_addr_phy_rgb_y;
            if (0 == src_phys_addr) {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s address error "
                        "(format=CUSTOM_YCbCr/YCrCb_420_SP Y-addr=0x%x "
                        "CbCr-Addr=0x%x)",
                        __func__, fimc->params.src.buf_addr_phy_rgb_y,
                        fimc->params.src.buf_addr_phy_cb);
                return 0;
            }
            break;
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
        case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
        case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
            addr = (ADDRS *)(src_img->base + src_img->offset);
            fimc->params.src.buf_addr_phy_rgb_y = addr->addr_y;
            src_phys_addr = fimc->params.src.buf_addr_phy_rgb_y;
            if (0 == src_phys_addr) {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s address error "
                        "(format=CUSTOM_YCbCr/CbYCrY_422_I Y-addr=0x%x)",
                    __func__, fimc->params.src.buf_addr_phy_rgb_y);
                return 0;
            }
            break;
        default:
            if (src_img->usage & GRALLOC_USAGE_HW_FIMC1) {
                fimc->params.src.buf_addr_phy_rgb_y = src_img->paddr;
                fimc->params.src.buf_addr_phy_cb = src_img->paddr + src_img->uoffset;
                fimc->params.src.buf_addr_phy_cr = src_img->paddr + src_img->uoffset + src_img->voffset;
                src_phys_addr = fimc->params.src.buf_addr_phy_rgb_y;
            } else {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s::\nformat = 0x%x : Not "
                        "GRALLOC_USAGE_HW_FIMC1 can not supported\n",
                        __func__, src_img->format);
            }
            break;
        }
    }

    return src_phys_addr;
}

static inline int rotateValueHAL2PP(unsigned char transform)
{
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:  return 90;
    case HAL_TRANSFORM_ROT_180: return 180;
    case HAL_TRANSFORM_ROT_270: return 270;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90: return 90;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90: return 90;
    case HAL_TRANSFORM_FLIP_H: return 0;
    case HAL_TRANSFORM_FLIP_V: return 0;
    }
    return 0;
}

static inline int hflipValueHAL2PP(unsigned char transform)
{
    int flip_flag = transform & 0x7;
    switch (flip_flag) {
    case HAL_TRANSFORM_FLIP_H:
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
        return 1;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_180:
    case HAL_TRANSFORM_ROT_270:
    case HAL_TRANSFORM_FLIP_V:
        break;
    }
    return 0;
}

static inline int vflipValueHAL2PP(unsigned char transform)
{
    int flip_flag = transform & 0x7;
    switch (flip_flag) {
    case HAL_TRANSFORM_FLIP_V:
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
        return 1;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_180:
    case HAL_TRANSFORM_ROT_270:
    case HAL_TRANSFORM_FLIP_H:
        break;
    }
    return 0;
}

static inline int multipleOf2(int number)
{
    if (number % 2 == 1)
        return (number - 1);
    else
        return number;
}

static inline int multipleOf4(int number)
{
    int remain_number = number % 4;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf8(int number)
{
    int remain_number = number % 8;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf16(int number)
{
    int remain_number = number % 16;

    if (remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int widthOfPP(unsigned int ver, int pp_color_format, int number)
{
    if (0x50 <= ver) {
        switch (pp_color_format) {
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
            return multipleOf2(number);

        default :
            return number;
        }
    } else {
        switch (pp_color_format) {
        case V4L2_PIX_FMT_RGB565:
            return multipleOf8(number);

        case V4L2_PIX_FMT_RGB32:
            return multipleOf4(number);

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
            return multipleOf4(number);

        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_NV16:
            return multipleOf8(number);

        case V4L2_PIX_FMT_YUV422P:
            return multipleOf16(number);

        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
            return multipleOf8(number);

        case V4L2_PIX_FMT_YUV420:
            return multipleOf16(number);

        default :
            return number;
        }
    }
    return number;
}

static inline int heightOfPP(int pp_color_format, int number)
{
    switch (pp_color_format) {
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12T:
    case V4L2_PIX_FMT_YUV420:
        return multipleOf2(number);

    default :
        return number;
        break;
    }
    return number;
}

static unsigned int get_yuv_bpp(unsigned int fmt)
{
    int i, sel = -1;

    for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
        if (yuv_list[i].fmt == fmt) {
            sel = i;
            break;
        }
    }

    if (sel == -1)
        return sel;
    else
        return yuv_list[sel].bpp;
}

static unsigned int get_yuv_planes(unsigned int fmt)
{
    int i, sel = -1;

    for (i = 0; i < (int)(sizeof(yuv_list) / sizeof(struct yuv_fmt_list)); i++) {
        if (yuv_list[i].fmt == fmt) {
            sel = i;
            break;
        }
    }

    if (sel == -1)
        return sel;
    else
        return yuv_list[sel].planes;
}

static int runFimcCore(struct hwc_context_t *ctx,
        unsigned int src_phys_addr, sec_img *src_img, sec_rect *src_rect,
        uint32_t src_color_space,
        unsigned int dst_phys_addr, sec_img *dst_img, sec_rect *dst_rect,
        uint32_t dst_color_space, int transform)
{
    s5p_fimc_t        * fimc = &ctx->fimc;
    s5p_fimc_params_t * params = &(fimc->params);

    struct fimc_buf fimc_src_buf;
    int src_bpp, src_planes;

    unsigned int    frame_size = 0;

    bool src_cbcr_order = true;
    int rotate_value = rotateValueHAL2PP(transform);
    int hflip = 0;
    int vflip = 0;

    /* 1. param(fimc config)->src information
     *    - src_img,src_rect => s_fw,s_fh,s_w,s_h,s_x,s_y
     */
    params->src.full_width  = src_img->f_w;
    params->src.full_height = src_img->f_h;
    params->src.width       = src_rect->w;
    params->src.height      = src_rect->h;
    params->src.start_x     = src_rect->x;
    params->src.start_y     = src_rect->y;
    params->src.color_space = src_color_space;
    params->src.buf_addr_phy_rgb_y = src_phys_addr;

    /* check src minimum */
    if (src_rect->w < 16 || src_rect->h < 8) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s src size is not supported by fimc : f_w=%d f_h=%d "
                "x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
                params->src.full_width, params->src.full_height,
                params->src.start_x, params->src.start_y,
                params->src.width, params->src.height,
                src_rect->w, src_rect->h,
                params->src.color_space);
        return -1;
    }

    /* 2. param(fimc config)->dst information
     *    - dst_img,dst_rect,rot => d_fw,d_fh,d_w,d_h,d_x,d_y
     */
    switch (rotate_value) {
    case 0:
        hflip = hflipValueHAL2PP(transform);
        vflip = vflipValueHAL2PP(transform);
        params->dst.full_width  = dst_img->f_w;
        params->dst.full_height = dst_img->f_h;

        params->dst.start_x     = dst_rect->x;
        params->dst.start_y     = dst_rect->y;

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);
        params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);
        break;
    case 90:
        hflip = vflipValueHAL2PP(transform);
        vflip = hflipValueHAL2PP(transform);
        params->dst.full_width  = dst_img->f_h;
        params->dst.full_height = dst_img->f_w;

        params->dst.start_x     = dst_rect->y;
        params->dst.start_y     = dst_img->f_w - (dst_rect->x + dst_rect->w);

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->h);
        params->dst.height      =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);

        if (0x50 > fimc->hw_ver)
            params->dst.start_y     += (dst_rect->w - params->dst.height);
        break;
    case 180:
        params->dst.full_width  = dst_img->f_w;
        params->dst.full_height = dst_img->f_h;

        params->dst.start_x     = dst_img->f_w - (dst_rect->x + dst_rect->w);
        params->dst.start_y     = dst_img->f_h - (dst_rect->y + dst_rect->h);

        params->dst.width       =
            widthOfPP(fimc->hw_ver, dst_color_space, dst_rect->w);
        params->dst.height      = heightOfPP(dst_color_space, dst_rect->h);
        break;
    case 270:
        params->dst.full_width  = dst_img->f_h;
        params->dst.full_height = dst_img->f_w;

        params->dst.start_x     = dst_img->f_h - (dst_rect->y + dst_rect->h);
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

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "runFimcCore()::"
            "SRC f.w(%d),f.h(%d),x(%d),y(%d),w(%d),h(%d)=>"
            "DST f.w(%d),f.h(%d),x(%d),y(%d),w(%d),h(%d)",
            params->src.full_width, params->src.full_height,
            params->src.start_x, params->src.start_y,
            params->src.width, params->src.height,
            params->dst.full_width, params->dst.full_height,
            params->dst.start_x, params->dst.start_y,
            params->dst.width, params->dst.height);

    /* check dst minimum */
    if (dst_rect->w  < 8 || dst_rect->h < 4) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s dst size is not supported by fimc : f_w=%d f_h=%d "
                "x=%d y=%d w=%d h=%d (ow=%d oh=%d) format=0x%x", __func__,
                params->dst.full_width, params->dst.full_height,
                params->dst.start_x, params->dst.start_y,
                params->dst.width, params->dst.height,
                dst_rect->w, dst_rect->h, params->dst.color_space);
        return -1;
    }
    /* check scaling limit
     * the scaling limie must not be more than MAX_RESIZING_RATIO_LIMIT
     */
    if (((src_rect->w > dst_rect->w) &&
                ((src_rect->w / dst_rect->w) > MAX_RESIZING_RATIO_LIMIT)) ||
        ((dst_rect->w > src_rect->w) &&
                ((dst_rect->w / src_rect->w) > MAX_RESIZING_RATIO_LIMIT))) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s over scaling limit : src.w=%d dst.w=%d (limit=%d)",
                __func__, src_rect->w, dst_rect->w, MAX_RESIZING_RATIO_LIMIT);
        return -1;
    }

   /* 3. Set configuration related to destination (DMA-OUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */

    if (fimc_v4l2_set_dst(fimc->dev_fd, &params->dst, rotate_value, hflip, vflip, dst_phys_addr) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "fimc_v4l2_set_dst is failed\n");
        return -1;
    }

   /* 4. Set configuration related to source (DMA-INPUT)
     *   - set input format & size
     *   - crop input size
     *   - set input buffer
     *   - set buffer type (V4L2_MEMORY_USERPTR)
     */
    if (fimc_v4l2_set_src(fimc->dev_fd, fimc->hw_ver, &params->src) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "fimc_v4l2_set_src is failed\n");
        return -1;
    }

    /* 5. Set input dma address (Y/RGB, Cb, Cr)
     *    - zero copy : mfc, camera
     */
    switch (src_img->format) {
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
        /* for video contents zero copy case */
        fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
        fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
        break;

    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_YV12:
    default:
        if (src_img->format == HAL_PIXEL_FORMAT_YV12){
            src_cbcr_order = false;
        }

        if (src_img->usage & GRALLOC_USAGE_HW_FIMC1) {
            fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
            if (src_cbcr_order == true) {
                fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
                fimc_src_buf.base[2] = params->src.buf_addr_phy_cr;
            }
            else {
                fimc_src_buf.base[2] = params->src.buf_addr_phy_cb;
                fimc_src_buf.base[1] = params->src.buf_addr_phy_cr;
            }
            SEC_HWC_Log(HWC_LOG_DEBUG,
                    "runFimcCore - Y=0x%X, U=0x%X, V=0x%X\n",
                    fimc_src_buf.base[0], fimc_src_buf.base[1],fimc_src_buf.base[2]);
            break;
        }
    }

    /* 6. Run FIMC
     *    - stream on => queue => dequeue => stream off => clear buf
     */
    if (fimc_handle_oneshot(fimc->dev_fd, &fimc_src_buf, NULL) < 0) {
        ALOGE("fimcrun fail");            
        fimc_v4l2_clr_buf(fimc->dev_fd, V4L2_BUF_TYPE_OUTPUT);
        return -1;
    }

    return 0;
}

int createFimc(s5p_fimc_t *fimc)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_control vc;

    // open device file
    if (fimc->dev_fd <= 0)
        fimc->dev_fd = open(PP_DEVICE_DEV_NAME, O_RDWR);

    if (fimc->dev_fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Post processor open error (%d)",
                __func__, errno);
        goto err;
    }

    // check capability
    if (ioctl(fimc->dev_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "VIDIOC_QUERYCAP failed");
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%d has no streaming support", fimc->dev_fd);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%d is no video output", fimc->dev_fd);
        goto err;
    }

    /*
     * malloc fimc_outinfo structure
     */
    fmt.type = V4L2_BUF_TYPE_OUTPUT;
    if (ioctl(fimc->dev_fd, VIDIOC_G_FMT, &fmt) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_G_FMT", __func__);
        goto err;
    }

    vc.id = V4L2_CID_FIMC_VERSION;
    vc.value = 0;

    if (ioctl(fimc->dev_fd, VIDIOC_G_CTRL, &vc) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Error in video VIDIOC_G_CTRL", __func__);
        goto err;
    }
    fimc->hw_ver = vc.value;

    return 0;

err:
    if (0 < fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd =0;

    return -1;
}

int destroyFimc(s5p_fimc_t *fimc)
{
    if (fimc->out_buf.virt_addr != NULL) {
        fimc->out_buf.virt_addr = NULL;
        fimc->out_buf.length = 0;
    }

    // close
    if (0 < fimc->dev_fd)
        close(fimc->dev_fd);
    fimc->dev_fd = 0;

    return 0;
}

int runFimc(struct hwc_context_t *ctx,
            struct sec_img *src_img, struct sec_rect *src_rect,
            struct sec_img *dst_img, struct sec_rect *dst_rect,
            uint32_t transform)
{
    s5p_fimc_t *  fimc = &ctx->fimc;

    unsigned int src_phys_addr  = 0;
    unsigned int dst_phys_addr  = 0;
    int          rotate_value   = 0;
    int32_t      src_color_space;
    int32_t      dst_color_space;

    /* 1. source address and size */
    src_phys_addr = get_src_phys_addr(ctx, src_img, src_rect);
    if (0 == src_phys_addr)
        return -1;

    /* 2. destination address and size */
    dst_phys_addr = dst_img->base;
    if (0 == dst_phys_addr)
        return -2;

    /* 3. check whether fimc supports the src format */
    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    if (0 > src_color_space)
        return -3;
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    if (0 > dst_color_space)
        return -4;

    /* 4. FIMC: src_rect of src_img => dst_rect of dst_img */
    if (runFimcCore(ctx, src_phys_addr, src_img, src_rect,
                (uint32_t)src_color_space, dst_phys_addr, dst_img, dst_rect,
                (uint32_t)dst_color_space, transform) < 0)
        return -5;

    return 0;
}

int check_yuv_format(unsigned int color_format) {
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
        return 1;
    default:
        return 0;
    }
}
