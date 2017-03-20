/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <s3c_lcd.h>

#include <EGL/egl.h>

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <cutils/compiler.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/gralloc.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <hardware_legacy/uevent.h>
#include <utils/String8.h>
#include <utils/Vector.h>

#include <sync/sync.h>

#include "libedid.h"
#include "gralloc_priv.h"
#include "exynos_fimc.h"
#include "exynos_format.h"
#include "exynos_v4l2.h"
#include "graphics.h"
#include "s5p_tvout_v4l2.h"

#define GRALLOC_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#define USE_MIXER_VP 1
#define HDMI_DEFAULT_PRESET_ID V4L2_DV_480P59_94

const size_t NUM_HW_WINDOWS = 5;
const size_t NO_FB_NEEDED = NUM_HW_WINDOWS + 1;
const size_t MAX_PIXELS = 480 * 800 * 2;
const size_t FIMC_W_ALIGNMENT = 16;
const size_t FIMC_H_ALIGNMENT = 16;
const size_t FIMC_DST_CROP_W_ALIGNMENT_RGB888 = 32;
const size_t FIMC_DST_W_ALIGNMENT_RGB888 = 32;
const size_t FIMC_DST_H_ALIGNMENT_RGB888 = 1;
const size_t FIMD_FIMC_IDX = 0;
const size_t HDMI_FIMC_IDX = 1;
const int AVAILABLE_FIMC_UNITS[] = { 2, 3 };
const size_t NUM_FIMC_UNITS = sizeof(AVAILABLE_FIMC_UNITS) /
        sizeof(AVAILABLE_FIMC_UNITS[0]);
const size_t BURSTLEN_BYTES = 16 * 8;
const size_t NUM_HDMI_BUFFERS = 4;

struct exynos4_hwc_composer_device_1_t;

struct exynos4_fimc_map_t {
    enum {
        FIMC_NONE = 0,
        FIMC_M2M,
        // TODO: FIMC_LOCAL_PATH
    } mode;
    int idx;
};

struct exynos4_hwc_post_data_t {
    int                 overlay_map[NUM_HW_WINDOWS];
    exynos4_fimc_map_t   fimc_map[NUM_HW_WINDOWS];
    size_t              fb_window;
};

const size_t NUM_FIMC_DST_BUFS = 3;
struct exynos4_fimc_data_t {
    void            *fimc;
    exynos_fimc_img  src_cfg;
    exynos_fimc_img  dst_cfg;
    buffer_handle_t dst_buf[NUM_FIMC_DST_BUFS];
    int             dst_buf_fence[NUM_FIMC_DST_BUFS];
    size_t          current_buf;
};

struct hdmi_layer_t {
    int     id;
    int     fd;
    bool    enabled;
    exynos_fimc_img  cfg;

    bool    streaming;
    size_t  current_buf;
    size_t  queued_buf;
};

struct exynos4_hwc_composer_device_1_t {
    hwc_composer_device_1_t base;

    int                     fd;
    int                     vsync_fd;
    exynos4_hwc_post_data_t bufs;

    const private_module_t  *gralloc_module;
    alloc_device_t          *alloc_device;
    const hwc_procs_t       *procs;
    pthread_t               vsync_thread;
    int                     force_gpu;

    int32_t                 xres;
    int32_t                 yres;
    int32_t                 xdpi;
    int32_t                 ydpi;
    int32_t                 vsync_period;

    bool hdmi_hpd;
    bool hdmi_enabled;
    bool hdmi_blanked;
    int  edid_enabled;
    int hdmi_configured;
    int  hdmi_w;
    int  hdmi_h;

    hdmi_layer_t            hdmi_layers[2];

    exynos4_fimc_data_t      fimc[NUM_FIMC_UNITS];

    struct s3c_fb_win_config last_config[NUM_HW_WINDOWS];
    size_t                  last_fb_window;
    const void              *last_handles[NUM_HW_WINDOWS];
    exynos4_fimc_map_t       last_fimc_map[NUM_HW_WINDOWS];
};

static void exynos4_cleanup_fimc_m2m(exynos4_hwc_composer_device_1_t *pdev,
        size_t fimc_idx);

static void dump_handle(private_handle_t *h)
{
    ALOGV("\t\tformat = %d, width = %u, height = %u, stride = %u, vstride = %u",
            h->format, h->width, h->height, h->stride, h->stride);
}

static void dump_layer(hwc_layer_1_t const *l)
{
    ALOGV("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform,
            l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);

    if(l->handle && !(l->flags & HWC_SKIP_LAYER))
        dump_handle(private_handle_t::dynamicCast(l->handle));
}

static void dump_config(s3c_fb_win_config &c)
{
    ALOGV("\tstate = %u", c.state);
    if (c.state == c.S3C_FB_WIN_STATE_BUFFER) {
        ALOGV("\t\tfd = %d, offset = %u, stride = %u, "
                "x = %d, y = %d, w = %u, h = %u, "
                "format = %u, blending = %u",
                c.fd, c.offset, c.stride,
                c.x, c.y, c.w, c.h,
                c.format, c.blending);
    }
    else if (c.state == c.S3C_FB_WIN_STATE_COLOR) {
        ALOGV("\t\tcolor = %u", c.color);
    }
}

static void dump_fimc_img(exynos_fimc_img &c)
{
    ALOGV("\tx = %u, y = %u, w = %u, h = %u, fw = %u, fh = %u",
            c.x, c.y, c.w, c.h, c.fw, c.fh);
    ALOGV("\taddr = {%u, %u, %u}, rot = %u, cacheable = %u, drmMode = %u",
            c.yaddr, c.uaddr, c.vaddr, c.rot, c.cacheable, c.drmMode);
}

inline int WIDTH(const hwc_rect &rect) { return rect.right - rect.left; }
inline int HEIGHT(const hwc_rect &rect) { return rect.bottom - rect.top; }
template<typename T> inline T max(T a, T b) { return (a > b) ? a : b; }
template<typename T> inline T min(T a, T b) { return (a < b) ? a : b; }

template<typename T> void align_crop_and_center(T &w, T &h,
        hwc_rect_t *crop, size_t alignment)
{
    double aspect = 1.0 * h / w;
    T w_orig = w, h_orig = h;

    w = ALIGN(w, alignment);
    h = round(aspect * w);
    if (crop) {
        crop->left = (w - w_orig) / 2;
        crop->top = (h - h_orig) / 2;
        crop->right = crop->left + w_orig;
        crop->bottom = crop->top + h_orig;
    }
}

static bool is_transformed(const hwc_layer_1_t &layer)
{
    return layer.transform != 0;
}

static bool is_rotated(const hwc_layer_1_t &layer)
{
    return (layer.transform & HAL_TRANSFORM_ROT_90) ||
            (layer.transform & HAL_TRANSFORM_ROT_180);
}

static bool is_scaled(const hwc_layer_1_t &layer)
{
    return WIDTH(layer.displayFrame) != WIDTH(layer.sourceCrop) ||
            HEIGHT(layer.displayFrame) != HEIGHT(layer.sourceCrop);
}

static inline bool fimc_dst_cfg_changed(exynos_fimc_img &c1, exynos_fimc_img &c2)
{
    return c1.x != c2.x ||
            c1.y != c2.y ||
            c1.w != c2.w ||
            c1.h != c2.h ||
            c1.format != c2.format ||
            c1.rot != c2.rot ||
            c1.cacheable != c2.cacheable ||
            c1.drmMode != c2.drmMode;
}

static inline bool fimc_src_cfg_changed(exynos_fimc_img &c1, exynos_fimc_img &c2)
{
    return fimc_dst_cfg_changed(c1, c2) ||
            c1.fw != c2.fw ||
            c1.fh != c2.fh;
}

static enum s3c_fb_pixel_format exynos4_format_to_s3c_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return S3C_FB_PIXEL_FORMAT_RGBA_8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return S3C_FB_PIXEL_FORMAT_RGBX_8888;
    case HAL_PIXEL_FORMAT_RGBA_5551:
        return S3C_FB_PIXEL_FORMAT_RGBA_5551;
    case HAL_PIXEL_FORMAT_RGB_565:
        return S3C_FB_PIXEL_FORMAT_RGB_565;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return S3C_FB_PIXEL_FORMAT_BGRA_8888;
    default:
        return S3C_FB_PIXEL_FORMAT_MAX;
    }
}

static bool exynos4_format_is_supported(int format)
{
    return exynos4_format_to_s3c_format(format) < S3C_FB_PIXEL_FORMAT_MAX;
}

static bool exynos4_format_is_rgb(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
        return true;

    default:
        return false;
    }
}

static bool exynos4_format_is_supported_by_fimc(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_EXYNOS_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:      /* To support SW codec     */
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:

        return true;

    default:
        return false;
    }
}

static bool exynos4_format_is_ycrcb(int format)
{
    return format == HAL_PIXEL_FORMAT_EXYNOS_YV12;
}

static bool exynos4_format_requires_fimc(int format)
{
    return (exynos4_format_is_supported_by_fimc(format) &&
           (format != HAL_PIXEL_FORMAT_RGBX_8888) && (format != HAL_PIXEL_FORMAT_RGB_565));
}

static uint8_t exynos4_format_to_bpp(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGB_888:
        return 32;

    case HAL_PIXEL_FORMAT_RGBA_5551:
    case HAL_PIXEL_FORMAT_RGBA_4444:
    case HAL_PIXEL_FORMAT_RGB_565:
        return 16;

    default:
        ALOGW("unrecognized pixel format %u", format);
        return 0;
    }
}

