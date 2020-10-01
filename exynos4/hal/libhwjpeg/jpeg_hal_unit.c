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

#define LOG_TAG "libhwjpeg"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <sys/poll.h>

#include <cutils/log.h>

#include "jpeg_hal.h"

#ifdef JPEG_PERF_MEAS
unsigned long measure_time(struct timeval *start, struct timeval *stop)
{
    unsigned long sec, usec, time;

    sec = stop->tv_sec - start->tv_sec;

    if (stop->tv_usec >= start->tv_usec) {
        usec = stop->tv_usec - start->tv_usec;
    } else {
        usec = stop->tv_usec + 1000000 - start->tv_usec;
        sec--;
    }

    time = (sec * 1000000) + usec;

    return time;
}
#endif

static int jpeg_v4l2_querycap(int fd)
{
    struct v4l2_capability cap;
    int ret = 0;

    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        ALOGE("[%s]: does not support streaming", __func__);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))
        ALOGE("[%s]: does not support output", __func__);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        ALOGE("[%s]: does not support capture", __func__);

    return ret;
}

static int jpeg_v4l2_s_jpegcomp(int fd, int quality)
{
    struct v4l2_jpegcompression arg;
    int ret = 0;

    arg.quality = quality;

    ret = ioctl(fd, VIDIOC_S_JPEGCOMP, &arg);

    return ret;
}

static int jpeg_v4l2_s_fmt(int fd, enum v4l2_buf_type type, struct jpeg_config *config)
{
    struct v4l2_format fmt;
    int ret = 0;

    fmt.type = type;
    fmt.fmt.pix_mp.width = config->width;
    fmt.fmt.pix_mp.height = config->height;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.num_planes = config->num_planes;

    if (config->mode == JPEG_ENCODE)
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:    /* fall through */
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        if (config->mode == JPEG_ENCODE) {
            fmt.fmt.pix_mp.pixelformat = config->pix.enc_fmt.in_fmt;
        } else {
            fmt.fmt.pix_mp.pixelformat = config->pix.dec_fmt.in_fmt;
            fmt.fmt.pix_mp.plane_fmt[0].sizeimage = config->sizeJpeg;
        }
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        if (config->mode == JPEG_ENCODE) {
            fmt.fmt.pix_mp.pixelformat = config->pix.enc_fmt.out_fmt;
        } else {
            fmt.fmt.pix_mp.pixelformat = config->pix.dec_fmt.out_fmt;
            fmt.fmt.pix_mp.width = config->scaled_width;
            fmt.fmt.pix_mp.height = config->scaled_height;
        }
        break;
    default:
            ALOGE("[%s]: invalid v4l2 buf type", __func__);
            return -1;
    }

    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);

    return ret;
}

static int jpeg_v4l2_g_fmt(int fd, enum v4l2_buf_type type, struct jpeg_config *config)
{
    struct v4l2_format fmt;
    int ret = 0;

    fmt.type = type;
    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0)
        return -1;

    config->width = fmt.fmt.pix_mp.width;
    config->height = fmt.fmt.pix_mp.height;

    switch (fmt.type) {
    case V4L2_BUF_TYPE_VIDEO_OUTPUT:    /* fall through */
    case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        break;
    case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
        if (config->mode == JPEG_ENCODE)
            config->pix.enc_fmt.in_fmt = fmt.fmt.pix_mp.pixelformat;
        else
            config->pix.dec_fmt.in_fmt = fmt.fmt.pix_mp.pixelformat;
        break;
    case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
        if (config->mode == JPEG_ENCODE)
            config->pix.enc_fmt.out_fmt = fmt.fmt.pix_mp.pixelformat;
        else
            config->pix.dec_fmt.out_fmt = fmt.fmt.pix_mp.pixelformat;
        break;
    default:
        ALOGE("[%s]: invalid v4l2 buf type", __func__);
        return -1;
    }

    return ret;
}

int jpeghal_getconfig(int fd, struct jpeg_config *config)
{
    int ret = 0;

    ret = jpeg_v4l2_g_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, config);
    if (ret < 0) {
        ALOGE("[%s]: input G_FMT failed", __func__);
        return -1;
    }

    ret = jpeg_v4l2_g_fmt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, config);
    if (ret < 0)
        ALOGE("[%s]: output G_FMT failed", __func__);

    return ret;
}

static int jpeg_v4l2_reqbufs(int fd, int buf_cnt, struct jpeg_buf *buf)
{
    struct v4l2_requestbuffers req;
    int ret = 0;

    memset(&req, 0, sizeof(req));

    req.type = buf->buf_type;
    req.memory = buf->memory;

    req.count = buf_cnt;

    ret = ioctl(fd, VIDIOC_REQBUFS, &req);

    return ret;
}

