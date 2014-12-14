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

/*
 **
 ** @author Hyunkyung, Kim(hk310.kim@samsung.com)
 ** @date   2010-10-13
 */

#define LOG_TAG "libfimc"
#include <cutils/log.h>
#include <errno.h>

#include "SecFimc.h"

#undef BOARD_SUPPORT_SYSMMU

/*------------ DEFINE ---------------------------------------------------------*/
#define  FIMC2_DEV_NAME  "/dev/video2"

#define ALIGN(x, a)    (((x) + (a) - 1) & ~((a) - 1))

//#define DEBUG_LIB_FIMC
struct yuv_fmt_list yuv_list[] = {
    { "V4L2_PIX_FMT_NV12",      "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12,      12, 2 },
    { "V4L2_PIX_FMT_NV12T",     "YUV420/2P/LSB_CBCR",   V4L2_PIX_FMT_NV12T,     12, 2 },
    { "V4L2_PIX_FMT_NV21",      "YUV420/2P/LSB_CRCB",   V4L2_PIX_FMT_NV21,      12, 2 },
    { "V4L2_PIX_FMT_NV21X",     "YUV420/2P/MSB_CBCR",   V4L2_PIX_FMT_NV21X,     12, 2 },
    { "V4L2_PIX_FMT_NV12X",     "YUV420/2P/MSB_CRCB",   V4L2_PIX_FMT_NV12X,     12, 2 },
    { "V4L2_PIX_FMT_YUV420",    "YUV420/3P",            V4L2_PIX_FMT_YUV420,    12, 3 },
    { "V4L2_PIX_FMT_YUYV",      "YUV422/1P/YCBYCR",     V4L2_PIX_FMT_YUYV,      16, 1 },
    { "V4L2_PIX_FMT_YVYU",      "YUV422/1P/YCRYCB",     V4L2_PIX_FMT_YVYU,      16, 1 },
    { "V4L2_PIX_FMT_UYVY",      "YUV422/1P/CBYCRY",     V4L2_PIX_FMT_UYVY,      16, 1 },
    { "V4L2_PIX_FMT_VYUY",      "YUV422/1P/CRYCBY",     V4L2_PIX_FMT_VYUY,      16, 1 },
    { "V4L2_PIX_FMT_UV12",      "YUV422/2P/LSB_CBCR",   V4L2_PIX_FMT_NV16,      16, 2 },
    { "V4L2_PIX_FMT_UV21",      "YUV422/2P/LSB_CRCB",   V4L2_PIX_FMT_NV61,      16, 2 },
    { "V4L2_PIX_FMT_UV12X",     "YUV422/2P/MSB_CBCR",   V4L2_PIX_FMT_NV16X,     16, 2 },
    { "V4L2_PIX_FMT_UV21X",     "YUV422/2P/MSB_CRCB",   V4L2_PIX_FMT_NV61X,     16, 2 },
    { "V4L2_PIX_FMT_YUV422P",   "YUV422/3P",            V4L2_PIX_FMT_YUV422P,   16, 3 },
};

s5p_fimc_t	s5p_fimc;

//unsigned int	fimc_hdmi_ui_layer_status = 0;
unsigned int fimc_reserved_mem_addr = 0;

/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/

void dump_pixfmt(struct v4l2_pix_format *pix)
{
    ALOGI("w: %d\n", pix->width);
    ALOGI("h: %d\n", pix->height);
    ALOGI("color: %x\n", pix->colorspace);
    switch (pix->pixelformat) {
    case V4L2_PIX_FMT_YUYV:
        ALOGI ("YUYV\n");
        break;
    case V4L2_PIX_FMT_UYVY:
        ALOGI ("UYVY\n");
        break;
    case V4L2_PIX_FMT_RGB565:
        ALOGI ("RGB565\n");
        break;
    case V4L2_PIX_FMT_RGB565X:
        ALOGI ("RGB565X\n");
        break;
    default:
        ALOGI("not supported\n");
    }
}

void dump_crop(struct v4l2_crop *crop)
{
    ALOGI("crop l: %d ", crop->c.left);
    ALOGI("crop t: %d ", crop->c.top);
    ALOGI("crop w: %d ", crop->c.width);
    ALOGI("crop h: %d\n", crop->c.height);
}

void dump_window(struct v4l2_window *win)
{
    ALOGI("window l: %d ", win->w.left);
    ALOGI("window t: %d ", win->w.top);
    ALOGI("window w: %d ", win->w.width);
    ALOGI("window h: %d\n", win->w.height);
}

void v4l2_overlay_dump_state(int fd)
{
    struct v4l2_format format;
    struct v4l2_crop crop;
    int ret;

    ALOGI("dumping driver state:");
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret < 0)
        return;
    ALOGI("output pixfmt:\n");
    dump_pixfmt(&format.fmt.pix);

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = ioctl(fd, VIDIOC_G_FMT, &format);
    if (ret < 0)
        return;
    ALOGI("v4l2_overlay window:\n");
    dump_window(&format.fmt.win);

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(fd, VIDIOC_G_CROP, &crop);
    if (ret < 0)
        return;
    ALOGI("output crop:\n");
    dump_crop(&crop);
}

static void error(int fd, const char *msg)
{
    ALOGE("Error = %s from %s", strerror(errno), msg);

#ifdef DEBUG_LIB_FIMC
    v4l2_overlay_dump_state(fd);
#endif
}

