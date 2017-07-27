/*
 * Copyright 2011, Havlena Petr <havlenapetr@gmail.com>
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

#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>

#include <sec_lcd.h>

#include "SecHDMI.h"
#include "fimd.h"

using namespace android;

#define RETURN_IF(return_value)                                      \
    if (return_value < 0) {                                          \
        ALOGE("%s::%d fail. errno: %s",                              \
             __func__, __LINE__, strerror(errno));                   \
        return -1;                                                   \
    }

#define ALOG_IF(return_value)                                        \
    if (return_value < 0) {                                          \
        ALOGE("%s::%d fail. errno: %s",                              \
             __func__, __LINE__, strerror(errno));                   \
    }

#define ALIGN_TO_32B(x)     ((((x) + (1 <<  5) - 1) >>  5) <<  5)
#define ALIGN_TO_128B(x)    ((((x) + (1 <<  7) - 1) >>  7) <<  7)
#define ALIGN_TO_8KB(x)     ((((x) + (1 << 13) - 1) >> 13) << 13)

struct s5p_tv_standart_internal {
    int index;
    unsigned long value;
} s5p_tv_standards[] = {
    {
        S5P_TV_STD_NTSC_M,
        V4L2_STD_NTSC_M,
    }, {
        S5P_TV_STD_PAL_BDGHI,
        V4L2_STD_PAL_BDGHI,
    }, {
        S5P_TV_STD_PAL_M,
        V4L2_STD_PAL_M,
    }, {
        S5P_TV_STD_PAL_N,
        V4L2_STD_PAL_N,
    }, {
        S5P_TV_STD_PAL_Nc,
        V4L2_STD_PAL_Nc,
    }, {
        S5P_TV_STD_PAL_60,
        V4L2_STD_PAL_60,
    }, {
        S5P_TV_STD_NTSC_443,
        V4L2_STD_NTSC_443,
    }, {
        S5P_TV_STD_480P_60_16_9,
        V4L2_STD_480P_60_16_9,
    }, {
        S5P_TV_STD_480P_60_4_3,
        V4L2_STD_480P_60_4_3,
    }, {
        S5P_TV_STD_576P_50_16_9,
        V4L2_STD_576P_50_16_9,
    }, {
        S5P_TV_STD_576P_50_4_3,
        V4L2_STD_576P_50_4_3,
    }, {
        S5P_TV_STD_720P_60,
        V4L2_STD_720P_60,
    }, {
        S5P_TV_STD_720P_50,
        V4L2_STD_720P_50,
    },
};

static inline int calcFrameSize(int format, int width, int height)
{
    int size = 0;
    
    switch (format) {
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
            size = (width * height * 3 / 2);
            break;
            
        case V4L2_PIX_FMT_NV12T:
            size = ALIGN_TO_8KB(ALIGN_TO_128B(width) * ALIGN_TO_32B(height)) +
            ALIGN_TO_8KB(ALIGN_TO_128B(width) * ALIGN_TO_32B(height / 2));
            break;
            
        case V4L2_PIX_FMT_YUV422P:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
            size = (width * height * 2);
            break;
            
        default :
            ALOGE("ERR(%s):Invalid V4L2 pixel format(%d)\n", __func__, format);
        case V4L2_PIX_FMT_RGB565:
            size = (width * height * 2);
            break;
    }
    return size;
}

static int get_pixel_depth(unsigned int fmt)
{
    int depth = 0;
    
    switch (fmt) {
        case V4L2_PIX_FMT_NV12:
            depth = 12;
            break;
        case V4L2_PIX_FMT_NV12T:
            depth = 12;
            break;
        case V4L2_PIX_FMT_NV21:
            depth = 12;
            break;
        case V4L2_PIX_FMT_YUV420:
            depth = 12;
            break;
            
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YVYU:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_YUV422P:
            depth = 16;
            break;
            
        case V4L2_PIX_FMT_RGB32:
            depth = 32;
            break;
    }
    
    return depth;
}

// ======================================================================
// Video ioctls

static int tv20_v4l2_querycap(int fp)
{
    struct v4l2_capability cap;
    
    int ret = ioctl(fp, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_QUERYCAP failed", __func__);
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        ALOGE("ERR(%s):no output devices\n", __func__);
        return -1;
    }
    ALOGV("Name of cap driver is %s", cap.driver);
    
    return ret;
}

static const __u8* tv20_v4l2_enum_output(int fp, int index)
{
    static struct v4l2_output output;
    
    output.index = index;
    if (ioctl(fp, VIDIOC_ENUMOUTPUT, &output) != 0) {
        ALOGE("ERR(%s):No matching index found", __func__);
        return NULL;
    }
    ALOGV("Name of output channel[%d] is %s", output.index, output.name);
    
    return output.name;
}

static const __u8* tv20_v4l2_enum_standarts(int fp, int index)
{
    static struct v4l2_standard standart;
    
    standart.index = index;
    if (ioctl(fp, VIDIOC_ENUMSTD, &standart) != 0) {
        ALOGE("ERR(%s):No matching index found\n", __func__);
        return NULL;
    }
    ALOGV("Name of output standart[%d] is %s\n", standart.index, standart.name);
    
    return standart.name;
}

static int tv20_v4l2_s_output(int fp, int index)
{
    struct v4l2_output output;
    int ret;
    
    output.index = index;
    
    ret = ioctl(fp, VIDIOC_S_OUTPUT, &output);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_OUPUT failed\n", __func__);
        return ret;
    }
    return ret;
}

static int tv20_v4l2_s_std(int fp, unsigned long id)
{
    v4l2_std_id std;
    int ret;
    
    std = id;
    
    ret = ioctl(fp, VIDIOC_S_STD, &std);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_OUPUT failed\n", __func__);
        return ret;
    }
    return ret;
}

static int tv20_v4l2_enum_fmt(int fp, unsigned int fmt)
{
    struct v4l2_fmtdesc fmtdesc;
    int found = 0;
    
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmtdesc.index = 0;
    
    while (ioctl(fp, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        if (fmtdesc.pixelformat == fmt) {
            ALOGV("passed fmt = %#x found pixel format[%d]: %s\n", fmt, fmtdesc.index, fmtdesc.description);
            found = 1;
            break;
        }
        
        fmtdesc.index++;
    }
    
    if (!found) {
        ALOGE("unsupported pixel format\n");
        return -1;
    }
    
    return 0;
}

static int tv20_v4l2_s_fmt(int fp, int width, int height,
                           unsigned int fmt, unsigned int yAddr, unsigned int cAddr)
{
    struct v4l2_format v4l2_fmt;
    struct v4l2_pix_format_s5p_tvout pixfmt;
    int ret;

    v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
#if 0
    ret = ioctl(fp, VIDIOC_G_FMT, &v4l2_fmt);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_G_FMT failed", __func__);
        return -1;
    }
#endif

    memset(&pixfmt, 0, sizeof(pixfmt));
    pixfmt.pix_fmt.width = width;
    pixfmt.pix_fmt.height = height;
    pixfmt.pix_fmt.pixelformat = fmt;
    pixfmt.pix_fmt.sizeimage = (width * height * get_pixel_depth(fmt)) / 8;
    pixfmt.pix_fmt.field = V4L2_FIELD_NONE;

    // here we must set addresses of our memory for video out
    pixfmt.base_y = (void *)yAddr;
    pixfmt.base_c = (void* )cAddr;

    v4l2_fmt.fmt.pix = pixfmt.pix_fmt;
    memcpy(v4l2_fmt.fmt.raw_data, &pixfmt,
           sizeof(struct v4l2_pix_format_s5p_tvout));

    /* Set up for capture */
    ret = ioctl(fp, VIDIOC_S_FMT, &v4l2_fmt);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_FMT failed\n", __func__);
        return -1;
    }
    return 0;
}