static int jpeg_v4l2_querybuf(int fd, struct jpeg_buf *buf)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane plane[JPEG_MAX_PLANE_CNT];
    int i;
    int ret = 0;

    memset(plane, 0, (int)JPEG_MAX_PLANE_CNT * sizeof(struct v4l2_plane));

    v4l2_buf.index = 0;
    v4l2_buf.type = buf->buf_type;
    v4l2_buf.memory = buf->memory;
    v4l2_buf.length = buf->num_planes;
    v4l2_buf.m.planes = plane;

    ret = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf);
    if (ret < 0) {
        ALOGE("[%s:%d]: VIDIOC_QUERYBUF failed", __func__, ret);
        return ret;
    }

    for (i= 0; i < buf->num_planes; i++) {
        buf->length[i] = v4l2_buf.m.planes[i].length;
        buf->start[i] = (char *) mmap(0, buf->length[i],
                    PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                    v4l2_buf.m.planes[i].m.mem_offset);

        //ALOGI("[%s]: buf.start[%d] = %p, length = %d", __func__, 0, buf->start[0], buf->length[0]);
        if (buf->start[0] == MAP_FAILED) {
            ALOGE("[%s]: mmap failed", __func__);
            return -1;
        }
    }

    return ret;
}

static int jpeg_v4l2_qbuf(int fd, struct jpeg_buf *buf)
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane plane[JPEG_MAX_PLANE_CNT];
    int i;
    int ret = 0;

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(plane, 0, (int)JPEG_MAX_PLANE_CNT * sizeof(struct v4l2_plane));

    v4l2_buf.index = 0;
    v4l2_buf.type = buf->buf_type;
    v4l2_buf.memory = buf->memory;
    v4l2_buf.length = buf->num_planes;
    v4l2_buf.m.planes = plane;

    if (buf->memory == V4L2_MEMORY_USERPTR) {
        for (i = 0; i < buf->num_planes; i++) {
            v4l2_buf.m.planes[i].m.userptr = (unsigned long)buf->start[i];
            v4l2_buf.m.planes[i].length = buf->length[i];
        }
    }

    ret = ioctl(fd, VIDIOC_QBUF, &v4l2_buf);
    if (ret < 0) {
        ALOGE("[%s:%d] QBUF failed", __func__, ret);
        return -1;
    }

    return ret;
}

static int jpeg_v4l2_dqbuf(int fd, enum v4l2_buf_type type, enum v4l2_memory memory)
{
    struct v4l2_buffer buf;
    int ret = 0;

    memset(&buf, 0, sizeof(struct v4l2_buffer));

    buf.type = type;
    buf.memory = memory;

    ret = ioctl(fd, VIDIOC_DQBUF, &buf);
    if (ret < 0) {
        ALOGE("[%s:%d] DQBUF failed", __func__, ret);
        return -1;
    }

    return ret;
}

static int jpeg_v4l2_streamon(int fd, enum v4l2_buf_type type)
{
    int ret = 0;

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        ALOGE("[%s:%d] STREAMON failed", __func__, ret);
        return -1;
    }

    return ret;
}

static int jpeg_v4l2_streamoff(int fd, enum v4l2_buf_type type)
{
    int ret = 0;

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        ALOGE("[%s:%d] STREAMOFF failed", __func__, ret);
        return -1;
    }

    return ret;
}

int jpeghal_dec_init()
{
    int fd;
    int ret = 0;

    fd = open(JPEG_DEC_NODE, O_RDWR, 0);

    if (fd < 0) {
        ALOGE("[%s]: JPEG dec open failed", __func__);
        return -1;
    }

    ret = jpeg_v4l2_querycap(fd);
    if (ret < 0) {
        ALOGE("[%s]: QUERYCAP failed", __func__);
        return -1;
    }

    return fd;
}

int jpeghal_enc_init()
{
    int fd;
    int ret = 0;

    fd = open(JPEG_ENC_NODE, O_RDWR, 0);
    if (fd < 0) {
        ALOGE("[%s]: JPEG enc open failed", __func__);
        return -1;
    }

    ret = jpeg_v4l2_querycap(fd);
    if (ret < 0) {
        ALOGE("[%s]: QUERYCAP failed", __func__);
        return -1;
    }

    return fd;
}

int jpeghal_dec_setconfig(int fd, struct jpeg_config *config)
{
    int ret = 0;

    config->mode = JPEG_DECODE;

    ret = jpeg_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, config);
    if (ret < 0) {
        ALOGE("[%s]: decoder input S_FMT failed", __func__);
        return -1;
    }

    ret = jpeg_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, config);
    if (ret < 0) {
        ALOGE("[%s]: decoder output S_FMT failed", __func__);
        return -1;
    }

    return ret;
}

int jpeghal_dec_getconfig(int fd, struct jpeg_config *config)
{
    int ret = 0;

    jpeghal_getconfig(fd, config);

    return ret;
}