int fimc_v4l2_req_buf(int fd, uint32_t *num_bufs, int cacheable_buffers)
{
    //LOG_FUNCTION_NAME

    struct v4l2_requestbuffers reqbuf;
    int ret, i;
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = *num_bufs;


    //reqbuf.reserved[0] = cacheable_buffers;
    ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
    if (ret < 0) {
        error(fd, "reqbuf ioctl");
        return ret;
    }
#ifdef DEBUG_LIB_FIMC
    ALOGI("%d buffers allocated %d requested\n", reqbuf.count, 4);
#endif

    if (reqbuf.count > *num_bufs) {
        error(fd, "Not enough buffer structs passed to get_buffers");
        return -ENOMEM;
    }
    //*num_bufs = reqbuf.count;

#ifdef DEBUG_LIB_FIMC
    ALOGI("buffer cookie is %d\n", reqbuf.type);
#endif

    return 0;
}

int fimc_v4l2_set_src(int fd, unsigned int hw_ver, s5p_fimc_img_info *src)
{
    struct v4l2_format		fmt;
    struct v4l2_cropcap		cropcap;
    struct v4l2_crop		crop;
    struct v4l2_requestbuffers	req;
    int				ret_val;

    /*
     * To set size & format for source image (DMA-INPUT)
     */

    fmt.type 			= V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width 		= src->full_width;
    fmt.fmt.pix.height 		= src->full_height;
    fmt.fmt.pix.pixelformat	= src->color_space;
    fmt.fmt.pix.field 		= V4L2_FIELD_NONE;

    ret_val = ioctl (fd, VIDIOC_S_FMT, &fmt);
    if (ret_val < 0) {
        ALOGE("VIDIOC_S_FMT failed : ret=%d\n", ret_val);
        return -1;
    }

    /*
     * crop input size
     */

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if(hw_ver == 0x50) {
        crop.c.left = src->start_x;
        crop.c.top = src->start_y;
    } else {
        crop.c.left   = 0;
        crop.c.top    = 0;
    }

    crop.c.width  = src->width;
    crop.c.height = src->height;

    ret_val = ioctl(fd, VIDIOC_S_CROP, &crop);
    if (ret_val < 0) {
        ALOGE("Error in video VIDIOC_S_CROP (%d)\n",ret_val);
        return -1;
    }

    return ret_val;
}

int fimc_v4l2_set_dst(int fd, s5p_fimc_img_info *dst, int rotation, unsigned int addr)
{
    struct v4l2_format      sFormat;
    struct v4l2_control     vc;
    struct v4l2_framebuffer fbuf;
    int                     ret_val;

    /*
     * set rotation configuration
     */

    vc.id = V4L2_CID_ROTATION;
    vc.value = rotation;

    ret_val = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret_val < 0) {
        ALOGE("Error in video VIDIOC_S_CTRL - rotation (%d)\n",ret_val);
        return -1;
    }

    /*
     * set size, format & address for destination image (DMA-OUTPUT)
     */
    ret_val = ioctl(fd, VIDIOC_G_FBUF, &fbuf);
    if (ret_val < 0) {
        ALOGE("Error in video VIDIOC_G_FBUF (%d)\n", ret_val);
        return -1;
    }

/* RyanJung modify for testing 2010-11-17*/
#if 0//BOARD_SUPPORT_SYSMMU
    unsigned int secure_id;
    int ret;
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_GET_UMP_SECURE_ID;
    ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
    if (ret < 0) {
        ALOGE("ERR(%s):VIDIOC_G_CTRL failed\n", __func__);
        return ret;
    }
    // Temporary use fimc1 output buffer.
    fbuf.base 			= (void *)ctrl.id;
#else
    fbuf.base 			= (void *)addr;
#endif
    fbuf.fmt.width 		= dst->full_width;
    fbuf.fmt.height 		= dst->full_height;
    fbuf.fmt.pixelformat 	= dst->color_space;

    ret_val = ioctl (fd, VIDIOC_S_FBUF, &fbuf);
    if (ret_val < 0) {
        ALOGE("Error in video VIDIOC_S_FBUF (%d)\n",ret_val);
        return -1;
    }

    /*
     * set destination window
     */

    sFormat.type 		= V4L2_BUF_TYPE_VIDEO_OVERLAY;
    sFormat.fmt.win.w.left 	= dst->start_x;
    sFormat.fmt.win.w.top 	= dst->start_y;
    sFormat.fmt.win.w.width 	= dst->width;
    sFormat.fmt.win.w.height 	= dst->height;

    ret_val = ioctl(fd, VIDIOC_S_FMT, &sFormat);
    if (ret_val < 0) {
        ALOGE("Error in video VIDIOC_S_FMT (%d)\n",ret_val);
        return -1;
    }

    return 0;
}