static int tv20_v4l2_streamon(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;
    
    ret = ioctl(fp, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMON failed\n", __func__);
        return ret;
    }
    
    return ret;
}

static int tv20_v4l2_streamoff(int fp)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int ret;
    
    ALOGV("%s :", __func__);
    ret = ioctl(fp, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_STREAMOFF failed\n", __func__);
        return ret;
    }
    
    return ret;
}

static int tv20_v4l2_g_parm(int fp, struct v4l2_streamparm *streamparm)
{
    int ret;
    
    streamparm->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    
    ret = ioctl(fp, VIDIOC_G_PARM, streamparm);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_G_PARM failed\n", __func__);
        return -1;
    }
    
    ALOGV("%s : timeperframe: numerator %d, denominator %d\n", __func__,
         streamparm->parm.capture.timeperframe.numerator,
         streamparm->parm.capture.timeperframe.denominator);
    
    return 0;
}

static int tv20_v4l2_s_parm(int fp, struct v4l2_streamparm *streamparm)
{
    int ret;
    
    streamparm->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    
    ret = ioctl(fp, VIDIOC_S_PARM, streamparm);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_PARM failed\n", __func__);
        return ret;
    }
    return 0;
}

static int tv20_v4l2_s_crop(int fp, int offset_x, int offset_y, int width, int height)
{
    struct v4l2_crop crop;
    
    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    crop.c.left    = offset_x;
    crop.c.top = offset_y;
    crop.c.width = width;
    crop.c.height = height;
    
    int ret = ioctl(fp, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_S_PARM failed\n", __func__);
        return ret;
    }
    return 0;
}