static bool is_x_aligned(const hwc_layer_1_t &layer, int format)
{
    if (!exynos4_format_is_supported(format))
        return true;

    uint8_t bpp = exynos4_format_to_bpp(format);
    uint8_t pixel_alignment = 32 / bpp;

    return (layer.displayFrame.left % pixel_alignment) == 0 &&
            (layer.displayFrame.right % pixel_alignment) == 0;
}

static bool dst_crop_w_aligned(int dest_w)
{
    int dst_crop_w_alignement;

   /* FIMC's dst crop size should be aligned 128Bytes */
    dst_crop_w_alignement = FIMC_DST_CROP_W_ALIGNMENT_RGB888;

    return (dest_w % dst_crop_w_alignement) == 0;
}

static bool exynos4_supports_fimc(hwc_layer_1_t &layer, int format,
        bool local_path)
{
    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    int max_w = is_rotated(layer) ? 1920 : 8192;
    int max_h = is_rotated(layer) ? 1920 : 4224;

    bool rot90or270 = !!(layer.transform & HAL_TRANSFORM_ROT_90);
    // n.b.: HAL_TRANSFORM_ROT_270 = HAL_TRANSFORM_ROT_90 |
    //                               HAL_TRANSFORM_ROT_180

    int src_w = WIDTH(layer.sourceCrop), src_h = HEIGHT(layer.sourceCrop);
    int dest_w, dest_h;
    if (rot90or270) {
        dest_w = HEIGHT(layer.displayFrame);
        dest_h = WIDTH(layer.displayFrame);
    } else {
        dest_w = WIDTH(layer.displayFrame);
        dest_h = HEIGHT(layer.displayFrame);
    }

    if (handle->flags & GRALLOC_USAGE_PROTECTED)
        align_crop_and_center(dest_w, dest_h, NULL,
                FIMC_DST_CROP_W_ALIGNMENT_RGB888);

    int max_downscale = local_path ? 4 : 16;
    const int max_upscale = 8;

    return exynos4_format_is_supported_by_fimc(format) &&
            dst_crop_w_aligned(dest_w) &&
            handle->stride <= max_w &&
            handle->stride % FIMC_W_ALIGNMENT == 0 &&
            src_w <= dest_w * max_downscale &&
            dest_w <= src_w * max_upscale &&
            //handle->vstride <= max_h &&
            //handle->vstride % FIMC_H_ALIGNMENT == 0 &&
            src_h <= dest_h * max_downscale &&
            dest_h <= src_h * max_upscale &&
            // per 46.2
            (!rot90or270 || layer.sourceCrop.top % 2 == 0) &&
            (!rot90or270 || layer.sourceCrop.left % 2 == 0);
            // per 46.3.1.6
}

static bool exynos4_requires_fimc(hwc_layer_1_t &layer, int format)
{
    return exynos4_format_requires_fimc(format) || is_scaled(layer)
            || is_transformed(layer) || !is_x_aligned(layer, format);
}

int hdmi_get_config(struct exynos4_hwc_composer_device_1_t *dev)
{
    struct v4l2_dv_preset preset;
    struct v4l2_dv_enum_preset enum_preset;
    struct v4l2_dv_enum_preset default_preset;
    int index = 0;
    bool found = false;
    int ret;

    if(dev->hdmi_configured)
      return 0;

    if (ioctl(dev->hdmi_layers[1].fd, VIDIOC_G_DV_PRESET, &preset) < 0) {
        ALOGE("%s: g_dv_preset error, %d", __func__, errno);
        return -1;
    }

    while (true) {
        enum_preset.index = index++;
        ret = ioctl(dev->hdmi_layers[1].fd, VIDIOC_ENUM_DV_PRESETS, &enum_preset);
        if (ret < 0) {
            if (errno == EINVAL)
                break;
            ALOGE("%s: enum_dv_presets error, %d", __func__, errno);
            return -1;
        }

        if (enum_preset.preset == HDMI_DEFAULT_PRESET_ID) {
            default_preset.width  = enum_preset.width;
            default_preset.height = enum_preset.height;
            default_preset.preset = HDMI_DEFAULT_PRESET_ID;
        }

        ALOGV("%s: %d preset=%02d width=%d height=%d name=%s",
                    __func__, enum_preset.index, enum_preset.preset,
                    enum_preset.width, enum_preset.height, enum_preset.name);

        if (preset.preset == enum_preset.preset) {
             if(dev->edid_enabled){
                /*if (!hdmi_check_resolution(preset.preset)) {
                    dev->hdmi_w  = enum_preset.width;
                    dev->hdmi_h  = enum_preset.height;
                    found = true;
                    break;
                } else*/
                    found = false;
                    break;
            }
        }
    }

    if (!found){
        preset.preset = HDMI_DEFAULT_PRESET_ID;
        ret = ioctl(dev->hdmi_layers[1].fd, VIDIOC_S_DV_PRESET, &preset);
        if (ret < 0) {
            ALOGE("%s: enum_dv_presets error, %d", __func__, errno);
            return -1;
        }
        dev->hdmi_w  = default_preset.width;
        dev->hdmi_h  = default_preset.height;
        found = true;
    }

    dev->hdmi_configured = 1;
    return found ? 0 : -1;
}

static enum s3c_fb_blending exynos4_blending_to_s3c_blending(int32_t blending)
{
    switch (blending) {
    case HWC_BLENDING_NONE:
        return S3C_FB_BLENDING_NONE;
    case HWC_BLENDING_PREMULT:
        return S3C_FB_BLENDING_PREMULT;
    case HWC_BLENDING_COVERAGE:
        return S3C_FB_BLENDING_COVERAGE;

    default:
        return S3C_FB_BLENDING_MAX;
    }
}

static bool exynos4_blending_is_supported(int32_t blending)
{
    return exynos4_blending_to_s3c_blending(blending) < S3C_FB_BLENDING_MAX;
}


static int hdmi_enable_layer(struct exynos4_hwc_composer_device_1_t *dev,
                             hdmi_layer_t &hl)
{
    if (hl.enabled)
        return 0;

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count  = NUM_HDMI_BUFFERS;
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(hl.fd, &reqbuf) < 0) {
        ALOGE("%s: layer%d: reqbufs failed %d", __func__, hl.id, errno);
        return -1;
    }

    if (reqbuf.count != NUM_HDMI_BUFFERS) {
        ALOGE("%s: layer%d: didn't get buffer", __func__, hl.id);
        return -1;
    }

    if (hl.id == 1) {
        if (exynos_v4l2_s_ctrl(hl.fd, V4L2_CID_TV_PIXEL_BLEND_ENABLE, 1) < 0) {
            ALOGE("%s: layer%d: PIXEL_BLEND_ENABLE failed %d", __func__,
                                                                hl.id, errno);
            return -1;
        }
    }

    ALOGV("%s: layer%d enabled", __func__, hl.id);
    hl.enabled = true;
    return 0;
}

static void hdmi_disable_layer(struct exynos4_hwc_composer_device_1_t *dev,
                               hdmi_layer_t &hl)
{
    if (!hl.enabled)
        return;

    if (hl.streaming) {
        if (exynos_v4l2_streamoff(hl.fd, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0)
            ALOGE("%s: layer%d: streamoff failed %d", __func__, hl.id, errno);
        hl.streaming = false;
    }

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(hl.fd, &reqbuf) < 0)
        ALOGE("%s: layer%d: reqbufs failed %d", __func__, hl.id, errno);

    memset(&hl.cfg, 0, sizeof(hl.cfg));
    hl.current_buf = 0;
    hl.queued_buf = 0;
    hl.enabled = false;

    ALOGV("%s: layer%d disabled", __func__, hl.id);
}

static int hdmi_display_edid_info(struct exynos4_hwc_composer_device_1_t *dev)
{
    int ret = 0;
    if (dev->edid_enabled)
        return 1;
    if (!EDIDOpen())
        ALOGE("EDIDInit() failed!\n");
    else {
        if (!EDIDRead()) {
            ALOGE("EDIDRead() failed!\n");
            if (!EDIDClose())
                ALOGE("EDIDClose() failed!\n");
        } else {
            //display_menu();
            ret = 1;
        }
    }
    dev->edid_enabled = ret;
    return ret;
}

static int hdmi_open(struct exynos4_hwc_composer_device_1_t *dev)
{
    if(dev->hdmi_layers[0].fd > 0)
        return 0;

    dev->hdmi_layers[0].id = 0;
#ifdef USE_MIXER_VP
    dev->hdmi_layers[0].fd = open("/dev/video12", O_RDWR);
#else
    dev->hdmi_layers[0].fd = open("/dev/video10", O_RDWR);
#endif
    if (dev->hdmi_layers[0].fd < 0) {
	    ALOGE("failed to open hdmi layer0 device");
            return -1;
    }

    dev->hdmi_layers[1].id = 1;
    dev->hdmi_layers[1].fd = open("/dev/video11", O_RDWR);
    if (dev->hdmi_layers[1].fd < 0) {
        ALOGE("failed to open hdmi layer1 device");
        return -1;
    }
    return 0;
}