int fimc_v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
    if (-1 == ioctl (fd, VIDIOC_STREAMON, &type)) {
        ALOGE("Error in VIDIOC_STREAMON\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_queue(int fd, struct fimc_buf *fimc_buf, int index)
{
    struct v4l2_buffer	buf;
    int			ret_val;

    buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory	= V4L2_MEMORY_USERPTR;
    buf.m.userptr	= (unsigned long)fimc_buf;
    buf.length	= 0;
    buf.index	= index;

    ret_val = ioctl (fd, VIDIOC_QBUF, &buf);
    if (0 > ret_val) {
        ALOGE("Error in VIDIOC_QBUF : (%d) \n", ret_val);
        return -1;
    }

    return 0;
}

int fimc_v4l2_dequeue(int fd)
{
    struct v4l2_buffer	buf;

    buf.type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory      = V4L2_MEMORY_USERPTR;

    if (-1 == ioctl (fd, VIDIOC_DQBUF, &buf)) {
        ALOGE("Error in VIDIOC_DQBUF\n");
        return -1;
    }

    return buf.index;
}

int fimc_v4l2_stream_off(int fd)
{
    enum v4l2_buf_type	type;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (-1 == ioctl (fd, VIDIOC_STREAMOFF, &type)) {
        ALOGE("Error in VIDIOC_STREAMOFF\n");
        return -1;
    }

    return 0;
}

int fimc_v4l2_clr_buf(int fd)
{
    struct v4l2_requestbuffers req;

    req.count   = 0;
    req.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory  = V4L2_MEMORY_USERPTR;

    if (ioctl (fd, VIDIOC_REQBUFS, &req) == -1) {
        ALOGE("Error in VIDIOC_REQBUFS");
    }

    return 0;
}

static inline int multipleOf2(int number)
{
    if(number % 2 == 1)
        return (number - 1);
    else
        return number;
}

static inline int multipleOf4(int number)
{
    int remain_number = number % 4;

    if(remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf8(int number)
{
    int remain_number = number % 8;

    if(remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

static inline int multipleOf16(int number)
{
    int remain_number = number % 16;

    if(remain_number != 0)
        return (number - remain_number);
    else
        return number;
}

int fimc_switch_color_format(int color_space)
{
    switch(color_space)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            return V4L2_PIX_FMT_RGB32;
            break;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            return V4L2_PIX_FMT_RGB32;
            //return V4L2_PIX_FMT_BGR32;
            break;

        case HAL_PIXEL_FORMAT_RGBA_4444:
            return V4L2_PIX_FMT_RGB444;
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
            return V4L2_PIX_FMT_RGB565;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            return V4L2_PIX_FMT_NV61;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            return V4L2_PIX_FMT_NV21;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_P:
            return V4L2_PIX_FMT_YUV422P;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_420_P:
            return V4L2_PIX_FMT_YUV420;
            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            return V4L2_PIX_FMT_YUYV;
            break;

        case HAL_PIXEL_FORMAT_CbYCrY_422_I:
            return V4L2_PIX_FMT_UYVY;
            break;

        case HAL_PIXEL_FORMAT_CUSTOM_CbYCr_422_I:
            return V4L2_PIX_FMT_UYVY;
            break;

        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I: 
            return V4L2_PIX_FMT_YUYV;
            break;

        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
            return V4L2_PIX_FMT_NV12T;
            break;

        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
            return V4L2_PIX_FMT_NV21;
            break;

        case V4L2_PIX_FMT_RGB32:
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_YUV422P:
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_NV12T:
            return color_space;
            break;

        default:
            return -1;
            break;
    }

    return -1;
}

SecFimc::SecFimc()
:   mFlagCreate(false),
    mFimcDev(FIMC_DEV1),
    mFimcOvlyMode(FIMC_OVLY_NOT_FIXED),
    mFimcRrvedPhysMemAddr(0x0),
    mBufNum(0),
    mBufIndex(0),
    mRotVal(0),
    mFlagGlobalAlpha(false),
    mGlobalAlpha(0x0),
    mFlagLocalAlpha(false),
    mFlagColorKey(false),
    mColorKey(0x0),
    mFlagSetSrcParam(false),
    mFlagSetDstParam(false),
    mFlagStreamOn(false)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    mS5pFimc.dev_fd = 0;
    memset(&mS5pFimc.out_buf, 0, sizeof(struct fimc_buffer));
    memset(&mS5pFimc.params, 0, sizeof(s5p_fimc_params_t));
    mS5pFimc.use_ext_out_mem = 0;
    mS5pFimc.hw_ver = 0;
}

SecFimc::~SecFimc()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == true)
    {
        ALOGE("%s::this is not Destroyed fail \n", __func__);
        destroy();
    }
}

bool SecFimc::create(FIMC_DEV fimc_dev, 
        fimc_overlay_mode fimc_mode, unsigned int buf_num)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fimc_dev(%d)", __func__, unsigned int(fimc_dev));
#endif

    if(mFlagCreate == true)
    {
        ALOGE("%s:: Already Created fail \n", __func__);
        return false;
    }
    
    char node[20];
    struct v4l2_format fmt;
    struct v4l2_control     vc;
    int ret, index;

    // Initialize varialbles
    mFimcDev = FIMC_DEV1;
    mFimcOvlyMode = FIMC_OVLY_NOT_FIXED;
    mFimcRrvedPhysMemAddr = 0x0;
    //mFlagCreate = false;
    mBufNum = buf_num;
    mBufIndex = 0;
    mRotVal = 0;
    mFlagGlobalAlpha = false;
    mGlobalAlpha = 0x0;
    mFlagLocalAlpha = false;
    mFlagColorKey = false;
    mColorKey = 0x0;
    mFlagSetSrcParam = false;
    mFlagSetDstParam = false;
    mFlagStreamOn = false;

    mS5pFimc.dev_fd = 0;
    memset(&mS5pFimc.out_buf, 0, sizeof(struct fimc_buffer));
    memset(&mS5pFimc.params, 0, sizeof(s5p_fimc_params_t));
    mS5pFimc.use_ext_out_mem = 0;
    mS5pFimc.hw_ver = 0;

    sprintf(node, "%s%d", PFX_NODE_FIMC, (int)fimc_dev);


    /* open device file */
    if(mS5pFimc.dev_fd == 0) {
        mS5pFimc.dev_fd = open(node, O_RDWR);
        if(mS5pFimc.dev_fd < 0) {
            ALOGE("%s:: %s Post processor open error\n", __func__, node);
            return false;
        }
    }
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fd = %d", __func__, (int)mS5pFimc.dev_fd);
#endif

    /* check capability */
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_QUERYCAP, &mFimcCap);
    if (ret < 0) {
        ALOGE("VIDIOC_QUERYCAP failed\n");
        return false;
    }

    if (!(mFimcCap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s has no streaming support\n", node);
        return false;
    }

    if (!(mFimcCap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        ALOGE("%s is no video output\n", node);
        return false;
    }

    /*
     * malloc fimc_outinfo structure
     */
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        ALOGE("%s:: Error in video VIDIOC_G_FMT\n", __FUNCTION__);
        return false;
    }

    /*
     * get baes address of the reserved memory for fimc2
     */
    vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
    vc.value = 0;

    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Error in video VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)\n", ret);
        return false;
    }

    mFimcRrvedPhysMemAddr = (unsigned int)vc.value;
    ALOGI("%s:: Fimc reserved memory =0x%8x\n", __func__, mFimcRrvedPhysMemAddr);
    mS5pFimc.out_buf.phys_addr = (void *)mFimcRrvedPhysMemAddr;
    
    vc.id = V4L2_CID_FIMC_VERSION;
    vc.value = 0;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Error in video VIDIOC_G_CTRL - V4L2_CID_FIMC_VERSION (%d), FIMC version is set with default\n", ret);
        vc.value = 0x43;
    }
    mS5pFimc.hw_ver = vc.value;

    mFimcOvlyMode = fimc_mode;
    vc.id = V4L2_CID_OVLY_MODE;
    vc.value = fimc_mode;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_S_CTRL, &vc);
    if (ret < 0) {
        ALOGE("Error in video VIDIOC_S_CTRL - V4L2_CID_OVLY_MODE (%d)\n",ret);
        mFimcOvlyMode = FIMC_OVLY_NOT_FIXED;
    }

    mFlagCreate = true;

    return true;
}