static int tv20_v4l2_start_overlay(int fp)
{
    int ret, start = 1;
    
    ret = ioctl(fp, VIDIOC_OVERLAY, &start);
    if (ret < 0) {
        ALOGE("ERR(%s): VIDIOC_OVERLAY start failed\n", __func__);
        return ret;
    }
    
    return ret;
}

static int tv20_v4l2_stop_overlay(int fp)
{
    int ret, stop = 0;
    
    ret = ioctl(fp, VIDIOC_OVERLAY, &stop);
    if (ret < 0) {
        ALOGE("ERR(%s): VIDIOC_OVERLAY stop failed\n", __func__);
        return ret;
    }
    
    return ret;
}

static int tv20_v4l2_s_baseaddr(int fp, void *base_addr)
{
    int ret;

    ret = ioctl(fp, S5PTVFB_WIN_SET_ADDR, base_addr);
    if (ret < 0) {
        ALOGE("ERR(%s): VIDIOC_S_BASEADDR failed %d", __func__, ret);
        return ret;
    }

    return 0;
}

static int tv20_v4l2_s_position(int fp, int x, int y)
{
    int ret;
    struct s5ptvfb_user_window window;

    memset(&window, 0, sizeof(struct s5ptvfb_user_window));
    window.x = x;
    window.y = y;

    ret = ioctl(fp, S5PTVFB_WIN_POSITION, &window);
    if (ret < 0) {
        ALOGE("ERR(%s): VIDIOC_S_WIN_POSITION failed %d", __func__, ret);
        return ret;
    }

    return 0;
}

// ======================================================================
// Audio ioctls

static int tv20_v4l2_audio_enable(int fp)
{
    return ioctl(fp, VIDIOC_INIT_AUDIO, 1);
}

static int tv20_v4l2_audio_disable(int fp)
{
    return ioctl(fp, VIDIOC_INIT_AUDIO, 0);
}

static int tv20_v4l2_audio_mute(int fp)
{
    return ioctl(fp, VIDIOC_AV_MUTE, 1);
}