static void hdmi_close(struct exynos4_hwc_composer_device_1_t *dev)
{
    close(dev->hdmi_layers[0].fd);
    close(dev->hdmi_layers[1].fd);

    dev->hdmi_layers[0].fd = -1;
    dev->hdmi_layers[1].fd = -1;
    dev->hdmi_configured = 0;
    dev->edid_enabled =0;
}

static int hdmi_enable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (dev->hdmi_enabled)
        return 0;

    if (dev->hdmi_blanked)
        return 0;

    if (hdmi_open(dev) < 0)
	return 0;


    if (exynos_v4l2_s_ctrl(dev->hdmi_layers[1].fd, VIDIOC_HDCP_ENABLE, \
                dev->edid_enabled) < 0)
	    ALOGE("%s: s_ctrl(CID_TV_HDCP_ENABLE) failed %d", __func__, errno);

    hdmi_enable_layer(dev, dev->hdmi_layers[1]);

    dev->hdmi_enabled = true;
    return 0;
}

static void hdmi_disable(struct exynos4_hwc_composer_device_1_t *dev)
{
    if (!dev->hdmi_enabled)
        return;

    hdmi_disable_layer(dev, dev->hdmi_layers[0]);
    hdmi_disable_layer(dev, dev->hdmi_layers[1]);
    hdmi_close(dev);
    EDIDClose();
#ifndef USE_MIXER_VP
    exynos4_cleanup_fimc_m2m(dev, HDMI_FIMC_IDX);
#endif
    dev->hdmi_enabled = false;
}
#ifdef USE_MIXER_VP
static int hdmi_vp_output(struct exynos4_hwc_composer_device_1_t *dev,
                       hdmi_layer_t &hl,
                       hwc_layer_1_t &layer,
                       private_handle_t *h,
                       int acquireFenceFd,
                       int *releaseFenceFd)
{
    int ret = 0;

    exynos_fimc_img cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.x = layer.displayFrame.left;
    cfg.y = layer.displayFrame.top;
    cfg.w = WIDTH(layer.displayFrame);
    cfg.h = HEIGHT(layer.displayFrame);

    exynos_fimc_img sourcecropcfg;
    memset(&sourcecropcfg, 0, sizeof(sourcecropcfg));
    sourcecropcfg.x = layer.sourceCrop.left;
    sourcecropcfg.y = layer.sourceCrop.top;
    sourcecropcfg.w = WIDTH(layer.sourceCrop);
    sourcecropcfg.h = HEIGHT(layer.sourceCrop);

    if (fimc_src_cfg_changed(hl.cfg, cfg)) {
        hdmi_disable_layer(dev, hl);

        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        fmt.fmt.pix_mp.width       = h->stride;
        fmt.fmt.pix_mp.height      = ALIGN(h->height, 16);
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
        fmt.fmt.pix_mp.num_planes  = 2;
        fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;

        ret = exynos_v4l2_s_fmt(hl.fd, &fmt);
        if (ret < 0) {
            ALOGE("%s: layer%d: s_fmt failed %d", __func__, hl.id, errno);
            goto err;
        }

        struct v4l2_crop crop;
        crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        crop.c.left   = cfg.x;
        crop.c.top    = cfg.y;
        crop.c.width  = cfg.w;
        crop.c.height = cfg.h - 8;
        if (exynos_v4l2_s_crop(hl.fd, &crop) < 0) {
            ALOGE("%s: layer=%d s_crop failed", __func__,hl.id);
            goto err;
        }

        memset(&crop, 0, sizeof(crop));
        crop.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        crop.c.left   = 0;
        crop.c.top    = 0;
        crop.c.width  = sourcecropcfg.w;
        crop.c.height = sourcecropcfg.h;
        if (exynos_v4l2_s_crop(hl.fd, &crop) < 0) {
            ALOGE("%s: layer=%d s_crop failed", __func__,hl.id);
            goto err;
        }

        hdmi_enable_layer(dev, hl);

        ALOGV("HDMI layer%d configuration:", hl.id);
        dump_fimc_img(cfg);
        hl.cfg = cfg;
    }

    struct v4l2_buffer buffer;
    struct v4l2_plane planes[2];

    if (hl.queued_buf == NUM_HDMI_BUFFERS) {
        memset(&buffer, 0, sizeof(buffer));
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_USERPTR;
        buffer.length = 2;
        buffer.m.planes = planes;
        ret = exynos_v4l2_dqbuf(hl.fd, &buffer);
        if (ret < 0) {
            ALOGE("%s: layer%d: dqbuf failed %d", __func__, hl.id, errno);
            goto err;
        }
        hl.queued_buf--;
    }

    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));

    buffer.index = hl.current_buf;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_USERPTR;
    buffer.flags = V4L2_BUF_FLAG_USE_SYNC;
    buffer.reserved = acquireFenceFd;

    buffer.length = 2;
    buffer.m.planes = planes;
    buffer.m.planes[0].m.userptr = h->base;
    buffer.m.planes[0].length = (h->stride * /*vstride*/ALIGN(h->height, 16)) * 3 >> 1;
    if (exynos_v4l2_qbuf(hl.fd, &buffer) < 0) {
        ALOGE("%s: layer%d: qbuf failed %d", __func__, hl.id, errno);
        ret = -1;
        goto err;
    }

    if (releaseFenceFd)
        *releaseFenceFd = buffer.reserved;
    else
        close(buffer.reserved);

    hl.queued_buf++;
    hl.current_buf = (hl.current_buf + 1) % NUM_HDMI_BUFFERS;

    if (!hl.streaming) {
        if (exynos_v4l2_streamon(hl.fd, buffer.type) < 0) {
            ALOGE("%s: layer%d: streamon failed %d", __func__, hl.id, errno);
            ret = -1;
            goto err;
        }
        hl.streaming = true;
    }

err:
    if (acquireFenceFd >= 0)
        close(acquireFenceFd);

    return ret;
}
#endif

static int hdmi_output(struct exynos4_hwc_composer_device_1_t *dev,
                       hdmi_layer_t &hl,
                       hwc_layer_1_t &layer,
                       private_handle_t *h,
                       int acquireFenceFd,
                       int *releaseFenceFd)
{
    int ret = 0;

    exynos_fimc_img cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.x = layer.displayFrame.left;
    cfg.y = layer.displayFrame.top;
    cfg.w = WIDTH(layer.displayFrame);
    cfg.h = HEIGHT(layer.displayFrame);

    if (fimc_src_cfg_changed(hl.cfg, cfg)) {
        hdmi_disable_layer(dev, hl);

        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        fmt.fmt.pix_mp.width       = h->stride;
        fmt.fmt.pix_mp.height      = /*h->vstride*/ALIGN(h->height, 16);
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_BGR32;
        fmt.fmt.pix_mp.num_planes  = 1;
        fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;
        ret = exynos_v4l2_s_fmt(hl.fd, &fmt);
        if (ret < 0) {
            ALOGE("%s: layer%d: s_fmt failed %d", __func__, hl.id, errno);
            goto err;
        }

        struct v4l2_crop crop;
        crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        crop.c.left   = 0;
        crop.c.top    = 0;
        crop.c.width  = cfg.w;
        crop.c.height = cfg.h - 8;
        if (exynos_v4l2_s_crop(hl.fd, &crop) < 0) {
            ALOGE("%s: layer=%d s_crop failed", __func__,hl.id);
            goto err;
        }

        hdmi_enable_layer(dev, hl);

        ALOGV("HDMI layer%d configuration:", hl.id);
        dump_fimc_img(cfg);
        hl.cfg = cfg;
    }

    struct v4l2_buffer buffer;
    struct v4l2_plane planes[1];

    if (hl.queued_buf == NUM_HDMI_BUFFERS) {
        memset(&buffer, 0, sizeof(buffer));
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_USERPTR;
        buffer.length = 1;
        buffer.m.planes = planes;

        ret = exynos_v4l2_dqbuf(hl.fd, &buffer);
        if (ret < 0) {
            ALOGE("%s: layer%d: dqbuf failed %d", __func__, hl.id, errno);
            goto err;
        }
        hl.current_buf = buffer.index;
        hl.queued_buf--;
    }

    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index = hl.current_buf;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_USERPTR;
    buffer.flags = V4L2_BUF_FLAG_USE_SYNC;
    buffer.reserved = acquireFenceFd;
    buffer.length = 1;
    buffer.m.planes = planes;
    buffer.m.planes[0].m.userptr = h->base;
    buffer.m.planes[0].length = (h->stride * /*vstride*/ALIGN(h->height, 16)) << 2;	
    if (exynos_v4l2_qbuf(hl.fd, &buffer) < 0) {
        ALOGE("%s: layer%d: qbuf failed %d", __func__, hl.id, errno);
        ret = -1;
        goto err;
    }

    if (releaseFenceFd)
        *releaseFenceFd = buffer.reserved;
    else
        close(buffer.reserved);

    hl.queued_buf++;
    hl.current_buf = (hl.current_buf + 1) % NUM_HDMI_BUFFERS;

    if (!hl.streaming) {
        if (exynos_v4l2_streamon(hl.fd, buffer.type) < 0) {
            ALOGE("%s: layer%d: streamon failed %d", __func__, hl.id, errno);
            ret = -1;
            goto err;
        }
        hl.streaming = true;
    }

err:
    if (acquireFenceFd >= 0)
        close(acquireFenceFd);

    return ret;
}