bool SecFimc::destroy()
{
    if(mFlagCreate == false)
    {
        ALOGE("%s:: Already Destroyed fail \n", __func__);
        return false;
    }

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(fimc_v4l2_clr_buf(mS5pFimc.dev_fd) < 0)
    {
        ALOGE("%s :: fimc_v4l2_clr_buf() fail", __func__);
        return false;
    }

    if(mS5pFimc.out_buf.phys_addr != NULL) {
        mS5pFimc.out_buf.phys_addr = NULL;
        mS5pFimc.out_buf.length = 0;
    }

    /* close */
    if(mS5pFimc.dev_fd != 0)
        close(mS5pFimc.dev_fd);

    mFlagCreate = false;

    return true;
}

bool SecFimc::flagCreate()
{
    return mFlagCreate;
}

int SecFimc::getSecFimcFd()
{
    return mS5pFimc.dev_fd;
}


int SecFimc::getFimcRsrvedPhysMemAddr()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: mFimcRrvedPhysMemAddr = 0x%8x", 
            __func__, mFimcRrvedPhysMemAddr);
#endif
    if(mFlagCreate == false)
        ALOGE("%s:: libFimc is not created", __func__);
    return mFimcRrvedPhysMemAddr; 
}

int SecFimc::getFimcVersion()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fimc version = 0x%x", __func__, mS5pFimc.hw_ver);
#endif
    if(mFlagCreate == false)
        ALOGE("%s:: libFimc is not created", __func__);
    return mS5pFimc.hw_ver; 
}

bool SecFimc::checkFimcSrcSize(unsigned int width, unsigned int height, 
        unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
        int colorFormat, bool forceChange)
{
    bool ret = true;

    if(height >= 8 && *cropHeight <8)
    {
        if(forceChange)
            *cropHeight = 8;
        ret = false;
    }
    if(width >= 16 && *cropWidth <16)
    {
        if(forceChange)
            *cropWidth = 16;
        ret = false;
    }

    if(0x50 == mS5pFimc.hw_ver)
    {
        if(colorFormat == V4L2_PIX_FMT_YUV422P)
        {
            if(*cropHeight % 2 != 0)
            {
                if(forceChange)
                    *cropHeight = multipleOf2(*cropHeight);
                ret = false;
            }
            if(*cropWidth % 2 != 0)
            {
                if(forceChange)
                    *cropWidth = multipleOf2(*cropWidth);
                ret = false;
            }
        }
    }
    else
    {
        if(height < 8)
            return false;
        if(width%16 != 0)
            return false;
        if(*cropWidth % 16 != 0)
        {
            if(forceChange)
                *cropWidth = multipleOf16(*cropWidth);
            ret = false;
        }
    }
    return ret;
}

bool SecFimc::checkFimcDstSize(unsigned int width, unsigned int height, 
        unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
        int colorFormat, int rotVal,  bool forceChange)
{
    bool ret = true;
    unsigned int rotWidth;
    unsigned int rotHeight;
    unsigned int* rotCropWidth;
    unsigned int* rotCropHeight;
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: format= %d, width = %, height= %d, cropW= %d, cropH= %d",
            __func__, colorFormat, width, height, *cropWidth, *cropHeight);
#endif
    if(rotVal == 90 || rotVal == 270)
    {
        rotWidth = height;
        rotHeight = width;
        rotCropWidth = cropHeight;
        rotCropHeight = cropWidth;
    }else{
        rotWidth = width;
        rotHeight = height;
        rotCropWidth = cropWidth;
        rotCropHeight = cropHeight;
    }

    if(rotHeight < 8)
        return false;
    if(rotWidth % 8 != 0)
        return false;

    switch(colorFormat) {
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_YUV420:
            if(*rotCropHeight % 2 != 0)
            {
                if(forceChange)
                    *rotCropHeight = multipleOf2(*rotCropHeight);
                ret = false;
            }
    }
    return ret;
}