int jpeghal_enc_setconfig(int fd, struct jpeg_config *config)
{
    int ret = 0;

    ret = jpeg_v4l2_s_jpegcomp(fd, config->enc_qual);
    if (ret < 0) {
        ALOGE("[%s]: S_JPEGCOMP failed", __func__);
        return -1;
    }

    config->mode    = JPEG_ENCODE;

    ret = jpeg_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, config);
    if (ret < 0) {
        ALOGE("[%s]: encoder input S_FMT failed", __func__);
        return -1;
    }

    ret = jpeg_v4l2_s_fmt(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, config);
    if (ret < 0) {
        ALOGE("[%s]: encoder output S_FMT failed", __func__);
        return -1;
    }

    return ret;
}

int jpeghal_enc_getconfig(int fd, struct jpeg_config *config)
{
    int ret = 0;

    jpeghal_getconfig(fd, config);

    return ret;
}

int jpeghal_set_inbuf(int fd, struct jpeg_buf *buf)
{
    int ret = 0;

    buf->buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    ret = jpeg_v4l2_reqbufs(fd, 1, buf);
    if (ret < 0) {
        ALOGE("[%s:%d]: Input REQBUFS failed", __func__, ret);
        return -1;
    }

    if (buf->memory == V4L2_MEMORY_MMAP) {
        ret = jpeg_v4l2_querybuf(fd, buf);
        if (ret < 0) {
            ALOGE("[%s:%d]: Input QUERYBUF failed", __func__, ret);
            return -1;
        }
    }

    return ret;
}

int jpeghal_set_outbuf(int fd, struct jpeg_buf *buf)
{
    int ret = 0;

    buf->buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    ret = jpeg_v4l2_reqbufs(fd, 1, buf);
    if (ret < 0) {
        ALOGE("[%s:%d]: Output REQBUFS failed", __func__, ret);
        return -1;
    }

    if (buf->memory == V4L2_MEMORY_MMAP) {
        ret = jpeg_v4l2_querybuf(fd, buf);
        if (ret < 0) {
            ALOGE("[%s:%d]: Output QUERYBUF failed", __func__, ret);
            return -1;
        }
    }

    return ret;
}

static int jpeg_exe(int fd, struct jpeg_buf *in_buf, struct jpeg_buf *out_buf)
{
    int ret = 0;

    ret = jpeg_v4l2_qbuf(fd, in_buf);
    if (ret < 0) {
        ALOGE("[%s:%d]: Input QBUF failed", __func__, ret);
        return -1;
    }

    ret = jpeg_v4l2_qbuf(fd, out_buf);
    if (ret < 0) {
        ALOGE("[%s:%d]: Output QBUF failed", __func__, ret);
        return -1;
    }

    ret = jpeg_v4l2_streamon(fd, in_buf->buf_type);
    ret = jpeg_v4l2_streamon(fd, out_buf->buf_type);

    ret = jpeg_v4l2_dqbuf(fd, in_buf->buf_type, in_buf->memory);
    ret = jpeg_v4l2_dqbuf(fd, out_buf->buf_type, out_buf->memory);

    return ret;
}

int jpeghal_dec_exe(int fd, struct jpeg_buf *in_buf, struct jpeg_buf *out_buf)
{
    int ret = 0;

    ret = jpeg_exe(fd, in_buf, out_buf);
    if (ret < 0)
        ALOGE("[%s]: JPEG decoding is failed", __func__);

    return ret;
}

int jpeghal_enc_exe(int fd, struct jpeg_buf *in_buf, struct jpeg_buf *out_buf)
{
    int ret = 0;

    ret = jpeg_exe(fd, in_buf, out_buf);
    if (ret < 0)
        ALOGE("[%s]: JPEG Encoding is failed", __func__);

    return ret;
}

int jpeghal_deinit(int fd, struct jpeg_buf *in_buf, struct jpeg_buf *out_buf)
{
    int ret = 0;

    jpeg_v4l2_streamoff(fd, in_buf->buf_type);
    jpeg_v4l2_streamoff(fd, out_buf->buf_type);

    if (in_buf->memory == V4L2_MEMORY_MMAP)
        munmap((char *)(in_buf->start[0]), in_buf->length[0]);

    if (out_buf->memory == V4L2_MEMORY_MMAP)
        munmap((char *)(out_buf->start[0]), out_buf->length[0]);

    jpeg_v4l2_reqbufs(fd, 0, in_buf);

    jpeg_v4l2_reqbufs(fd, 0, out_buf);

    ret = close(fd);

    return ret;
}

int jpeghal_s_ctrl(int fd, int cid, int value)
{
    struct v4l2_control vc;
    int ret = 0;

    vc.id = cid;
    vc.value = value;

    ret = ioctl(fd, VIDIOC_S_CTRL, &vc);
    if (ret != 0) {
        ALOGE("[%s] ioctl : cid(%d), value(%d)\n", __func__, cid, value);
        return -1;
    }

    return ret;
}

int jpeghal_g_ctrl(int fd, int id)
{
    struct v4l2_control ctrl;
    int ret = 0;

    ctrl.id = id;

    ret = ioctl(fd, VIDIOC_G_CTRL, &ctrl);
    if (ret < 0) {
        ALOGE("[%s] ioctl : cid(%d)\n", __func__, ctrl.id);
        return -1;
    }

    return ctrl.value;
}