bool exynos4_is_offscreen(hwc_layer_1_t &layer,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    return layer.sourceCrop.left > pdev->xres ||
            layer.sourceCrop.right < 0 ||
            layer.sourceCrop.top > pdev->yres ||
            layer.sourceCrop.bottom < 0;
}

size_t exynos4_visible_width(hwc_layer_1_t &layer, int format,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    int bpp;
    if (exynos4_requires_fimc(layer, format))
        bpp = 32;
    else
        bpp = exynos4_format_to_bpp(format);
    int left = max(layer.displayFrame.left, 0);
    int right = min(layer.displayFrame.right, pdev->xres);

    return (right - left) * bpp / 8;
}

bool exynos4_supports_overlay(hwc_layer_1_t &layer, size_t i,
        struct exynos4_hwc_composer_device_1_t *pdev)
{
    if (layer.flags & HWC_SKIP_LAYER) {
        ALOGV("\tlayer %u: skipping", i);
        return false;
    }

    private_handle_t *handle = private_handle_t::dynamicCast(layer.handle);

    if (!handle) {
        ALOGV("\tlayer %u: handle is NULL", i);
        return false;
    }

    if (exynos4_visible_width(layer, handle->format, pdev) < BURSTLEN_BYTES) {
        ALOGV("\tlayer %u: visible area is too narrow", i);
        return false;
    }
    if (exynos4_requires_fimc(layer, handle->format)) {
        if (!exynos4_supports_fimc(layer, handle->format, false)) {
            ALOGV("\tlayer %u: fimc required but not supported", i);
            return false;
        }
    } else {
        if (!exynos4_format_is_supported(handle->format)) {
            ALOGV("\tlayer %u: pixel format %u not supported", i, handle->format);
            return false;
        }
    }
    if (!exynos4_blending_is_supported(layer.blending)) {
        ALOGV("\tlayer %u: blending %d not supported", i, layer.blending);
        return false;
    }
    if (CC_UNLIKELY(exynos4_is_offscreen(layer, pdev))) {
        ALOGW("\tlayer %u: off-screen", i);
        return false;
    }

    return true;
}

inline bool intersect(const hwc_rect &r1, const hwc_rect &r2)
{
    return !(r1.left > r2.right ||
        r1.right < r2.left ||
        r1.top > r2.bottom ||
        r1.bottom < r2.top);
}

inline hwc_rect intersection(const hwc_rect &r1, const hwc_rect &r2)
{
    hwc_rect i;
    i.top = max(r1.top, r2.top);
    i.bottom = min(r1.bottom, r2.bottom);
    i.left = max(r1.left, r2.left);
    i.right = min(r1.right, r2.right);
    return i;
}

static int exynos4_prepare_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    ALOGV("preparing %u layers for FIMD", contents->numHwLayers);

    memset(pdev->bufs.fimc_map, 0, sizeof(pdev->bufs.fimc_map));

    bool force_fb = pdev->force_gpu;
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        pdev->bufs.overlay_map[i] = -1;

    bool fb_needed = false;
    size_t first_fb = 0, last_fb = 0;

    // find unsupported overlays
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGV("\tlayer %u: framebuffer target", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND && !force_fb) {
            ALOGV("\tlayer %u: background supported", i);
            dump_layer(&contents->hwLayers[i]);
            continue;
        }

        if (exynos4_supports_overlay(contents->hwLayers[i], i, pdev) &&
                !force_fb) {
            ALOGV("\tlayer %u: overlay supported", i);
            layer.compositionType = HWC_OVERLAY;
            dump_layer(&contents->hwLayers[i]);
            continue;
        }

        if (!fb_needed) {
            first_fb = i;
            fb_needed = true;
        }
        last_fb = i;
        layer.compositionType = HWC_FRAMEBUFFER;

        dump_layer(&contents->hwLayers[i]);
    }

    // can't composite overlays sandwiched between framebuffers
    if (fb_needed)
        for (size_t i = first_fb; i < last_fb; i++)
            contents->hwLayers[i].compositionType = HWC_FRAMEBUFFER;

    // Incrementally try to add our supported layers to hardware windows.
    // If adding a layer would violate a hardware constraint, force it
    // into the framebuffer and try again.  (Revisiting the entire list is
    // necessary because adding a layer to the framebuffer can cause other
    // windows to retroactively violate constraints.)
    bool changed;
    bool fimc_used;
    do {
        android::Vector<hwc_rect> rects;
        android::Vector<hwc_rect> overlaps;
        size_t pixels_left, windows_left;

        fimc_used = false;

        if (fb_needed) {
            hwc_rect_t fb_rect;
            fb_rect.top = fb_rect.left = 0;
            fb_rect.right = pdev->xres - 1;
            fb_rect.bottom = pdev->yres - 1;
            pixels_left = MAX_PIXELS - pdev->xres * pdev->yres;
            windows_left = NUM_HW_WINDOWS - 1;
            rects.push_back(fb_rect);
        }
        else {
            pixels_left = MAX_PIXELS;
            windows_left = NUM_HW_WINDOWS;
        }

        changed = false;

        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if ((layer.flags & HWC_SKIP_LAYER) ||
                    layer.compositionType == HWC_FRAMEBUFFER_TARGET)
                continue;

            private_handle_t *handle = private_handle_t::dynamicCast(
                    layer.handle);

            // we've already accounted for the framebuffer above
            if (layer.compositionType == HWC_FRAMEBUFFER)
                continue;

            // only layer 0 can be HWC_BACKGROUND, so we can
            // unconditionally allow it without extra checks
            if (layer.compositionType == HWC_BACKGROUND) {
                windows_left--;
                continue;
            }

            size_t pixels_needed = WIDTH(layer.displayFrame) *
                    HEIGHT(layer.displayFrame);
            bool can_compose = windows_left && pixels_needed <= pixels_left;
            bool fimc_required = exynos4_requires_fimc(layer, handle->format);
            if (fimc_required)
                can_compose = can_compose && !fimc_used;

            // hwc_rect_t right and bottom values are normally exclusive;
            // the intersection logic is simpler if we make them inclusive
            hwc_rect_t visible_rect = layer.displayFrame;
            visible_rect.right--; visible_rect.bottom--;

            // no more than 2 layers can overlap on a given pixel
            for (size_t j = 0; can_compose && j < overlaps.size(); j++) {
                if (intersect(visible_rect, overlaps.itemAt(j)))
                    can_compose = false;
            }

            if (!can_compose) {
                layer.compositionType = HWC_FRAMEBUFFER;
                if (!fb_needed) {
                    first_fb = last_fb = i;
                    fb_needed = true;
                }
                else {
                    first_fb = min(i, first_fb);
                    last_fb = max(i, last_fb);
                }
                changed = true;
                break;
            }

            for (size_t j = 0; j < rects.size(); j++) {
                const hwc_rect_t &other_rect = rects.itemAt(j);
                if (intersect(visible_rect, other_rect))
                    overlaps.push_back(intersection(visible_rect, other_rect));
            }
            rects.push_back(visible_rect);
            pixels_left -= pixels_needed;
            windows_left--;
            if (fimc_required)
                fimc_used = true;
        }

        if (changed)
            for (size_t i = first_fb; i < last_fb; i++)
                contents->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
    } while(changed);

    unsigned int nextWindow = 0;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (fb_needed && i == first_fb) {
            ALOGV("assigning framebuffer to window %u\n",
                    nextWindow);
            nextWindow++;
            continue;
        }

        if (layer.compositionType != HWC_FRAMEBUFFER &&
                layer.compositionType != HWC_FRAMEBUFFER_TARGET) {
            ALOGV("assigning layer %u to window %u", i, nextWindow);
            pdev->bufs.overlay_map[nextWindow] = i;
            if (layer.compositionType == HWC_OVERLAY) {
                private_handle_t *handle =
                        private_handle_t::dynamicCast(layer.handle);
                if (exynos4_requires_fimc(layer, handle->format)) {
                    ALOGV("\tusing fimc %u", AVAILABLE_FIMC_UNITS[FIMD_FIMC_IDX]);
                    pdev->bufs.fimc_map[nextWindow].mode =
                            exynos4_fimc_map_t::FIMC_M2M;
                    pdev->bufs.fimc_map[nextWindow].idx = FIMD_FIMC_IDX;
                }
            }
            nextWindow++;
        }
    }

    if (!fimc_used)
        exynos4_cleanup_fimc_m2m(pdev, FIMD_FIMC_IDX);

    if (fb_needed)
        pdev->bufs.fb_window = first_fb;
    else
        pdev->bufs.fb_window = NO_FB_NEEDED;

    return 0;
}

static int exynos4_prepare_hdmi(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    ALOGV("preparing %u layers for HDMI", contents->numHwLayers);
    hwc_layer_1_t *video_layer = NULL;

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGV("\tlayer %u: framebuffer target", i);
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND) {
            ALOGV("\tlayer %u: background layer", i);
            dump_layer(&layer);
            continue;
        }

        if (layer.handle) {
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
            if (h->flags & GRALLOC_USAGE_PROTECTED) {
                if (!video_layer) {
                    video_layer = &layer;
                    layer.compositionType = HWC_OVERLAY;
                    ALOGV("\tlayer %u: video layer", i);
                    dump_layer(&layer);
                    continue;
                }
            }
        }

        layer.compositionType = HWC_FRAMEBUFFER;
        dump_layer(&layer);
    }

    return 0;
}