static int tv20_v4l2_audio_unmute(int fp)
{
    return ioctl(fp, VIDIOC_AV_MUTE, 0);
}

static int tv20_v4l2_audio_get_mute_state(int fp)
{
    return ioctl(fp, VIDIOC_G_AVMUTE, 0);
}

// ======================================================================
// Class which comunicate with kernel driver

SecHDMI::SecHDMI()
    : mTvOutFd(-1),
      mTvOutVFd(-1),
      mLcdFd(-1),
      mHdcpEnabled(0),
      mFlagConnected(false)
{
    ALOGV("%s", __func__);

    memset(&mParams, 0, sizeof(struct v4l2_streamparm));
    memset(&mFlagLayerEnable, 0, sizeof(bool) * S5P_TV_LAYER_MAX);

    int ret = ioctl(mTvOutFd, VIDIOC_HDCP_ENABLE, &mHdcpEnabled);
    ALOG_IF(ret);
}

SecHDMI::~SecHDMI()
{
    destroy();
}

/* static */
int SecHDMI::getCableStatus()
{
    int  fd = 0;
    char value[8] = {0};

    ALOGV("%s", __func__);

    fd = open("/sys/class/switch/h2w/state", O_RDWR);
    if(fd < 0) {
        goto close;
    }

    if(read(fd, &value, 8) <= 0) {
        goto close;
    }

close:
    close(fd);
    return strtol(value, NULL, 10);
}

const __u8* SecHDMI::getName(int index)
{
    ALOGV("%s", __func__);
    return tv20_v4l2_enum_output(mTvOutFd, index);
}

int SecHDMI::destroy()
{
    ALOGV("%s", __func__);

    if(mFlagConnected) {
        disconnect();
    }
    if(mTvOutFd > 0) {
        close(mTvOutFd);
        mTvOutFd = -1;
    }
    if(mFimc.dev_fd > 0) {
        fimc_close(&mFimc);
        mFimc.dev_fd = -1;
    }
    if (mLcdFd > 0) {
        fb_close(mLcdFd);
        mLcdFd = -1;
    }

    return 0;
}

int SecHDMI::startLayer(s5p_tv_layer layer)
{
    int ret;

    if (mFlagLayerEnable[layer]) {
        return 0;
    }

    switch (layer) {
    case S5P_TV_LAYER_VIDEO:
        if(mTvOutVFd < 0) {
            mTvOutVFd = open(TVOUT_DEV_V, O_RDWR);
            RETURN_IF(mTvOutVFd);
        }
        ret = tv20_v4l2_start_overlay(mTvOutVFd);
        RETURN_IF(ret);
        break;
    case S5P_TV_LAYER_GRAPHIC_0 :
        ret = ioctl(0/*fp_tvout_g0*/, FBIOBLANK, (void *)FB_BLANK_UNBLANK);
        RETURN_IF(ret);
        break;
    case S5P_TV_LAYER_GRAPHIC_1 :
        ret = ioctl(0/*fp_tvout_g1*/, FBIOBLANK, (void *)FB_BLANK_UNBLANK);
        RETURN_IF(ret);
        break;
    default :
        RETURN_IF(-1);
    }

    mFlagLayerEnable[layer] = true;

    return 0;
}

int SecHDMI::stopLayer(s5p_tv_layer layer)
{
    int ret;

    if (!mFlagLayerEnable[layer]) {
        return 0;
    }

    switch (layer) {
    case S5P_TV_LAYER_VIDEO:
        ret = tv20_v4l2_stop_overlay(mTvOutVFd);
        RETURN_IF(ret);
        close(mTvOutVFd);
        mTvOutVFd = -1;
        break;
    case S5P_TV_LAYER_GRAPHIC_0 :
        ret = ioctl(0/*fp_tvout_g0*/, FBIOBLANK, (void *)FB_BLANK_POWERDOWN);
        RETURN_IF(ret);
        break;
    case S5P_TV_LAYER_GRAPHIC_1 :
        ret = ioctl(0/*fp_tvout_g1*/, FBIOBLANK, (void *)FB_BLANK_POWERDOWN);
        RETURN_IF(ret);
        break;
    default :
        RETURN_IF(-1);
    }

    mFlagLayerEnable[layer] = false;

    return 0;
}