bool SecFimc::setSrcParams(unsigned int width, unsigned int height, 
        unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
        int colorFormat, bool forceChange)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("%s :: libFimc is not created", __func__);
        return false;
    }

    int fimcColorFormat = fimc_switch_color_format(colorFormat);
    s5p_fimc_params_t* params = &(mS5pFimc.params);
    unsigned int fimcWidth  = *cropWidth;
    unsigned int fimcHeight = *cropHeight;
    checkFimcSrcSize(width, height, cropX, cropY, &fimcWidth, &fimcHeight, 
            fimcColorFormat, true);

    if(fimcColorFormat < 0)
    {
        ALOGE("%s:: not supported color format", __func__);
        return false;
    }

    if(fimcWidth != *cropWidth || fimcHeight != *cropHeight)
    {
        if(forceChange){
#ifdef DEBUG_LIB_FIMC
            ALOGD("size is changed from [w = %d, h= %d] to [w = %d, h = %d]",
                    *cropWidth, *cropHeight, fimcWidth, fimcHeight);
#endif
        }
        else{
            ALOGE("%s :: invalid source params", __func__);
            return false;
        }
    }


#if 1
    if(   (params->src.full_width == width) && (params->src.full_height == height) 
       && (params->src.start_x == cropX) &&  (params->src.start_y == cropY) 
       && (params->src.width == fimcWidth) && (params->src.height == fimcHeight) 
       && (params->src.color_space == (unsigned int)fimcColorFormat))
        return true;
#endif

    params->src.full_width  = width;
    params->src.full_height = height;
    params->src.start_x     = cropX;
    params->src.start_y     = cropY;
    params->src.width       = fimcWidth;
    params->src.height      = fimcHeight;
    params->src.color_space = fimcColorFormat;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fd = %d", __func__, (int)mS5pFimc.dev_fd);
    ALOGD("%s:: full_width = %d, full_height = %d, x = %d, y = %d, width = %d, height = %d", 
            __func__, params->src.full_width, params->src.full_height, 
            params->src.start_x, params->src.start_y, params->src.width, params->src.height);
#endif

    if(mFlagSetSrcParam == true)
    {
        if(fimc_v4l2_clr_buf(mS5pFimc.dev_fd) < 0)
        {
            ALOGE("%s:: fimc_v4l2_clr_buf() fail", __func__);
            return false;
        }
    }
    
#ifdef DEBUG_LIB_FIMC
    ALOGD("fimc_v4l2_set_src is called");
#endif
    if(fimc_v4l2_set_src(mS5pFimc.dev_fd, mS5pFimc.hw_ver, &(params->src)) < 0)
    {
        ALOGE("%s:: fimc_v4l2_set_src() fail", __func__);
        return false;
    }

    if(fimc_v4l2_req_buf(mS5pFimc.dev_fd, &mBufNum, 0) < 0)
    {
        ALOGE("%s:: fimc_v4l2_req_buf() fail", __func__);
        return false;
    }

    *cropWidth  = fimcWidth;
    *cropHeight = fimcHeight;

    mFlagSetSrcParam = true;
    return true;
}

bool SecFimc::getSrcParams(unsigned int* width, unsigned int* height, 
        unsigned int* cropX, unsigned int* cropY, unsigned int* cropWidth, 
        unsigned int* cropHeight, int* colorFormat)
{
    struct v4l2_format fmt;
    struct v4l2_crop crop;
    int ret;
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0) {
        ALOGE("%s:: Error in video VIDIOC_G_FMT\n", __func__);
        return false;
    }

    *width = fmt.fmt.pix.width;
    *height = fmt.fmt.pix.height;
    *colorFormat = fmt.fmt.pix.pixelformat;

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_CROP, &crop);
    if (ret < 0)
    {
        ALOGE("%s:: Error in video VIDIOC_G_CROP\n", __func__);
        return false;
    }

    *cropX = crop.c.left;
    *cropY = crop.c.top;
    *cropWidth = crop.c.width;
    *cropHeight = crop.c.height;
    return true;
}

bool SecFimc::setSrcPhyAddr(unsigned int physYAddr, 
        unsigned int physCbAddr, unsigned int physCrAddr, int colorFormat)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: physYAddr = 0x%8x, physCbAddr = 0x%8x",
            __func__, physYAddr, physCbAddr);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("%s :: libFimc is not created", __func__);
        return false;
    }

    s5p_fimc_params_t *params = &(mS5pFimc.params);
    unsigned int src_planes = m_get_yuv_planes(params->src.color_space);
    unsigned int src_bpp = m_get_yuv_bpp(params->src.color_space);
    unsigned int frame_size = params->src.full_width * params->src.full_height;

    params->src.buf_addr_phy_rgb_y = physYAddr;
    params->src.buf_addr_phy_cb = physCbAddr;
    params->src.buf_addr_phy_cr = physCrAddr;

    if(src_planes >= 2 &&  params->src.buf_addr_phy_cb == 0)
    {
        params->src.buf_addr_phy_cb = params->src.buf_addr_phy_rgb_y + frame_size;
    }
    if(src_planes == 3 &&  params->src.buf_addr_phy_cr == 0)
    {
        if(12 == src_bpp)
            params->src.buf_addr_phy_cr = params->src.buf_addr_phy_cb + (frame_size >> 2);
        else
            params->src.buf_addr_phy_cr = params->src.buf_addr_phy_cb + (frame_size >> 1);
    }

    return true;
}