static int exynos4_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || !displays)
        return 0;

    exynos4_hwc_composer_device_1_t *pdev =
            (exynos4_hwc_composer_device_1_t *)dev;
    hwc_display_contents_1_t *fimd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];

    if (pdev->hdmi_hpd) {
        hdmi_enable(pdev);
    } else {
        hdmi_disable(pdev);
    }

    if (fimd_contents) {
        int err = exynos4_prepare_fimd(pdev, fimd_contents);
        if (err)
            return err;
    }

    if (hdmi_contents) {
        int err = exynos4_prepare_hdmi(pdev, hdmi_contents);
        if (err)
            return err;
    }

    return 0;
}

static int exynos4_config_fimc_m2m(hwc_layer_1_t &layer,
        alloc_device_t* alloc_device, exynos4_fimc_data_t *fimc_data,
        int fimc_idx, int dst_format, hwc_rect_t *sourceCrop)
{
    int i;
    ALOGV("configuring fimc %u for memory-to-memory", AVAILABLE_FIMC_UNITS[fimc_idx]);

    private_handle_t *src_handle = private_handle_t::dynamicCast(layer.handle);
    buffer_handle_t dst_buf;
    private_handle_t *dst_handle;
    int ret = 0;
    int uoffset = (GRALLOC_ALIGN(src_handle->stride, 16) * /*src_handle->vstride*/ALIGN(src_handle->height, 16));
    int voffset = uoffset + ((GRALLOC_ALIGN(src_handle->stride, 16) * /*src_handle->vstride*/ALIGN(src_handle->height, 16)) / 4);
    exynos_fimc_img src_cfg, dst_cfg;
    memset(&src_cfg, 0, sizeof(src_cfg));
    memset(&dst_cfg, 0, sizeof(dst_cfg));

    hwc_rect_t sourceCropTemp;
    if (!sourceCrop)
        sourceCrop = &sourceCropTemp;

    src_cfg.x = layer.sourceCrop.left;
    src_cfg.y = layer.sourceCrop.top;
    src_cfg.w = WIDTH(layer.sourceCrop);
    src_cfg.fw = src_handle->stride;
    src_cfg.h = HEIGHT(layer.sourceCrop);
    src_cfg.fh = ALIGN(src_handle->height, 16);//src_handle->vstride;
    src_cfg.yaddr = src_handle->base;

    if (exynos4_format_is_ycrcb(src_handle->format)) {
        src_cfg.uaddr = src_handle->base + voffset;
        src_cfg.vaddr = src_handle->base + uoffset;
    } else {
        src_cfg.uaddr = src_handle->base + uoffset;
        src_cfg.vaddr = src_handle->base + voffset;
    }
    src_cfg.format = src_handle->format;
    src_cfg.drmMode = !!(src_handle->flags & GRALLOC_USAGE_PROTECTED);
    src_cfg.acquireFenceFd = layer.acquireFenceFd;
    layer.acquireFenceFd = -1;

    dst_cfg.x = 0;
    dst_cfg.y = 0;
    dst_cfg.w = WIDTH(layer.displayFrame);
    dst_cfg.h = HEIGHT(layer.displayFrame);
    dst_cfg.rot = layer.transform;
    dst_cfg.drmMode = src_cfg.drmMode;
    dst_cfg.format = dst_format;
    dst_cfg.narrowRgb = !exynos4_format_is_rgb(src_handle->format);
    if (dst_cfg.drmMode)
        align_crop_and_center(dst_cfg.w, dst_cfg.h, sourceCrop,
                FIMC_DST_CROP_W_ALIGNMENT_RGB888);

    ALOGV("source configuration:");
    dump_fimc_img(src_cfg);

    bool reconfigure = fimc_src_cfg_changed(src_cfg, fimc_data->src_cfg) ||
            fimc_dst_cfg_changed(dst_cfg, fimc_data->dst_cfg);
    if (reconfigure) {
        int dst_stride;
        int usage = GRALLOC_USAGE_SW_READ_NEVER |
                GRALLOC_USAGE_SW_WRITE_NEVER |
                GRALLOC_USAGE_HW_COMPOSER;

        if (src_handle->flags & GRALLOC_USAGE_PROTECTED)
            usage |= GRALLOC_USAGE_PROTECTED;

        int w = ALIGN(dst_cfg.w, FIMC_DST_W_ALIGNMENT_RGB888);
        int h = ALIGN(dst_cfg.h, FIMC_DST_H_ALIGNMENT_RGB888);

        for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
            if (fimc_data->dst_buf[i]) {
                alloc_device->free(alloc_device, fimc_data->dst_buf[i]);
                fimc_data->dst_buf[i] = NULL;
            }

            if (fimc_data->dst_buf_fence[i] >= 0) {
                close(fimc_data->dst_buf_fence[i]);
                fimc_data->dst_buf_fence[i] = -1;
            }

            int ret = alloc_device->alloc(alloc_device, w, h,
                    HAL_PIXEL_FORMAT_RGBX_8888, usage, &fimc_data->dst_buf[i],
                    &dst_stride);
            if (ret < 0) {
                ALOGE("failed to allocate destination buffer: %s",
                        strerror(-ret));
                goto err_alloc;
            }
        }

        fimc_data->current_buf = 0;
    }

    dst_buf = fimc_data->dst_buf[fimc_data->current_buf];
    dst_handle = private_handle_t::dynamicCast(dst_buf);

    dst_cfg.fw = dst_handle->stride;
    dst_cfg.fh = ALIGN(dst_cfg.h, 16); // TODO probably incorrect
    dst_cfg.yaddr = dst_handle->base;
    dst_cfg.acquireFenceFd = fimc_data->dst_buf_fence[fimc_data->current_buf];
    fimc_data->dst_buf_fence[fimc_data->current_buf] = -1;

    ALOGV("destination configuration:");
    dump_fimc_img(dst_cfg);

    if ((int)dst_cfg.w != WIDTH(layer.displayFrame))
        ALOGV("padding %u x %u output to %u x %u and cropping to {%u,%u,%u,%u}",
                WIDTH(layer.displayFrame), HEIGHT(layer.displayFrame),
                dst_cfg.w, dst_cfg.h, sourceCrop->left, sourceCrop->top,
                sourceCrop->right, sourceCrop->bottom);

    if (fimc_data->fimc) {
        ALOGV("reusing open fimc %u", AVAILABLE_FIMC_UNITS[fimc_idx]);
    } else {
        ALOGV("opening fimc %u", AVAILABLE_FIMC_UNITS[fimc_idx]);
        fimc_data->fimc = exynos_fimc_create_exclusive(
                AVAILABLE_FIMC_UNITS[fimc_idx], FIMC_M2M_MODE, FIMC_DUMMY, src_cfg.drmMode);
        if (!fimc_data->fimc) {
            ALOGE("failed to create fimc handle");
            ret = -1;
            goto err_alloc;
        }
    }

    if (reconfigure) {
        ret = exynos_fimc_stop_exclusive(fimc_data->fimc);
        if (ret < 0) {
            ALOGE("failed to stop fimc %u", fimc_idx);
            goto err_fimc_config;
        }

        ret = exynos_fimc_config_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
        if (ret < 0) {
            ALOGE("failed to configure fimc %u", fimc_idx);
            goto err_fimc_config;
        }
    }

    ret = exynos_fimc_run_exclusive(fimc_data->fimc, &src_cfg, &dst_cfg);
    if (ret < 0) {
        ALOGE("failed to run fimc %u", fimc_idx);
        goto err_fimc_config;
    }

    fimc_data->src_cfg = src_cfg;
    fimc_data->dst_cfg = dst_cfg;

    layer.releaseFenceFd = src_cfg.releaseFenceFd;

    return 0;

err_fimc_config:
    exynos_fimc_destroy(fimc_data->fimc);
    fimc_data->fimc = NULL;
err_alloc:
    if (src_cfg.acquireFenceFd >= 0)
        close(src_cfg.acquireFenceFd);
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
       if (fimc_data->dst_buf[i]) {
           alloc_device->free(alloc_device, fimc_data->dst_buf[i]);
           fimc_data->dst_buf[i] = NULL;
       }
       if (fimc_data->dst_buf_fence[i] >= 0) {
           close(fimc_data->dst_buf_fence[i]);
           fimc_data->dst_buf_fence[i] = -1;
       }
    }
    memset(&fimc_data->src_cfg, 0, sizeof(fimc_data->src_cfg));
    memset(&fimc_data->dst_cfg, 0, sizeof(fimc_data->dst_cfg));
    return ret;
}


static void exynos4_cleanup_fimc_m2m(exynos4_hwc_composer_device_1_t *pdev,
        size_t fimc_idx)
{
    exynos4_fimc_data_t &fimc_data = pdev->fimc[fimc_idx];
    if (!fimc_data.fimc)
        return;

    ALOGV("closing fimc %u", AVAILABLE_FIMC_UNITS[fimc_idx]);

    exynos_fimc_stop_exclusive(fimc_data.fimc);
    exynos_fimc_destroy(fimc_data.fimc);
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++) {
        if (fimc_data.dst_buf[i])
            pdev->alloc_device->free(pdev->alloc_device, fimc_data.dst_buf[i]);
        if (fimc_data.dst_buf_fence[i] >= 0)
            close(fimc_data.dst_buf_fence[i]);
    }

    memset(&fimc_data, 0, sizeof(fimc_data));
    for (size_t i = 0; i < NUM_FIMC_DST_BUFS; i++)
        fimc_data.dst_buf_fence[i] = -1;
}