int SecHDMI::create(int width, int height)
{
    int ret, y_size;
    unsigned int addr;

    ALOGV("%s", __func__);

    mTvOutFd = open(TVOUT_DEV, O_RDWR);
    RETURN_IF(mTvOutFd);

    memset(&mFimc, 0, sizeof(s5p_fimc_t));
    mFimc.dev_fd = -1;
    ret = fimc_open(&mFimc, "/dev/video2");
    RETURN_IF(ret);

    ALOGV("query capabilities");
    ret = tv20_v4l2_querycap(mTvOutFd);
    RETURN_IF(ret);

    struct s5p_tv_standart_internal std =
            s5p_tv_standards[(int) S5P_TV_STD_PAL_BDGHI];

    ALOGV("searching for standart: %i", std.index);
    if(!tv20_v4l2_enum_standarts(mTvOutFd, std.index))
        return -1;

    ret = tv20_v4l2_s_std(mTvOutFd, std.value);
    RETURN_IF(ret);

    ALOGV("searching for output: %i", S5P_TV_OUTPUT_TYPE_COMPOSITE);
    if (!tv20_v4l2_enum_output(mTvOutFd, S5P_TV_OUTPUT_TYPE_COMPOSITE))
        return -1;

    ret = tv20_v4l2_s_output(mTvOutFd, S5P_TV_OUTPUT_TYPE_COMPOSITE);
    RETURN_IF(ret);

    struct v4l2_window_s5p_tvout* p =
            (struct v4l2_window_s5p_tvout*)&mParams.parm.raw_data;
    p->win.w.top = 0;
    p->win.w.left = 0;
    p->win.w.width = width;
    p->win.w.height = height;

    ALOGV("searching for format: %i", V4L2_PIX_FMT_NV12);
    ret = tv20_v4l2_enum_fmt(mTvOutFd, V4L2_PIX_FMT_NV12);
    RETURN_IF(ret);

    addr = (unsigned int) mFimc.out_buf.phys_addr;
    y_size = ALIGN_TO_8KB(ALIGN_TO_128B(width) * ALIGN_TO_32B(height));
    ret = tv20_v4l2_s_fmt(mTvOutFd, width, height, V4L2_PIX_FMT_NV12, 
                          (unsigned int) addr,
                          (unsigned int) addr + y_size);
    RETURN_IF(ret);

    return 0;
}

int SecHDMI::connect()
{
    int ret;

    ALOGV("%s", __func__);

    RETURN_IF(mTvOutFd);

    if(mFlagConnected) {
        return 0;
    }

#if 0
    ret = getCableStatus() <= 0 ? -1 : 0;
    RETURN_IF(ret);
#endif

    ret = tv20_v4l2_s_parm(mTvOutFd, &mParams);
    RETURN_IF(ret);

    ret = tv20_v4l2_streamon(mTvOutFd);
    RETURN_IF(ret);

#if 0
    ret = startLayer(S5P_TV_LAYER_VIDEO);
    RETURN_IF(ret);
#endif

    mFlagConnected = true;

    return 0;
}

int SecHDMI::disconnect()
{
    int ret;

    ALOGV("%s", __func__);

    RETURN_IF(mTvOutFd);

    if(!mFlagConnected) {
        return 0;
    }

    ret = tv20_v4l2_streamoff(mTvOutFd);
    RETURN_IF(ret);

#if 0
    ret = stopLayer(S5P_TV_LAYER_VIDEO);
    RETURN_IF(ret);
#endif

    mFlagConnected = false;

    return 0;
}