bool SecFimc::setDstParams(unsigned int width, unsigned int height, 
        unsigned int cropX, unsigned int cropY, unsigned int* cropWidth, unsigned int* cropHeight, 
        int colorFormat, bool forceChange)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("[%s] libFimc is not created", __func__);
        return false;
    }

    unsigned int fimcWidth = *cropWidth;
    unsigned int fimcHeight = *cropHeight;
    int ret_val;
    struct v4l2_framebuffer fbuf;
    struct v4l2_format sFormat;

    int fimcColorFormat = fimc_switch_color_format(colorFormat);
    s5p_fimc_params_t* params = &(mS5pFimc.params);
    checkFimcDstSize(width, height, 
            cropX, cropY, &fimcWidth, &fimcHeight, fimcColorFormat, mRotVal, true);

    if(fimcColorFormat < 0)
    {
        ALOGE("%s:: not supported color format", __func__);
        return false;
    }

    params->dst.color_space = fimcColorFormat;

    if(fimcWidth != *cropWidth || fimcHeight != *cropHeight)
    {
        if(forceChange){
#ifdef DEBUG_LIB_FIMC
            ALOGD("size is changed from [w = %d, h= %d] to [w = %d, h = %d]",
                    *cropWidth, *cropHeight, fimcWidth, fimcHeight);
#endif
        }
        else{
            ALOGE("%s :: invalid destination params", __func__);
            return false;
        }
    }

    if (90 == mRotVal || 270 == mRotVal) {
        params->dst.full_width  = height;
        params->dst.full_height = width;

        if( 90 == mRotVal ) {
            params->dst.start_x     = cropY;
            params->dst.start_y     = width - (cropX + fimcWidth);
        } else {
            params->dst.start_x = height - (cropY + fimcHeight);
            params->dst.start_y = cropX;
        }

        params->dst.width       = fimcHeight;
        params->dst.height      = fimcWidth;

        // work-around
        if(0x50 != mS5pFimc.hw_ver)
            params->dst.start_y     += (fimcWidth - params->dst.height);

    } else {
        params->dst.full_width  = width;
        params->dst.full_height = height;

        if( 180 == mRotVal ) {
            params->dst.start_x = width - (cropX + fimcWidth);
            params->dst.start_y = height - (cropY + fimcHeight);
        } else {
            params->dst.start_x     = cropX;
            params->dst.start_y     = cropY;
        }

        params->dst.width       = fimcWidth;
        params->dst.height      = fimcHeight;
    }

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fd = %d", __func__, (int)mS5pFimc.dev_fd);
    ALOGD("%s:: full_width = %d, full_height = %d, x= %d, y= %d, width = %d, height = %d", 
            __func__, params->dst.full_width, params->dst.full_height, 
            params->dst.start_x, params->dst.start_y, params->dst.width, params->dst.height);
#endif

#ifdef DEBUG_LIB_FIMC
    ALOGD("fimc_v4l2_set_dst is called");
#endif
    if(fimc_v4l2_set_dst(mS5pFimc.dev_fd, &(params->dst), 
                mRotVal, (unsigned int)mS5pFimc.out_buf.phys_addr) < 0)
    {
        ALOGE("%s :: fimc_v4l2_set_dst", __func__);
        return false;
    }

    *cropWidth  = fimcWidth;
    *cropHeight = fimcHeight;

    mFlagSetDstParam = true;

    return true;
}

bool SecFimc::getDstParams(unsigned int* width, unsigned int* height, 
        unsigned int* cropX, unsigned int* cropY, 
        unsigned int* cropWidth, unsigned int* cropHeight, int* colorFormat)
{
    struct v4l2_framebuffer		fbuf;
    struct v4l2_format format;
    int ret;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FBUF, &fbuf);
    if (ret < 0) {
        ALOGE("Error in video VIDIOC_G_FBUF (%d)\n", ret);
        return false;
    }

    *width       = fbuf.fmt.width;
    *height      = fbuf.fmt.height;
    *colorFormat = fbuf.fmt.pixelformat;

    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FMT, &format);
    if (ret < 0)
    {
        ALOGE("Error in video VIDIOC_G_FMT (%d)\n", ret);
        return false;
    }
    *cropX = format.fmt.win.w.left;
    *cropY = format.fmt.win.w.top;
    *cropWidth = format.fmt.win.w.width;
    *cropHeight = format.fmt.win.w.height;

    return true;
}

bool SecFimc::setDstPhyAddr(unsigned int physYAddr, unsigned int physCbAddr, unsigned int physCrAddr)
{
    struct v4l2_framebuffer fbuf;
    s5p_fimc_params_t* params = &(mS5pFimc.params);
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: physYAddr = 0x%x, physCbAddr = 0x%x",
            __func__, physYAddr, physCbAddr);
#endif

    if(mFlagCreate == false)
        ALOGE("[%s] libFimc is not created", __func__);

    mS5pFimc.out_buf.phys_addr = (void *)physYAddr;

    if((physYAddr != 0) && ((unsigned int)mS5pFimc.out_buf.phys_addr != mFimcRrvedPhysMemAddr))
        mS5pFimc.use_ext_out_mem = 1;

#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: fd = %d", __func__, (int)mS5pFimc.dev_fd);
    ALOGD("fimc_v4l2_set_dst is called");
#endif
    if(fimc_v4l2_set_dst(mS5pFimc.dev_fd, &(params->dst), mRotVal, 
                (unsigned int)mS5pFimc.out_buf.phys_addr) < 0)
    {
        ALOGE("%s :: fimc_v4l2_set_dst", __func__);
        return false;
    }

    return true;
}

bool SecFimc::setRotVal(unsigned int rotVal)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: rotVal = %d", __func__, rotVal);
#endif
    struct v4l2_control vc;
    int ret_val;

    if(mFlagCreate == false)
    {
        ALOGE("[%s] libFimc is not created", __func__);
        return false;
    }
    if(mFlagStreamOn == true)
    {
        ALOGE("[%s] Fimc stream on", __func__);
        return false;
    }

    /*
     * set rotation configuration
     */
    vc.id = V4L2_CID_ROTATION;
    vc.value = rotVal;

    ret_val = ioctl(mS5pFimc.dev_fd, VIDIOC_S_CTRL, &vc);
    if (ret_val) {
        ALOGE("Error in video VIDIOC_S_CTRL - rotation (%d)\n", rotVal);
        return false;
    }

    mRotVal = rotVal;
    return true;
}