static void exynos4_config_handle(private_handle_t *handle,
        hwc_rect_t &sourceCrop, hwc_rect_t &displayFrame,
        int32_t blending, int fence_fd, s3c_fb_win_config &cfg,
        exynos4_hwc_composer_device_1_t *pdev)
{
    uint32_t x, y;
    uint32_t w = WIDTH(displayFrame);
    uint32_t h = HEIGHT(displayFrame);
    uint8_t bpp = exynos4_format_to_bpp(handle->format);
    uint32_t offset = (sourceCrop.top * handle->stride + sourceCrop.left) * bpp / 8;

    if (displayFrame.left < 0) {
        unsigned int crop = -displayFrame.left;
        ALOGV("layer off left side of screen; cropping %u pixels from left edge",
                crop);
        x = 0;
        w -= crop;
        offset += crop * bpp / 8;
    } else {
        x = displayFrame.left;
    }

    if (displayFrame.right > pdev->xres) {
        unsigned int crop = displayFrame.right - pdev->xres;
        ALOGV("layer off right side of screen; cropping %u pixels from right edge",
                crop);
        w -= crop;
    }

    if (displayFrame.top < 0) {
        unsigned int crop = -displayFrame.top;
        ALOGV("layer off top side of screen; cropping %u pixels from top edge",
                crop);
        y = 0;
        h -= crop;
        offset += handle->stride * crop * bpp / 8;
    } else {
        y = displayFrame.top;
    }

    if (displayFrame.bottom > pdev->yres) {
        int crop = displayFrame.bottom - pdev->yres;
        ALOGV("layer off bottom side of screen; cropping %u pixels from bottom edge",
                crop);
        h -= crop;
    }

    cfg.state = cfg.S3C_FB_WIN_STATE_BUFFER;
    cfg.fd = -1;//handle->share_fd;
    cfg.x = x;
    cfg.y = y;
    cfg.w = w;
    cfg.h = h;
    cfg.format = exynos4_format_to_s3c_format(handle->format);
    cfg.offset = offset;
    cfg.stride = handle->stride * bpp / 8;
    cfg.blending = exynos4_blending_to_s3c_blending(blending);
    cfg.fence_fd = fence_fd;
}

static void exynos4_config_overlay(hwc_layer_1_t *layer, s3c_fb_win_config &cfg,
        exynos4_hwc_composer_device_1_t *pdev)
{
    if (layer->compositionType == HWC_BACKGROUND) {
        hwc_color_t color = layer->backgroundColor;
        cfg.state = cfg.S3C_FB_WIN_STATE_COLOR;
        cfg.color = (color.r << 16) | (color.g << 8) | color.b;
        cfg.x = 0;
        cfg.y = 0;
        cfg.w = pdev->xres;
        cfg.h = pdev->yres;
        return;
    }

    private_handle_t *handle = private_handle_t::dynamicCast(layer->handle);
    exynos4_config_handle(handle, layer->sourceCrop, layer->displayFrame,
            layer->blending, layer->acquireFenceFd, cfg, pdev);
}

static int exynos4_post_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    exynos4_hwc_post_data_t *pdata = &pdev->bufs;
    struct s3c_fb_win_config_data win_data;
    struct s3c_fb_win_config *config = win_data.config;

    memset(config, 0, sizeof(win_data.config));
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        config[i].fence_fd = -1;

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        int layer_idx = pdata->overlay_map[i];
        if (layer_idx != -1 && contents->hwLayers[layer_idx].handle) {
            hwc_layer_1_t &layer = contents->hwLayers[layer_idx];
            private_handle_t *handle =
                    private_handle_t::dynamicCast(layer.handle);

            if (pdata->fimc_map[i].mode == exynos4_fimc_map_t::FIMC_M2M) {
                int fimc_idx = pdata->fimc_map[i].idx;
                exynos4_fimc_data_t &fimc = pdev->fimc[fimc_idx];

                // RGBX8888 surfaces are already in the right color order from the GPU,
                // RGB565 and YUV surfaces need the FIMC to swap R & B
                int dst_format = HAL_PIXEL_FORMAT_RGBA_8888;
                if (exynos4_format_is_rgb(handle->format) &&
                                handle->format != HAL_PIXEL_FORMAT_RGB_565)
                    dst_format = HAL_PIXEL_FORMAT_RGBX_8888;

                hwc_rect_t sourceCrop = { 0, 0,
                        WIDTH(layer.displayFrame), HEIGHT(layer.displayFrame) };
                int err = exynos4_config_fimc_m2m(layer, pdev->alloc_device, &fimc,
                        fimc_idx, dst_format, &sourceCrop);
                if (err < 0) {
                    ALOGE("failed to configure fimc %u for layer %u",
                            fimc_idx, i);
                    pdata->fimc_map[i].mode = exynos4_fimc_map_t::FIMC_NONE;
                    continue;
                }

                buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
                private_handle_t *dst_handle =
                        private_handle_t::dynamicCast(dst_buf);
                int fence = fimc.dst_cfg.releaseFenceFd;
                dst_handle->format = HAL_PIXEL_FORMAT_BGRA_8888;
                exynos4_config_handle(dst_handle, sourceCrop,
                        layer.displayFrame, layer.blending, fence, config[i],
                        pdev);
            } else {
                exynos4_config_overlay(&layer, config[i], pdev);
            }
        }
        if (i == 0 && config[i].blending != S3C_FB_BLENDING_NONE) {
            ALOGV("blending not supported on window 0; forcing BLENDING_NONE");
            config[i].blending = S3C_FB_BLENDING_NONE;
        }

        ALOGV("window %u configuration:", i);
        dump_config(config[i]);
    }

    int ret = ioctl(pdev->fd, S3CFB_WIN_CONFIG, &win_data);
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++)
        if (config[i].fence_fd != -1)
            close(config[i].fence_fd);
    if (ret < 0) {
        ALOGE("ioctl S3CFB_WIN_CONFIG failed: %s", strerror(errno));
        return ret;
    }

    memcpy(pdev->last_config, &win_data.config, sizeof(win_data.config));
    memcpy(pdev->last_fimc_map, pdata->fimc_map, sizeof(pdata->fimc_map));
    pdev->last_fb_window = pdata->fb_window;
    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        int layer_idx = pdata->overlay_map[i];
        if (layer_idx != -1) {
            hwc_layer_1_t &layer = contents->hwLayers[layer_idx];
            pdev->last_handles[i] = layer.handle;
        }
    }

    return win_data.fence;
}

static int exynos4_clear_fimd(exynos4_hwc_composer_device_1_t *pdev)
{
    struct s3c_fb_win_config_data win_data;
    memset(&win_data, 0, sizeof(win_data));

    int ret = ioctl(pdev->fd, S3CFB_WIN_CONFIG, &win_data);
  //  LOG_ALWAYS_FATAL_IF(ret < 0,
  //          "ioctl S3CFB_WIN_CONFIG failed to clear screen: %s",
  //          strerror(errno));
    // the causes of an empty config failing are all unrecoverable

    return win_data.fence;
}

static int exynos4_set_fimd(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    if (!contents->dpy || !contents->sur)
        return 0;

    hwc_layer_1_t *fb_layer = NULL;
    int err = 0;

    if (pdev->bufs.fb_window != NO_FB_NEEDED) {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            if (contents->hwLayers[i].compositionType ==
                    HWC_FRAMEBUFFER_TARGET) {
                pdev->bufs.overlay_map[pdev->bufs.fb_window] = i;
                fb_layer = &contents->hwLayers[i];
                break;
            }
        }

        if (CC_UNLIKELY(!fb_layer)) {
            ALOGE("framebuffer target expected, but not provided");
            err = -EINVAL;
        } else {
            ALOGV("framebuffer target buffer:");
            dump_layer(fb_layer);
        }
    }

    int fence;
    if (!err) {
        fence = exynos4_post_fimd(pdev, contents);
        if (fence < 0)
            err = fence;
    }

    if (err)
        fence = exynos4_clear_fimd(pdev);

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        if (pdev->bufs.overlay_map[i] != -1) {
            hwc_layer_1_t &layer =
                    contents->hwLayers[pdev->bufs.overlay_map[i]];
            int dup_fd = dup(fence);
            if (dup_fd < 0)
                ALOGW("release fence dup failed: %s", strerror(errno));
            if (pdev->bufs.fimc_map[i].mode == exynos4_fimc_map_t::FIMC_M2M) {
                int fimc_idx = pdev->bufs.fimc_map[i].idx;
                exynos4_fimc_data_t &fimc = pdev->fimc[fimc_idx];
                fimc.dst_buf_fence[fimc.current_buf] = dup_fd;
                fimc.current_buf = (fimc.current_buf + 1) % NUM_FIMC_DST_BUFS;
            } else {
                layer.releaseFenceFd = dup_fd;
            }
        }
    }
    close(fence);

    return err;
}

