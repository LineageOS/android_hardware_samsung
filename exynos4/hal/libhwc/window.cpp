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

/*****************************************************************************/

int window_clear(struct hwc_context_t *ctx)
{
    struct s3c_fb_win_config_data config;

    memset(&config, 0, sizeof(struct s3c_fb_win_config_data));
    if ( ioctl(ctx->fb0_fd, S3CFB_WIN_CONFIG, &config) < 0 )
    {
        ALOGE("%s Failed to clear screen : %s", __FUNCTION__, strerror(errno));
    }

    return config.fence;
}

void config_handle(struct hwc_context_t* ctx, const hwc_layer_1_t &layer, s3c_fb_win_config &cfg)
{
    uint32_t x, y;
    uint32_t w = WIDTH(layer.displayFrame);
    uint32_t h = HEIGHT(layer.displayFrame);
    private_handle_t* handle = (private_handle_t*) layer.handle;
    uint8_t bpp = format_to_bpp(handle->format);
    hwc_rect_t crop = integerizeSourceCrop(layer.sourceCropf);
    uint32_t offset = (crop.top * handle->stride + crop.left) * bpp / 8;

    if (layer.displayFrame.left < 0) {
        unsigned int crop = -layer.displayFrame.left;
        ALOGW("layer off left side of screen; cropping %u pixels from left edge",
                crop);
        x = 0;
        w -= crop;
        offset += crop * bpp / 8;
    } else {
        x = layer.displayFrame.left;
    }

    if (layer.displayFrame.right > ctx->xres) {
        ALOGV("displayFrame.right=%d ctx->xres=%d", layer.displayFrame.right, ctx->xres);
        unsigned int crop = layer.displayFrame.right - ctx->xres;
        ALOGW("layer off right side of screen; cropping %u pixels from right edge",
                crop);
        w -= crop;
    }

    if (layer.displayFrame.top < 0) {
        unsigned int crop = -layer.displayFrame.top;
        ALOGW("layer off top side of screen; cropping %u pixels from top edge",
                crop);
        y = 0;
        h -= crop;
        offset += handle->stride * crop * bpp / 8;
    } else {
        y = layer.displayFrame.top;
    }

    if (layer.displayFrame.bottom > ctx->yres) {
        ALOGV("displayFrame.bottom=%d ctx->yres=%d", layer.displayFrame.bottom, ctx->yres);
        int crop = layer.displayFrame.bottom - ctx->yres;
        ALOGW("layer off bottom side of screen; cropping %u pixels from bottom edge",
                crop);
        h -= crop;
    }

    cfg.state = cfg.S3C_FB_WIN_STATE_BUFFER;
    cfg.fd = handle->fd;
    cfg.x = x;
    cfg.y = y;
    cfg.w = w;
    cfg.h = h;
    cfg.format = format_to_s3c_format(handle->format);
    cfg.offset = offset;
    cfg.stride = handle->stride * bpp / 8;
    cfg.blending = blending_to_s3c_blending(layer.blending);
    cfg.fence_fd = layer.acquireFenceFd;

    cfg.plane_alpha = 255;
    if (layer.planeAlpha && (layer.planeAlpha < 254)) {
        cfg.plane_alpha = layer.planeAlpha;
    }
}

void dump_fb_win_cfg(struct s3c_fb_win_config_data &cfg)
{
    ALOGV("%s: fence=%d", __FUNCTION__, cfg.fence);

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        if (cfg.config[i].state != s3c_fb_win_config::S3C_FB_WIN_STATE_DISABLED) {
            ALOGV("win%d state(%d) (%d,%d) (%d,%d) format(%d) blending(%d) phys_addr(0x%x) offset(0x%x)",
                    i, cfg.config[i].state, cfg.config[i].x, cfg.config[i].y, cfg.config[i].w, cfg.config[i].h,
                    cfg.config[i].format, cfg.config[i].blending, cfg.config[i].phys_addr, cfg.config[i].offset);
        }
    }
}

int window_buffer_allocate(struct hwc_context_t *ctx, struct hwc_win_info_t *win)
{
    int rc = 0;
    int dst_stride;
    size_t i, j;

    for (i = 0; i < NUM_OF_WIN_BUF; i++) {
        rc = ctx->alloc_device->alloc(ctx->alloc_device, win->rect_info.w, win->rect_info.h,
                    HAL_PIXEL_FORMAT_RGBA_8888, GRALLOC_USAGE_HW_ION, &win->dst_buf[i],
                    &dst_stride);
        if (rc < 0) {
            ALOGE("%s: Failed to allocate destination buffer: %s", __FUNCTION__, strerror(-rc));
            goto err_alloc;
        }
        struct private_handle_t *hnd = private_handle_t::dynamicCast(win->dst_buf[i]);
    }

    return rc;

err_alloc:
    for (j = 0; j < i &&  j < NUM_OF_WIN_BUF; j++) {
        if (win->dst_buf[j]) {
            ctx->alloc_device->free(ctx->alloc_device, win->dst_buf[j]);
            win->dst_buf[j] = NULL;
        }
    }

    return rc;
}

int window_buffer_deallocate(struct hwc_context_t *ctx, struct hwc_win_info_t *win)
{
    for (size_t i = 0; i < NUM_OF_WIN_BUF; i++) {
        if (win->dst_buf_fence[i] >= 0) {
            close(win->dst_buf_fence[i]);
            win->dst_buf_fence[i] = -1;
        }

        if (win->dst_buf[i]) {
            ctx->alloc_device->free(ctx->alloc_device, win->dst_buf[i]);
            win->dst_buf[i] = NULL;
        }
    }

    return 0;
}