bool SecFimc::setGlobalAlpha(bool enable, int alpha)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: enable= %d, alpha = %d", __func__, (int)enable, alpha);
#endif
    int ret;
    struct v4l2_framebuffer fbuf;
    struct v4l2_format fmt;

    if(mFlagCreate == false)
    {
        ALOGE("[%s] libFimc is not created", __func__);
        return false;
    }
    if(mFlagStreamOn == true)
    {
        ALOGE("[%s] Fimc stream on", __func__);
        return false;
    }

    if(mFlagGlobalAlpha == enable && mGlobalAlpha == alpha)
        return true;

    memset(&fbuf, 0, sizeof(fbuf));
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FBUF, &fbuf);

    if (ret)
    {
        ALOGE("%s::VIDIOC_G_FBUF fail\n", __func__);
        return false;
    }

    if (enable)
        fbuf.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
    else
        fbuf.flags &= ~V4L2_FBUF_FLAG_GLOBAL_ALPHA;

    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_S_FBUF, &fbuf);

    if (ret)
    {
        ALOGE("%s::VIDIOC_S_FBUF fail\n", __func__);
        return false;
    }

    if (enable) {
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
        ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FMT, &fmt);

        if (ret)
        {
            ALOGE("%s::VIDIOC_G_FMT fail\n", __func__);
            return false;
        }

        fmt.fmt.win.global_alpha = alpha & 0xFF;
        ret = ioctl(mS5pFimc.dev_fd, VIDIOC_S_FMT, &fmt);
        if (ret)
        {
            ALOGE("%s::VIDIOC_S_FMT fail\n", __func__);
            return false;
        }
    }
    mFlagGlobalAlpha = enable;
    mGlobalAlpha     = alpha;
    return true;

}
bool SecFimc::setLocalAlpha(bool enable)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: enable= %d", __func__, (int)enable);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("[%s] libFimc is not created", __func__);
        return false;
    }
    if(mFlagStreamOn == true)
    {
        ALOGE("[%s] Fimc stream on", __func__);
        return false;
    }
    if(mFlagLocalAlpha == enable)
        return true;
#if 0
    int ret;
    struct v4l2_framebuffer fbuf;

    ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FBUF, &fbuf,
            "get transparency enables");

    if (ret)
    {
        ALOGE("%s::VIDIOC_G_FBUF fail\n", __func__);
        return ret;
    }

    if (enable)
        fbuf.flags |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
    else
        fbuf.flags &= ~V4L2_FBUF_FLAG_LOCAL_ALPHA;

    ret = ioctl(fd, VIDIOC_S_FBUF, &fbuf);
    mFlagLoalAlpha = enable;
    mLocalAlpha = alpha;
#else
    return true;
#endif
}

bool SecFimc::setColorKey(bool enable, int colorKey)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s :: enable= %d, colorKey = 0x%8x", __func__, (int)enable, colorKey);
#endif
    int ret;
    struct v4l2_framebuffer fbuf;
    struct v4l2_format fmt;

    if(mFlagCreate == false)
    {
        ALOGE("[%s] libFimc is not created", __func__);
        return false;
    }
    if(mFlagStreamOn == true)
    {
        ALOGE("[%s] Fimc stream on", __func__);
        return false;
    }
    if(mFlagColorKey == enable && mColorKey == colorKey)
        return true;

    memset(&fbuf, 0, sizeof(fbuf));
    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FBUF, &fbuf);

    if (ret)
    {
        ALOGE("%s::VIDIOC_G_FBUF fail\n", __func__);
        return false;
    }

    if (enable)
        fbuf.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
    else
        fbuf.flags &= ~V4L2_FBUF_FLAG_CHROMAKEY;

    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_S_FBUF, &fbuf);

    if (ret)
    {
        ALOGE("%s::VIDIOC_S_FBUF fail\n", __func__);
        return false;
    }

    if (enable) {
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
        ret = ioctl(mS5pFimc.dev_fd, VIDIOC_G_FMT, &fmt);

        if (ret)
        {
            ALOGE("%s::VIDIOC_G_FMT fail\n", __func__);
            return false;
        }

        fmt.fmt.win.chromakey = colorKey & 0xFFFFFF;

        ret = ioctl(mS5pFimc.dev_fd, VIDIOC_S_FMT, &fmt);
        if (ret)
        {
            ALOGE("%s::VIDIOC_S_FMT fail\n", __func__);
            // no return??
        }
    }
    mFlagColorKey = enable;
    mColorKey = colorKey;
    return true;
}

bool SecFimc::streamOn()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
        ALOGE("[%s] libFimc is not created", __func__);

    if(mFlagStreamOn == true)
        return true;

    if(fimc_v4l2_stream_on(mS5pFimc.dev_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT)< 0)
    {
        ALOGE("%s::fimc_v4l2_stream_on(V4L2_BUF_TYPE_VIDEO_OUTPUT) fail\n", __func__);
        return false;
    }

    mFlagStreamOn = true;

    return true;
}

bool SecFimc::streamOff()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
        ALOGE("[%s] libFimc is not created", __func__);

    if(mFlagStreamOn == false)
        return true;

    if(fimc_v4l2_stream_off(mS5pFimc.dev_fd) < 0)
    {
        ALOGE("%s::fimc_v4l2_stream_off() fail\n", __func__);
        return false;
    }

    mFlagStreamOn = false;

    return true;
}