int SecHDMI::flush(int srcW, int srcH, int srcColorFormat,
                   unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
                   int dstX, int dstY,
                   int layer,
                   int num_of_hwc_layer)
{
    int ret;

#if 0
    usleep(1000 * 10);
#else
    sec_img         src_img;
    sec_img         dst_img;
    sec_rect        src_rect;
    sec_rect        dst_rect;
    unsigned int    phyAddr[3/*MAX_NUM_PLANES*/];

    if(!srcYAddr) {
        struct s3cfb_next_info fb_info;

        if (mLcdFd < 0) {
            mLcdFd = fb_open(0);
        }

        RETURN_IF(mLcdFd);

        ret = ioctl(mLcdFd, S3CFB_GET_CURR_FB_INFO, &fb_info);
        RETURN_IF(ret);

        srcYAddr = fb_info.phy_start_addr;
        srcCbAddr = srcYAddr;
    }

    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(src_img));
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(src_rect));
    memset(&phyAddr, 0, sizeof(int) * sizeof(phyAddr));

    phyAddr[0] = srcYAddr;
    phyAddr[1] = srcCbAddr;
    phyAddr[2] = srcCrAddr;

    src_img.w       = srcW;
    src_img.h       = srcH;
    src_img.format  = HAL_PIXEL_FORMAT_YCbCr_420_SP/*srcColorFormat*/;
    src_img.base    = 0;
    src_img.offset  = 0;
    src_img.mem_id  = 0;
    src_img.mem_type = FIMC_MEM_TYPE_PHYS;
    src_img.w       = (src_img.w + 15) & (~15);
    src_img.h       = (src_img.h + 1)  & (~1) ;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.w = src_img.w;
    src_rect.h = src_img.h;

    struct v4l2_window_s5p_tvout* p =
        (struct v4l2_window_s5p_tvout*)&mParams.parm.raw_data;
    if (!p) {
        return -1;
    }

    dst_img.w       = p->win.w.width;
    dst_img.h       = p->win.w.height;
    dst_img.format  = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    dst_img.base    = (unsigned int) mFimc.out_buf.phys_addr;
    dst_img.offset  = 0;
    dst_img.mem_id  = 0;
    dst_img.mem_type = FIMC_MEM_TYPE_PHYS;

    dst_rect.x = p->win.w.top;
    dst_rect.y = p->win.w.left;
    dst_rect.w = dst_img.w;
    dst_rect.h = dst_img.h;

    ALOGV("%s::sr_x %d sr_y %d sr_w %d sr_h %d dr_x %d dr_y %d dr_w %d dr_h %d ",
          __func__, src_rect.x, src_rect.y, src_rect.w, src_rect.h,
          dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h);

    ret = fimc_flush(&mFimc, &src_img, &src_rect, &dst_img, &dst_rect,
                     phyAddr, 0);
    RETURN_IF(ret);

/*
    struct fb_var_screeninfo var;
    var.xres = srcW;
    var.yres = srcH;
    var.xres_virtual = var.xres;
    var.yres_virtual = var.yres;
    var.xoffset = 0;
    var.yoffset = 0;
    var.width = srcW;
    var.height = srcH;
    var.activate = FB_ACTIVATE_FORCE;
    if (srcColorFormat == HAL_PIXEL_FORMAT_RGB_565) {
        var.bits_per_pixel = 16;
        var.transp.length = 0;
    }
    else {
        var.bits_per_pixel = 32;
        var.transp.length = 8;
    }

    ret = tv20_v4l2_s_baseaddr(mTvOutFd, (void *)srcYAddr);
    RETURN_IF(ret);

    ret = fb_put_vscreeninfo(mLcdFd, &var);
    RETURN_IF(ret);

    ret = tv20_v4l2_s_position(mTvOutFd, dstX, dstY);
    RETURN_IF(ret);
*/
#endif

    return 0;
}