static int exynos4_set_hdmi(exynos4_hwc_composer_device_1_t *pdev,
        hwc_display_contents_1_t* contents)
{
    hwc_layer_1_t *fb_layer = NULL;
    hwc_layer_1_t *video_layer = NULL;

    if (!pdev->hdmi_enabled) {
        for (size_t i = 0; i < contents->numHwLayers; i++) {
            hwc_layer_1_t &layer = contents->hwLayers[i];
            if (layer.acquireFenceFd != -1) {
                close(layer.acquireFenceFd);
                layer.acquireFenceFd = -1;
            }
        }
        return 0;
    }

    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.flags & HWC_SKIP_LAYER) {
            ALOGV("HDMI skipping layer %d", i);
            continue;
        }

        if (layer.compositionType == HWC_OVERLAY) {
             if (!layer.handle)
                continue;

            ALOGV("HDMI video layer:");
            dump_layer(&layer);
#ifdef USE_MIXER_VP
            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
            hdmi_vp_output(pdev, pdev->hdmi_layers[0], layer, h, layer.acquireFenceFd,
                                                             &layer.releaseFenceFd);
            video_layer = &layer;
#else
            exynos4_fimc_data_t &fimc = pdev->fimc[HDMI_FIMC_IDX];
            int ret = exynos4_config_fimc_m2m(layer, pdev->alloc_device, &fimc, 1,
                                             HAL_PIXEL_FORMAT_RGBX_8888, NULL);
            if (ret < 0) {
                ALOGE("failed to configure fimc for video layer");
                continue;
            }

            buffer_handle_t dst_buf = fimc.dst_buf[fimc.current_buf];
            private_handle_t *h = private_handle_t::dynamicCast(dst_buf);

            int acquireFenceFd = fimc.dst_cfg.releaseFenceFd;
            int releaseFenceFd = -1;

            hdmi_output(pdev, pdev->hdmi_layers[0], layer, h, acquireFenceFd,
                                                             &releaseFenceFd);
            video_layer = &layer;

            fimc.dst_buf_fence[fimc.current_buf] = releaseFenceFd;
            fimc.current_buf = (fimc.current_buf + 1) % NUM_FIMC_DST_BUFS;
#endif
        }

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            if (!layer.handle)
                continue;

            ALOGV("HDMI FB layer:");
            dump_layer(&layer);

            private_handle_t *h = private_handle_t::dynamicCast(layer.handle);
            hdmi_output(pdev, pdev->hdmi_layers[1], layer, h, layer.acquireFenceFd,
                                                             &layer.releaseFenceFd);
            fb_layer = &layer;
        }
    }

    if (!video_layer) {
        hdmi_disable_layer(pdev, pdev->hdmi_layers[0]);
#ifndef USE_MIXER_VP
        exynos4_cleanup_fimc_m2m(pdev, HDMI_FIMC_IDX);
#endif
    }
    if (!fb_layer)
        hdmi_disable_layer(pdev, pdev->hdmi_layers[1]);


    return 0;
}

static int exynos4_set(struct hwc_composer_device_1 *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    if (!numDisplays || !displays)
        return 0;

    exynos4_hwc_composer_device_1_t *pdev =
            (exynos4_hwc_composer_device_1_t *)dev;
    hwc_display_contents_1_t *fimd_contents = displays[HWC_DISPLAY_PRIMARY];
    hwc_display_contents_1_t *hdmi_contents = displays[HWC_DISPLAY_EXTERNAL];
    int fimd_err = 0, hdmi_err = 0;

    if (fimd_contents)
        fimd_err = exynos4_set_fimd(pdev, fimd_contents);

    if (hdmi_contents)
        hdmi_err = exynos4_set_hdmi(pdev, hdmi_contents);

    if (fimd_err)
        return fimd_err;

    return hdmi_err;
}

static void exynos4_registerProcs(struct hwc_composer_device_1* dev,
        hwc_procs_t const* procs)
{
    struct exynos4_hwc_composer_device_1_t* pdev =
            (struct exynos4_hwc_composer_device_1_t*)dev;
    pdev->procs = procs;
}

static int exynos4_query(struct hwc_composer_device_1* dev, int what, int *value)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we support the background layer
        value[0] = 1;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = pdev->vsync_period;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int exynos4_eventControl(struct hwc_composer_device_1 *dev, int dpy,
        int event, int enabled)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        __u32 val = !!enabled;
        int err = ioctl(pdev->fd, S3CFB_SET_VSYNC_INT, &val);
        if (err < 0) {
            ALOGE("vsync ioctl failed");
            return -errno;
        }

        return 0;
    }

    return -EINVAL;
}

static void handle_hdmi_uevent(struct exynos4_hwc_composer_device_1_t *pdev,
        const char *buff, int len)
{
    const char *s = buff;
    s += strlen(s) + 1;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
            pdev->hdmi_hpd = atoi(s + strlen("SWITCH_STATE=")) == 1;

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    if (pdev->hdmi_hpd) {
        hdmi_display_edid_info(pdev);
        if(hdmi_open(pdev)< 0)
            return;
        if (hdmi_get_config(pdev)) {
            ALOGE("Error reading HDMI configuration");
            pdev->hdmi_hpd = false;
            hdmi_close(pdev);
            return;
        }

        pdev->hdmi_blanked = false;
    }

    ALOGV("HDMI HPD changed to %s", pdev->hdmi_hpd ? "enabled" : "disabled");
    if (pdev->hdmi_hpd)
        ALOGI("HDMI Resolution changed to %dx%d", pdev->hdmi_h, pdev->hdmi_w);

    /* hwc_dev->procs is set right after the device is opened, but there is
     * still a race condition where a hotplug event might occur after the open
     * but before the procs are registered. */
    if (pdev->procs)
        pdev->procs->hotplug(pdev->procs, HWC_DISPLAY_EXTERNAL, pdev->hdmi_hpd);
}

static void handle_vsync_event(struct exynos4_hwc_composer_device_1_t *pdev)
{
    if (!pdev->procs)
        return;

    int err = lseek(pdev->vsync_fd, 0, SEEK_SET);
    if (err < 0) {
        ALOGE("error seeking to vsync timestamp: %s", strerror(errno));
        return;
    }

    char buf[4096];
    err = read(pdev->vsync_fd, buf, sizeof(buf));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return;
    }
    buf[sizeof(buf) - 1] = '\0';

    errno = 0;
    uint64_t timestamp = strtoull(buf, NULL, 0);
    if (!errno)
        pdev->procs->vsync(pdev->procs, 0, timestamp);
}

static void *hwc_vsync_thread(void *data)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)data;
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    uevent_init();

    char temp[4096];
    int err = read(pdev->vsync_fd, temp, sizeof(temp));
    if (err < 0) {
        ALOGE("error reading vsync timestamp: %s", strerror(errno));
        return NULL;
    }

    struct pollfd fds[2];
    fds[0].fd = pdev->vsync_fd;
    fds[0].events = POLLPRI;
    fds[1].fd = uevent_get_fd();
    fds[1].events = POLLIN;

    while (true) {
        int err = poll(fds, 2, -1);

        if (err > 0) {
            if (fds[0].revents & POLLPRI) {
                handle_vsync_event(pdev);
            }
            else if (fds[1].revents & POLLIN) {
                int len = uevent_next_event(uevent_desc,
                        sizeof(uevent_desc) - 2);

                bool hdmi = !strcmp(uevent_desc,
                        "change@/devices/virtual/switch/hdmi");
                if (hdmi)
                    handle_hdmi_uevent(pdev, uevent_desc, len);
            }
        }
        else if (err == -1) {
            if (errno == EINTR)
                break;
            ALOGE("error in vsync thread: %s", strerror(errno));
        }
    }

    return NULL;
}

static int exynos4_blank(struct hwc_composer_device_1 *dev, int disp, int blank)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    switch (disp) {
    case HWC_DISPLAY_PRIMARY: {
        if (pdev->hdmi_hpd) {
            if (blank && !pdev->hdmi_blanked)
                hdmi_disable(pdev);
            pdev->hdmi_blanked = !!blank;
        }

        int fb_blank = blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK;
        int err = ioctl(pdev->fd, FBIOBLANK, fb_blank);
        if (err < 0) {
            if (errno == EBUSY)
                ALOGI("%sblank ioctl failed (display already %sblanked)",
                        blank ? "" : "un", blank ? "" : "un");
            else
                ALOGE("%sblank ioctl failed: %s", blank ? "" : "un",
                        strerror(errno));
            return -errno;
        }
        break;
    }

    case HWC_DISPLAY_EXTERNAL:
        if (pdev->hdmi_hpd) {
            if (blank && !pdev->hdmi_blanked)
                hdmi_disable(pdev);
            pdev->hdmi_blanked = !!blank;
        }
        break;

    default:
        return -EINVAL;

    }

    return 0;
}