bool SecFimc::queryBuffer(int index, struct v4l2_buffer *buf)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: index = %d", __func__, index);
#endif
    if(mFlagCreate == false)
        ALOGE("[%s] libFimc is not created", __func__);

    if((index < 0) || (index >= (int)mBufNum))
    {
        ALOGE("[%s] queryBuffer index is invalid (%d)", __func__, index);
    }

    memset(buf, 0, sizeof(struct v4l2_buffer));
    buf->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf->memory = V4L2_MEMORY_USERPTR;
    buf->index = index;

    if(ioctl(mS5pFimc.dev_fd, VIDIOC_QUERYBUF, buf) < 0)
    {
        ALOGE("%s::VIDIOC_QUERYBUF fail\n", __func__);
        return false;
    }
    return true;
}

bool SecFimc::queueBuffer(int index)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: index = %d", __func__, index);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("%s :: libFimc is not created", __func__);
        return false;
    }

    if((index < 0) || (index >= (int)mBufNum))
    {
        ALOGE("[%s] queueBuffer index is invalid (index : %d)(mBufNum : %d)",
            __func__, index, mBufNum);
    }

    s5p_fimc_params_t* params = &(mS5pFimc.params);
    struct fimc_buf fimc_src_buf;
    if(mFlagSetSrcParam == false)
    {
        ALOGE("%s :: source params are not set", __func__);
        return false;
    }
    if(mFlagSetDstParam == false)
    {
        ALOGE("%s :: destination params are not set", __func__);
        return false;
    }

    fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
    fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
    fimc_src_buf.base[2] = params->src.buf_addr_phy_cr;

    if(fimc_v4l2_queue(mS5pFimc.dev_fd, &fimc_src_buf, index) < 0)
    {
        ALOGE("%s :: fimc_v4l2_queue(index : %d) (mBufNum : %d) fail",
            __func__, index, mBufNum);
        return false;
    }
    return true;
}

int  SecFimc::dequeueBuffer(int* index)
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("%s :: libFimc is not created", __func__);
        return false;
    }

    s5p_fimc_params_t* params = &(mS5pFimc.params);
    struct v4l2_buffer buf;
    int ret;

    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory = V4L2_MEMORY_USERPTR;

    ret = ioctl(mS5pFimc.dev_fd, VIDIOC_DQBUF, &buf);
    if (ret < 0)
    {
        ALOGE("%s::VIDIOC_DQBUF fail\n", __func__);
        return false;
    }

    *index = buf.index;
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s:: index = %d", __func__, *index);
#endif
    return true;

}

bool SecFimc::handleOneShot()
{
#ifdef DEBUG_LIB_FIMC
    ALOGD("%s", __func__);
#endif
    if(mFlagCreate == false)
    {
        ALOGE("%s :: libFimc is not created", __func__);
        return false;
    }
    s5p_fimc_params_t* params = &(mS5pFimc.params);
    struct fimc_buf fimc_src_buf;

    if(mFlagSetSrcParam == false)
    {
        ALOGE("%s :: source params are not set", __func__);
        return false;
    }
    if(mFlagSetDstParam == false)
    {
        ALOGE("%s :: destination params are not set", __func__);
        return false;
    }

    fimc_src_buf.base[0] = params->src.buf_addr_phy_rgb_y;
    fimc_src_buf.base[1] = params->src.buf_addr_phy_cb;
    fimc_src_buf.base[2] = params->src.buf_addr_phy_cr;
    
    if(fimc_v4l2_stream_on(mS5pFimc.dev_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT)< 0)
    {
        ALOGE("%s :: fimc_v4l2_stream_on(V4L2_BUF_TYPE_VIDEO_OUTPUTd) fail", __func__);
        return false;
    }

    if(fimc_v4l2_queue(mS5pFimc.dev_fd, &fimc_src_buf, 0) < 0)
    {
        ALOGE("%s :: fimc_v4l2_queue(index : %d) (mBufNum : %d) fail", __func__, 0, mBufNum);
        goto STREAMOFF;
    }

    if(fimc_v4l2_dequeue(mS5pFimc.dev_fd) < 0)
    {
        ALOGE("%s :: fimc_v4l2_dequeue (mBufNum : %d) fail", __func__, mBufNum);
        goto STREAMOFF;
    }

STREAMOFF:

    if(fimc_v4l2_stream_off(mS5pFimc.dev_fd) < 0)
    {
        ALOGE("%s :: fimc_v4l2_stream_off() fail", __func__);
        return false;
    }

    return true;
}

int SecFimc::m_widthOfFimc(int fimc_color_format, int width)
{
    if (0x50 == mS5pFimc.hw_ver) {
        switch(fimc_color_format) {
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
            return multipleOf2(width);

        default :
            return width;
        }
    } else {
        switch(fimc_color_format) {
        case V4L2_PIX_FMT_RGB565:
            return multipleOf8(width);

        case V4L2_PIX_FMT_RGB32:
            return multipleOf4(width);

        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
            return multipleOf4(width);

        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_NV16:
            return multipleOf8(width);

        case V4L2_PIX_FMT_YUV422P:
            return multipleOf16(width);

        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
            return multipleOf8(width);

        case V4L2_PIX_FMT_YUV420:
            return multipleOf16(width);

        default :
            return width;
        }
    }
    return width;
}

int SecFimc::m_heightOfFimc(int fimc_color_format, int height)
{
    switch(fimc_color_format) {
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_YUV420:
            return multipleOf2(height);

    default :
        return height;
    }
    return height;
}

unsigned int SecFimc::m_get_yuv_bpp(unsigned int fmt)
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

unsigned int SecFimc::m_get_yuv_planes(unsigned int fmt)
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