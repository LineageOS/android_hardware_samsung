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

const char *V4L2_DEV = "/dev/video2";

int v4l2_open(struct hwc_context_t *ctx)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_control vc;

    // open V4L2 node
    if (ctx->v4l2_fd <= 0)
        ctx->v4l2_fd = open(V4L2_DEV, O_RDWR);

    if (ctx->v4l2_fd  <= 0) {
        ALOGE("%s: Unable to get v4l2 node for %s", __FUNCTION__, V4L2_DEV);
        goto err;
    }

    // Check capabilities
    if (ioctl(ctx->v4l2_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ALOGE("%s: Unable to query capabilities", __FUNCTION__);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s: %s has no streaming support", __FUNCTION__, V4L2_DEV);
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        ALOGE("%s: %s has no video out support", __FUNCTION__, V4L2_DEV);
        goto err;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(ctx->v4l2_fd, VIDIOC_G_FMT, &fmt) < 0) {
        ALOGE("%s: Unable to get format", __FUNCTION__);
        goto err;
    }

    vc.id = V4L2_CID_OVLY_MODE;
    vc.value = FIMC_OVLY_NONE_MULTI_BUF;

    if (ioctl(ctx->v4l2_fd, VIDIOC_S_CTRL, &vc) < 0) {
        ALOGE("%s: Unable to set overlay mode", __FUNCTION__);
        goto err;
    }

    return 0;

err:
    if (ctx->v4l2_fd > 0)
        close(ctx->v4l2_fd);

    ctx->v4l2_fd = 0;

    return -1;
}

void v4l2_close(struct hwc_context_t *ctx)
{
    if (ctx->v4l2_fd > 0)
        close(ctx->v4l2_fd);

    ctx->v4l2_fd = 0;
}

int v4l2_reqbufs_out(struct hwc_context_t *ctx, int count)
{
    struct v4l2_requestbuffers requestbuffers;
    int rc;

    if (count < 0)
        return -EINVAL;

    requestbuffers.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    requestbuffers.count = count;
    requestbuffers.memory = V4L2_MEMORY_USERPTR;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_REQBUFS, &requestbuffers);
    if (rc < 0)
        return rc;

    return requestbuffers.count;
}

int v4l2_s_fmt_pix_out(struct hwc_context_t *ctx, int width, int height, int fmt, int priv)
{
    struct v4l2_format format;
    int rc;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = fmt;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    format.fmt.pix.priv = priv;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_S_FMT, &format);
    return rc;
}

int v4l2_s_crop(struct hwc_context_t *ctx, int left, int top, int width, int height)
{
    struct v4l2_crop crop;
    int rc;

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    crop.c.left = left;
    crop.c.top = top;
    crop.c.width = width;
    crop.c.height = height;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_S_CROP, &crop);
    return rc;
}

int v4l2_s_ctrl(struct hwc_context_t *ctx, int id, int value)
{
    struct v4l2_control control;
    int rc;

    control.id = id;
    control.value = value;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_S_CTRL, &control);
    if (rc < 0)
        return rc;

    if (id == V4L2_CID_DST_INFO)
        return rc;
    else
        return control.value;
}

int v4l2_g_fbuf(struct hwc_context_t *ctx, void **base, int *width, int *height, int *fmt)
{
    struct v4l2_framebuffer framebuffer;
    int rc;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_G_FBUF, &framebuffer);
    if (rc < 0)
        return rc;

    if (base != NULL)
        *base = framebuffer.base;

    if (width != NULL)
        *width = framebuffer.fmt.width;

    if (height != NULL)
        *height = framebuffer.fmt.height;

    if (fmt != NULL)
        *fmt = framebuffer.fmt.pixelformat;

    return 0;
}

int v4l2_s_fbuf(struct hwc_context_t *ctx, void *base, int width, int height, int fmt)
{
    struct v4l2_framebuffer framebuffer;
    int rc;

    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.base = base;
    framebuffer.fmt.width = width;
    framebuffer.fmt.height = height;
    framebuffer.fmt.pixelformat = fmt;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_S_FBUF, &framebuffer);
    return rc;
}

int v4l2_s_fmt_win(struct hwc_context_t *ctx, int left, int top, int width, int height)
{
    struct v4l2_format format;
    int rc;

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
    format.fmt.win.w.left = left;
    format.fmt.win.w.top = top;
    format.fmt.win.w.width = width;
    format.fmt.win.w.height = height;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_S_FMT, &format);
    return rc;
}

int v4l2_streamon_out(struct hwc_context_t *ctx)
{
    enum v4l2_buf_type buf_type;
    int rc;

    buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_STREAMON, &buf_type);
    return rc;
}

int v4l2_qbuf(struct hwc_context_t *ctx, int index, unsigned long userptr)
{
    struct v4l2_buffer buffer;
    int rc;

    if (index < 0)
        return -EINVAL;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buffer.memory = V4L2_MEMORY_USERPTR;
    buffer.index = index;

    if (userptr)
        buffer.m.userptr = userptr;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_QBUF, &buffer);
    return rc;
}

int v4l2_dqbuf(struct hwc_context_t *ctx)
{
    struct v4l2_buffer buffer;
    int rc;

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buffer.memory = V4L2_MEMORY_USERPTR;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_DQBUF, &buffer);
    if (rc < 0)
        return rc;

    return buffer.index;
}

int v4l2_streamoff(struct hwc_context_t *ctx)
{
    enum v4l2_buf_type buf_type;
    int rc;

    buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    rc = ioctl(ctx->v4l2_fd, VIDIOC_STREAMOFF, &buf_type);
    return rc;
}