static void exynos4_dump(hwc_composer_device_1* dev, char *buff, int buff_len)
{
    if (buff_len <= 0)
        return;

    struct exynos4_hwc_composer_device_1_t *pdev =
            (struct exynos4_hwc_composer_device_1_t *)dev;

    android::String8 result;

    result.appendFormat("  hdmi_enabled=%u\n", pdev->hdmi_enabled);
    if (pdev->hdmi_enabled)
        result.appendFormat("    w=%u, h=%u\n", pdev->hdmi_w, pdev->hdmi_h);
    result.append(
            "   type   |  handle  |  color   | blend | format |   position    |     size      | fimc \n"
            "----------+----------|----------+-------+--------+---------------+---------------------\n");
    //        8_______ | 8_______ | 8_______ | 5____ | 6_____ | [5____,5____] | [5____,5____] | 3__ \n"

    for (size_t i = 0; i < NUM_HW_WINDOWS; i++) {
        struct s3c_fb_win_config &config = pdev->last_config[i];
        if (config.state == config.S3C_FB_WIN_STATE_DISABLED) {
            result.appendFormat(" %8s | %8s | %8s | %5s | %6s | %13s | %13s",
                    "DISABLED", "-", "-", "-", "-", "-", "-");
        }
        else {
            if (config.state == config.S3C_FB_WIN_STATE_COLOR)
                result.appendFormat(" %8s | %8s | %8x | %5s | %6s", "COLOR",
                        "-", config.color, "-", "-");
            else
                result.appendFormat(" %8s | %8x | %8s | %5x | %6x",
                        pdev->last_fb_window == i ? "FB" : "OVERLAY",
                        intptr_t(pdev->last_handles[i]),
                        "-", config.blending, config.format);

            result.appendFormat(" | [%5d,%5d] | [%5u,%5u]", config.x, config.y,
                    config.w, config.h);
        }
        if (pdev->last_fimc_map[i].mode == exynos4_fimc_map_t::FIMC_NONE)
            result.appendFormat(" | %3s", "-");
        else
            result.appendFormat(" | %3d",
                    AVAILABLE_FIMC_UNITS[pdev->last_fimc_map[i].idx]);
        result.append("\n");
    }

    strlcpy(buff, result.string(), buff_len);
}

static int exynos4_getDisplayConfigs(struct hwc_composer_device_1 *dev,
        int disp, uint32_t *configs, size_t *numConfigs)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
               (struct exynos4_hwc_composer_device_1_t *)dev;

    if (*numConfigs == 0)
        return 0;

    if (disp == HWC_DISPLAY_PRIMARY) {
        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
        if (!pdev->hdmi_hpd) {
            return -EINVAL;
        }

        int err = hdmi_get_config(pdev);
        if (err) {
            return -EINVAL;
        }

        configs[0] = 0;
        *numConfigs = 1;
        return 0;
    }

    return -EINVAL;
}

static int32_t exynos4_fimd_attribute(struct exynos4_hwc_composer_device_1_t *pdev,
        const uint32_t attribute)
{
    switch(attribute) {
    case HWC_DISPLAY_VSYNC_PERIOD:
        return pdev->vsync_period;

    case HWC_DISPLAY_WIDTH:
        return pdev->xres;

    case HWC_DISPLAY_HEIGHT:
        return pdev->yres;

    case HWC_DISPLAY_DPI_X:
        return pdev->xdpi;

    case HWC_DISPLAY_DPI_Y:
        return pdev->ydpi;

    default:
        ALOGE("unknown display attribute %u", attribute);
        return -EINVAL;
    }
}

static int32_t exynos4_hdmi_attribute(struct exynos4_hwc_composer_device_1_t *pdev,
        const uint32_t attribute)
{
    switch(attribute) {
    case HWC_DISPLAY_VSYNC_PERIOD:
        return pdev->vsync_period;

    case HWC_DISPLAY_WIDTH:
        return pdev->hdmi_w;

    case HWC_DISPLAY_HEIGHT:
        return pdev->hdmi_h;

    case HWC_DISPLAY_DPI_X:
    case HWC_DISPLAY_DPI_Y:
        return 0; // unknown

    default:
        ALOGE("unknown display attribute %u", attribute);
        return -EINVAL;
    }
}

static int exynos4_getDisplayAttributes(struct hwc_composer_device_1 *dev,
        int disp, uint32_t config, const uint32_t *attributes, int32_t *values)
{
    struct exynos4_hwc_composer_device_1_t *pdev =
                   (struct exynos4_hwc_composer_device_1_t *)dev;

    for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; i++) {
        if (disp == HWC_DISPLAY_PRIMARY)
            values[i] = exynos4_fimd_attribute(pdev, attributes[i]);
        else if (disp == HWC_DISPLAY_EXTERNAL)
            values[i] = exynos4_hdmi_attribute(pdev, attributes[i]);
        else {
            ALOGE("unknown display type %u", disp);
            return -EINVAL;
        }
    }

    return 0;
}

static int exynos4_close(hw_device_t* device);

static int exynos4_open(const struct hw_module_t *module, const char *name,
        struct hw_device_t **device)
{
    int ret;
    int refreshRate;
    int sw_fd;

    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        return -EINVAL;
    }

    struct exynos4_hwc_composer_device_1_t *dev;
    dev = (struct exynos4_hwc_composer_device_1_t *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));

    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
            (const struct hw_module_t **)&dev->gralloc_module)) {
        ALOGE("failed to get gralloc hw module");
        ret = -EINVAL;
        goto err_get_module;
    }

    if (gralloc_open((const hw_module_t *)dev->gralloc_module,
            &dev->alloc_device)) {
        ALOGE("failed to open gralloc");
        ret = -EINVAL;
        goto err_get_module;
    }

    dev->fd = open("/dev/graphics/fb0", O_RDWR);
    if (dev->fd < 0) {
        ALOGE("failed to open framebuffer");
        ret = dev->fd;
        goto err_open_fb;
    }

    struct fb_var_screeninfo info;
    if (ioctl(dev->fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("FBIOGET_VSCREENINFO ioctl failed: %s", strerror(errno));
        ret = -errno;
        goto err_ioctl;
    }

    refreshRate = 1000000000000LLU /
        (
         uint64_t( info.upper_margin + info.lower_margin + info.yres )
         * ( info.left_margin  + info.right_margin + info.xres )
         * info.pixclock
        );

    if (refreshRate == 0) {
        ALOGW("invalid refresh rate, assuming 60 Hz");
        refreshRate = 60;
    }

    dev->xres = info.xres;
    dev->yres = info.yres;
    dev->xdpi = 1000 * (info.xres * 25.4f) / info.width;
    dev->ydpi = 1000 * (info.yres * 25.4f) / info.height;
    dev->vsync_period  = 1000000000 / refreshRate;

    ALOGV("using\n"
          "xres         = %d px\n"
          "yres         = %d px\n"
          "width        = %d mm (%f dpi)\n"
          "height       = %d mm (%f dpi)\n"
          "refresh rate = %d Hz\n",
          dev->xres, dev->yres, info.width, dev->xdpi / 1000.0,
          info.height, dev->ydpi / 1000.0, refreshRate);

    for (size_t i = 0; i < NUM_FIMC_UNITS; i++)
        for (size_t j = 0; j < NUM_FIMC_DST_BUFS; j++)
            dev->fimc[i].dst_buf_fence[j] = -1;

    dev->vsync_fd = open("/sys/devices/platform/exynos4-fb.0/vsync", O_RDONLY);
    if (dev->vsync_fd < 0) {
        ALOGE("failed to open vsync attribute");
        ret = dev->vsync_fd;
        goto err_ioctl;
    }

    sw_fd = open("/sys/class/switch/hdmi/state", O_RDONLY);
    if (sw_fd) {
        char val;
        if (read(sw_fd, &val, 1) == 1 && val == '1') {
	    if(hdmi_open(dev) < 0)
		goto err_vsync;
            dev->hdmi_hpd = true;
            hdmi_display_edid_info(dev);
            if (hdmi_get_config(dev)) {
                ALOGE("Error reading HDMI configuration");
		hdmi_close(dev);
                dev->hdmi_hpd = false;
            }
            EDIDClose();
        }
    }

    dev->base.common.tag = HARDWARE_DEVICE_TAG;
    dev->base.common.version = HWC_DEVICE_API_VERSION_1_1;
    dev->base.common.module = const_cast<hw_module_t *>(module);
    dev->base.common.close = exynos4_close;

    dev->base.prepare = exynos4_prepare;
    dev->base.set = exynos4_set;
    dev->base.eventControl = exynos4_eventControl;
    dev->base.blank = exynos4_blank;
    dev->base.query = exynos4_query;
    dev->base.registerProcs = exynos4_registerProcs;
    dev->base.dump = exynos4_dump;
    dev->base.getDisplayConfigs = exynos4_getDisplayConfigs;
    dev->base.getDisplayAttributes = exynos4_getDisplayAttributes;

    *device = &dev->base.common;

    ret = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (ret) {
        ALOGE("failed to start vsync thread: %s", strerror(ret));
        ret = -ret;
        goto err_vsync;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.force_gpu", value, "0");
    dev->force_gpu = atoi(value);

    return 0;

err_vsync:
    close(dev->vsync_fd);
err_ioctl:
    close(dev->fd);
err_open_fb:
    gralloc_close(dev->alloc_device);
err_get_module:
    free(dev);
    return ret;
}

static int exynos4_close(hw_device_t *device)
{
    struct exynos4_hwc_composer_device_1_t *dev =
            (struct exynos4_hwc_composer_device_1_t *)device;
    pthread_kill(dev->vsync_thread, SIGTERM);
    pthread_join(dev->vsync_thread, NULL);
    for (size_t i = 0; i < NUM_FIMC_UNITS; i++)
        exynos4_cleanup_fimc_m2m(dev, i);
    gralloc_close(dev->alloc_device);
    close(dev->vsync_fd);

    close(dev->fd);
    return 0;
}

static struct hw_module_methods_t exynos4_hwc_module_methods = {
    open: exynos4_open,
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: HWC_MODULE_API_VERSION_0_1,
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Samsung exynos4 hwcomposer module",
        author: "Google",
        methods: &exynos4_hwc_module_methods,
    }
};
