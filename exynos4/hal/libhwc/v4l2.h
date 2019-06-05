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

#ifndef ANDROID_HWCOMPOSER_V4L2_H
#define ANDROID_HWCOMPOSER_V4L2_H

int v4l2_open(struct hwc_context_t *ctx);
void v4l2_close(struct hwc_context_t *ctx);
int v4l2_reqbufs_out(struct hwc_context_t *ctx, int count);
int v4l2_s_fmt_pix_out(struct hwc_context_t *ctx, int width, int height, int fmt, int priv);
int v4l2_s_crop(struct hwc_context_t *ctx, int left, int top, int width, int height);
int v4l2_s_ctrl(struct hwc_context_t *ctx, int id, int value);
int v4l2_g_fbuf(struct hwc_context_t *ctx, void **base, int *width, int *height, int *fmt);
int v4l2_s_fbuf(struct hwc_context_t *ctx, void *base, int width, int height, int fmt);
int v4l2_s_fmt_win(struct hwc_context_t *ctx, int left, int top, int width, int height);
int v4l2_streamon_out(struct hwc_context_t *ctx);
int v4l2_qbuf(struct hwc_context_t *ctx, int index, unsigned long userptr);
int v4l2_dqbuf(struct hwc_context_t *ctx);
int v4l2_streamoff(struct hwc_context_t *ctx);
#endif //ANDROID_HWCOMPOSER_V4L2_H
