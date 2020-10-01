/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012 The CyanogenMod Project <http://www.cyanogenmod.org>
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
 *
 * @author Rama, Meka(v.meka@samsung.com)
           Sangwoo, Park(sw5771.park@samsung.com)
           Jamie Oh (jung-min.oh@samsung.com)
 * @date   2011-03-11
 *
 */

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <EGL/egl.h>
#include <fcntl.h>
#include <hardware_legacy/uevent.h>
#include <sys/prctl.h>
#include <sys/resource.h>

#include "SecHWCUtils.h"

#include "gralloc_priv.h"

#include <GLES/gl.h>

#if defined(BOARD_USES_HDMI)
#include "SecHdmiClient.h"
#include "SecTVOutService.h"

#include "SecHdmi.h"

//#define CHECK_EGL_FPS
#ifdef CHECK_EGL_FPS
extern void check_fps();
#endif

static int lcd_width, lcd_height;
static int prev_usage = 0;

#define CHECK_TIME_DEBUG 0
#define SUPPORT_AUTO_UI_ROTATE
#endif
int testRenderNum =0;

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 2,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Samsung S5PC21X hwcomposer module",
        author: "SAMSUNG",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_1_t const* l) {
    SEC_HWC_Log(HWC_LOG_DEBUG,"\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

void calculate_rect(struct hwc_win_info_t *win, hwc_layer_1_t *cur,
        sec_rect *rect)
{
    rect->x = cur->displayFrame.left;
    rect->y = cur->displayFrame.top;
    rect->w = cur->displayFrame.right - cur->displayFrame.left;
    rect->h = cur->displayFrame.bottom - cur->displayFrame.top;

    if (rect->x < 0) {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres;
        else
            rect->w = rect->w + rect->x;
        rect->x = 0;
    } else {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres - rect->x;
    }
    if (rect->y < 0) {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres;
        else
            rect->h = rect->h + rect->y;
        rect->y = 0;
    } else {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres - rect->y;
    }
}

static int set_src_dst_img_rect(hwc_layer_1_t *cur,
        struct hwc_win_info_t *win,
        struct sec_img *src_img,
        struct sec_img *dst_img,
        struct sec_rect *src_rect,
        struct sec_rect *dst_rect,
        int win_idx)
{
    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    sec_rect rect;

    /* 1. Set src_img from prev_handle */
    src_img->f_w     = prev_handle->width;
    src_img->f_h     = prev_handle->height;
    src_img->w       = prev_handle->width;
    src_img->h       = prev_handle->height;
    src_img->format  = prev_handle->format;
    src_img->base    = (uint32_t)prev_handle->base;
    src_img->offset  = prev_handle->offset;
    src_img->mem_id  = prev_handle->fd;
    src_img->paddr  = prev_handle->paddr;
    src_img->usage  = prev_handle->usage;
    src_img->uoffset  = prev_handle->uoffset;
    src_img->voffset  = prev_handle->voffset;

    src_img->mem_type = HWC_VIRT_MEM_TYPE;

    switch (src_img->format) {
    case HAL_PIXEL_FORMAT_YV12:             /* To support video editor */
    case HAL_PIXEL_FORMAT_YCbCr_420_P:      /* To support SW codec     */
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
        src_img->f_w = (src_img->f_w + 15) & ~15;
        src_img->f_h = (src_img->f_h + 1) & ~1;
        break;
    default:
        src_img->f_w = src_img->w;
        src_img->f_h = src_img->h;
        break;
    }

    /* 2. Set dst_img from window(lcd) */
    calculate_rect(win, cur, &rect);
    dst_img->f_w = win->lcd_info.xres;
    dst_img->f_h = win->lcd_info.yres;
    dst_img->w = rect.w;
    dst_img->h = rect.h;

    switch (win->lcd_info.bits_per_pixel) {
    case 32:
        dst_img->format = HAL_PIXEL_FORMAT_RGBX_8888;
        break;
    default:
        dst_img->format = HAL_PIXEL_FORMAT_RGB_565;
        break;
    }

    dst_img->base     = win->addr[win->buf_index];
    dst_img->offset   = 0;
    dst_img->mem_id   = 0;
    dst_img->mem_type = HWC_PHYS_MEM_TYPE;

    /* 3. Set src_rect(crop rect) */
    if (cur->displayFrame.left < 0) {
        src_rect->x =
            (0 - cur->displayFrame.left)
            *(src_img->w)
            /(cur->displayFrame.right - cur->displayFrame.left);
        if (cur->displayFrame.right > win->lcd_info.xres) {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left) -
                src_rect->x -
                (cur->displayFrame.right - win->lcd_info.xres)
                *(src_img->w)
                /(cur->displayFrame.right - cur->displayFrame.left);
        } else {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left) -
                src_rect->x;
        }
    } else {
        src_rect->x = cur->sourceCrop.left;
        if (cur->displayFrame.right > win->lcd_info.xres) {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left) -
                src_rect->x -
                (cur->displayFrame.right - win->lcd_info.xres)
                *(src_img->w)
                /(cur->displayFrame.right - cur->displayFrame.left);
        } else {
            src_rect->w =
                (cur->sourceCrop.right - cur->sourceCrop.left);
        }
    }
    if (cur->displayFrame.top < 0) {
        src_rect->y =
            (0 - cur->displayFrame.top)
            *(src_img->h)
            /(cur->displayFrame.bottom - cur->displayFrame.top);
        if (cur->displayFrame.bottom > win->lcd_info.yres) {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top) -
                src_rect->y -
                (cur->displayFrame.bottom - win->lcd_info.yres)
                *(src_img->h)
                /(cur->displayFrame.bottom - cur->displayFrame.top);
        } else {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top) -
                src_rect->y;
        }
    } else {
        src_rect->y = cur->sourceCrop.top;
        if (cur->displayFrame.bottom > win->lcd_info.yres) {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top) -
                src_rect->y -
                (cur->displayFrame.bottom - win->lcd_info.yres)
                *(src_img->h)
                /(cur->displayFrame.bottom - cur->displayFrame.top);
        } else {
            src_rect->h =
                (cur->sourceCrop.bottom - cur->sourceCrop.top);
        }
    }

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "crop information()::"
            "sourceCrop left(%d),top(%d),right(%d),bottom(%d),"
            "src_rect x(%d),y(%d),w(%d),h(%d),"
            "prev_handle w(%d),h(%d)",
            cur->sourceCrop.left,
            cur->sourceCrop.top,
            cur->sourceCrop.right,
            cur->sourceCrop.bottom,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            prev_handle->width, prev_handle->height);

    src_rect->x = SEC_MAX(src_rect->x, 0);
    src_rect->y = SEC_MAX(src_rect->y, 0);
    src_rect->w = SEC_MAX(src_rect->w, 0);
    src_rect->w = SEC_MIN(src_rect->w, prev_handle->width);
    src_rect->h = SEC_MAX(src_rect->h, 0);
    src_rect->h = SEC_MIN(src_rect->h, prev_handle->height);

    /* 4. Set dst_rect(fb or lcd)
     *    fimc dst image will be stored from left top corner
     */
    dst_rect->x = 0;
    dst_rect->y = 0;
    dst_rect->w = win->rect_info.w;
    dst_rect->h = win->rect_info.h;

    /* Summery */
    SEC_HWC_Log(HWC_LOG_DEBUG,
            "set_src_dst_img_rect()::"
            "SRC w(%d),h(%d),f_w(%d),f_h(%d),fmt(0x%x),"
            "base(0x%x),offset(%d),paddr(0x%X),mem_id(%d),mem_type(%d)=>\r\n"
            "   DST w(%d),h(%d),f(0x%x),base(0x%x),"
            "offset(%d),mem_id(%d),mem_type(%d),"
            "rot(%d),win_idx(%d)"
            "   SRC_RECT x(%d),y(%d),w(%d),h(%d)=>"
            "DST_RECT x(%d),y(%d),w(%d),h(%d)",
            src_img->w, src_img->h, src_img->f_w, src_img->f_h, src_img->format,
            src_img->base, src_img->offset, src_img->paddr, src_img->mem_id, src_img->mem_type,
            dst_img->w, dst_img->h,  dst_img->format, dst_img->base,
            dst_img->offset, dst_img->mem_id, dst_img->mem_type,
            cur->transform, win_idx,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);

    return 0;
}

static int get_hwc_compos_decision(hwc_layer_1_t* cur, int iter, int win_cnt)
{
    if(cur->flags & HWC_SKIP_LAYER  || !cur->handle) {
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::is_skip_layer  %d  cur->handle %x ",
                __func__, cur->flags & HWC_SKIP_LAYER, cur->handle);

        return HWC_FRAMEBUFFER;
    }

    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    int compositionType = HWC_FRAMEBUFFER;

    if (iter == 0) {
    /* check here....if we have any resolution constraints */
        if (((cur->sourceCrop.right - cur->sourceCrop.left + 1) < 16) ||
            ((cur->sourceCrop.bottom - cur->sourceCrop.top + 1) < 8))
            return compositionType;

        if ((cur->transform == HAL_TRANSFORM_ROT_90) ||
            (cur->transform == HAL_TRANSFORM_ROT_270)) {
            if (((cur->displayFrame.right - cur->displayFrame.left + 1) < 4) ||
                ((cur->displayFrame.bottom - cur->displayFrame.top + 1) < 8))
                return compositionType;
        } else if (((cur->displayFrame.right - cur->displayFrame.left + 1) < 8) ||
                   ((cur->displayFrame.bottom - cur->displayFrame.top + 1) < 4)) {
            return compositionType;
        }

        switch (prev_handle->format) {
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
            compositionType = HWC_OVERLAY;
            break;
        case HAL_PIXEL_FORMAT_YV12:                 /* YCrCb_420_P */
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
            if ((prev_handle->usage & GRALLOC_USAGE_HWC_HWOVERLAY) &&
                 (cur->blending == HWC_BLENDING_NONE))
                compositionType = HWC_OVERLAY;
            else
                compositionType = HWC_FRAMEBUFFER;
            break;
        default:
            compositionType = HWC_FRAMEBUFFER;
            break;
        }
    }

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s::compositionType(%d)=>0:FB,1:OVERLAY \r\n"
            "   format(0x%x),magic(0x%x),flags(%d),size(%d),offset(%d)"
            "b_addr(0x%x),usage(%d),w(%d),h(%d),bpp(%d)",
            "get_hwc_compos_decision()", compositionType,
            prev_handle->format, prev_handle->magic, prev_handle->flags,
            prev_handle->size, prev_handle->offset, prev_handle->base,
            prev_handle->usage, prev_handle->width, prev_handle->height,
            prev_handle->bpp);

    return  compositionType;
}

static void reset_win_rect_info(hwc_win_info_t *win)
{
    win->rect_info.x = 0;
    win->rect_info.y = 0;
    win->rect_info.w = 0;
    win->rect_info.h = 0;
    return;
}


static int assign_overlay_window(struct hwc_context_t *ctx, hwc_layer_1_t *cur,
        int win_idx, int layer_idx)
{
    struct hwc_win_info_t   *win;
    sec_rect   rect;
    int ret = 0;

    if (NUM_OF_WIN <= win_idx)
        return -1;

    win = &ctx->win[win_idx];

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: left(%d),top(%d),right(%d),bottom(%d),transform(%d)"
            "lcd_info.xres(%d),lcd_info.yres(%d)",
            "++assign_overlay_window()",
            cur->displayFrame.left, cur->displayFrame.top,
            cur->displayFrame.right, cur->displayFrame.bottom, cur->transform,
            win->lcd_info.xres, win->lcd_info.yres);

    calculate_rect(win, cur, &rect);

    if ((rect.x != win->rect_info.x) || (rect.y != win->rect_info.y) ||
        (rect.w != win->rect_info.w) || (rect.h != win->rect_info.h)){
        win->rect_info.x = rect.x;
        win->rect_info.y = rect.y;
        win->rect_info.w = rect.w;
        win->rect_info.h = rect.h;
            //turnoff the window and set the window position with new conf...
        if (window_set_pos(win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_set_pos is failed : %s",
                    __func__, strerror(errno));
            ret = -1;
        }
        ctx->layer_prev_buf[win_idx] = 0;
    }

    win->layer_index = layer_idx;
    win->status = HWC_WIN_RESERVED;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: win_x %d win_y %d win_w %d win_h %d  lay_idx %d win_idx %d\n",
            "--assign_overlay_window()",
            win->rect_info.x, win->rect_info.y, win->rect_info.w,
            win->rect_info.h, win->layer_index, win_idx );

    return 0;
}

#ifdef SKIP_DUMMY_UI_LAY_DRAWING
static void get_hwc_ui_lay_skipdraw_decision(struct hwc_context_t* ctx,
                               hwc_display_contents_1_t* list)
{
    private_handle_t *prev_handle;
    hwc_layer_1_t* cur;
    int num_of_fb_lay_skip = 0;
    int fb_lay_tot = ctx->num_of_fb_layer + ctx->num_of_fb_lay_skip;

    if (fb_lay_tot > NUM_OF_DUMMY_WIN)
        return;

    if (fb_lay_tot < 1) {
#ifdef GL_WA_OVLY_ALL
        ctx->ui_skip_frame_cnt++;
        if (ctx->ui_skip_frame_cnt >= THRES_FOR_SWAP) {
            ctx->ui_skip_frame_cnt = 0;
            ctx->num_of_fb_layer_prev = 1;
        }
#endif
        return;
    }

    if (ctx->fb_lay_skip_initialized) {
        for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
            cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
            if (ctx->win_virt[cnt].layer_prev_buf == (uint32_t)cur->handle)
                num_of_fb_lay_skip++;
        }
#ifdef GL_WA_OVLY_ALL
        if (ctx->ui_skip_frame_cnt >= THRES_FOR_SWAP)
            num_of_fb_lay_skip = 0;
#endif
        if (num_of_fb_lay_skip != fb_lay_tot) {
            ctx->num_of_fb_layer = fb_lay_tot;
            ctx->num_of_fb_lay_skip = 0;
#ifdef GL_WA_OVLY_ALL
            ctx->ui_skip_frame_cnt = 0;
#endif
            for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
                cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
                ctx->win_virt[cnt].layer_prev_buf = (uint32_t)cur->handle;
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->win_virt[cnt].status = HWC_WIN_FREE;
            }
        } else {
            ctx->num_of_fb_layer = 0;
            ctx->num_of_fb_lay_skip = fb_lay_tot;
#ifdef GL_WA_OVLY_ALL
            ctx->ui_skip_frame_cnt++;
#endif
            for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
                cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
                cur->compositionType = HWC_OVERLAY;
                ctx->win_virt[cnt].status = HWC_WIN_RESERVED;
            }
        }
    } else {
        ctx->num_of_fb_lay_skip = 0;
        for (int i = 0; i < list->numHwLayers ; i++) {
            if(num_of_fb_lay_skip >= NUM_OF_DUMMY_WIN)
                break;

            cur = &list->hwLayers[i];
            if (cur->handle) {
                prev_handle = (private_handle_t *)(cur->handle);

                switch (prev_handle->format) {
                case HAL_PIXEL_FORMAT_RGBA_8888:
                case HAL_PIXEL_FORMAT_BGRA_8888:
                case HAL_PIXEL_FORMAT_RGBX_8888:
                case HAL_PIXEL_FORMAT_RGB_565:
                    cur->compositionType = HWC_FRAMEBUFFER;
                    ctx->win_virt[num_of_fb_lay_skip].layer_prev_buf =
                        (uint32_t)cur->handle;
                    ctx->win_virt[num_of_fb_lay_skip].layer_index = i;
                    ctx->win_virt[num_of_fb_lay_skip].status = HWC_WIN_FREE;
                    num_of_fb_lay_skip++;
                    break;
                default:
                    break;
                }
            } else {
                cur->compositionType = HWC_FRAMEBUFFER;
            }
        }

        if (num_of_fb_lay_skip == fb_lay_tot)
            ctx->fb_lay_skip_initialized = 1;
    }

    return;

}
#endif

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t numDisplays, hwc_display_contents_1_t** displays)
{

    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int overlay_win_cnt = 0;
    int compositionType = 0;
    int ret;

    // Compat
    hwc_display_contents_1_t* list = NULL;
    if (numDisplays > 0) {
        list = displays[0];
    }

#if defined(BOARD_USES_HDMI)
    android::SecHdmiClient *mHdmiClient = android::SecHdmiClient::getInstance();
    int hdmi_cable_status = (int)mHdmiClient->getHdmiCableStatus();

    ctx->hdmi_cable_status = hdmi_cable_status;
#endif
        
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
    if ((list && (!(list->flags & HWC_GEOMETRY_CHANGED))) &&
	(ctx->num_of_hwc_layer > 0)) {
      get_hwc_ui_lay_skipdraw_decision(ctx, list);
      return 0;
    }
    ctx->fb_lay_skip_initialized = 0;
    ctx->num_of_fb_lay_skip = 0;
#ifdef GL_WA_OVLY_ALL
    ctx->ui_skip_frame_cnt = 0;
#endif
        
    for (int i = 0; i < NUM_OF_DUMMY_WIN; i++) {
        ctx->win_virt[i].layer_prev_buf = 0;
        ctx->win_virt[i].layer_index = -1;
        ctx->win_virt[i].status = HWC_WIN_FREE;
    }
#endif
        
    //if geometry is not changed, there is no need to do any work here
    if (!list || (!(list->flags & HWC_GEOMETRY_CHANGED)))
        return 0;

    //all the windows are free here....
    for (int i = 0 ; i < NUM_OF_WIN; i++) {
        ctx->win[i].status = HWC_WIN_FREE;
        ctx->win[i].buf_index = 0;
    }

    ctx->num_of_hwc_layer = 0;
    ctx->num_of_fb_layer = 0;
    ctx->num_2d_blit_layer = 0;
    ctx->num_of_ext_disp_video_layer = 0;

    for (int i = 0; i < list->numHwLayers; i++) {
        hwc_layer_1_t *cur = &list->hwLayers[i];
        private_handle_t *prev_handle = NULL;
        if (cur->handle) {
            prev_handle = (private_handle_t *)(cur->handle);
            SEC_HWC_Log(HWC_LOG_DEBUG, "prev_handle->usage = %d", prev_handle->usage);
            if (prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP) {
                ctx->num_of_ext_disp_layer++;
                if ((prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP) ||
                    check_yuv_format((unsigned int)prev_handle->format) == 1) {
                    ctx->num_of_ext_disp_video_layer++;
                }
            }
        }
    }

    for (int i = 0; i < list->numHwLayers ; i++) {
        hwc_layer_1_t* cur = &list->hwLayers[i];
        private_handle_t *prev_handle = (private_handle_t *)(cur->handle);

        if (overlay_win_cnt < NUM_OF_WIN) {
            compositionType = get_hwc_compos_decision(cur, 0, overlay_win_cnt);

            if (compositionType == HWC_FRAMEBUFFER) {
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->num_of_fb_layer++;
            } else {
                ret = assign_overlay_window(ctx, cur, overlay_win_cnt, i);
                if (ret != 0) {
                    SEC_HWC_Log(HWC_LOG_ERROR, "assign_overlay_window fail, change to frambuffer");
                    cur->compositionType = HWC_FRAMEBUFFER;
                    ctx->num_of_fb_layer++;
                    continue;
                }

                cur->compositionType = HWC_OVERLAY;
                cur->hints = HWC_HINT_CLEAR_FB;
                overlay_win_cnt++;
                ctx->num_of_hwc_layer++;
            }
        } else {
            cur->compositionType = HWC_FRAMEBUFFER;
            ctx->num_of_fb_layer++;
        }
#if defined(BOARD_USES_HDMI)
        SEC_HWC_Log(HWC_LOG_DEBUG, "ext disp vid = %d, cable status = %d, composition type = %d",
                ctx->num_of_ext_disp_video_layer, ctx->hdmi_cable_status, compositionType);
        if (ctx->num_of_ext_disp_video_layer >= 2) {
            if ((ctx->hdmi_cable_status) &&
                    (compositionType == HWC_OVERLAY) &&
                    (prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP)) {
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->num_of_hwc_layer--;
                overlay_win_cnt--;
                ctx->num_of_fb_layer++;
                cur->hints = 0;
            }
        }
#endif
    }

#if defined(BOARD_USES_HDMI)
    mHdmiClient = android::SecHdmiClient::getInstance();
    mHdmiClient->setHdmiHwcLayer(ctx->num_of_hwc_layer);
    if (ctx->num_of_ext_disp_video_layer > 1) {
        mHdmiClient->setExtDispLayerNum(0);
    }
#endif

    if (list->numHwLayers != (ctx->num_of_fb_layer + ctx->num_of_hwc_layer))
        SEC_HWC_Log(HWC_LOG_DEBUG,
                "%s:: numHwLayers %d num_of_fb_layer %d num_of_hwc_layer %d ",
                __func__, list->numHwLayers, ctx->num_of_fb_layer,
                ctx->num_of_hwc_layer);

    if (overlay_win_cnt < NUM_OF_WIN) {
        //turn off the free windows
        for (int i = overlay_win_cnt; i < NUM_OF_WIN; i++) {
            window_hide(&ctx->win[i]);
            reset_win_rect_info(&ctx->win[i]);
        }
    }

    return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev,
                   size_t numDisplays,
                   hwc_display_contents_1_t** displays)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    int skipped_window_mask = 0;
    hwc_layer_1_t* cur;
    struct hwc_win_info_t   *win;
    int ret;
    int pmem_phyaddr;
    struct sec_img src_img;
    struct sec_img dst_img;
    struct sec_rect src_work_rect;
    struct sec_rect dst_work_rect;
    bool need_swap_buffers = ctx->num_of_fb_layer > 0;

    memset(&src_img, 0, sizeof(src_img));
    memset(&dst_img, 0, sizeof(dst_img));
    memset(&src_work_rect, 0, sizeof(src_work_rect));
    memset(&dst_work_rect, 0, sizeof(dst_work_rect));

#if defined(BOARD_USES_HDMI)
    int skip_hdmi_rendering = 0;
    int rotVal = 0;
#endif

    // Only support one display
    hwc_display_contents_1_t* list = displays[0];

    if (!list) {
        //turn off the all windows
        for (int i = 0; i < NUM_OF_WIN; i++) {
            window_hide(&ctx->win[i]);
            reset_win_rect_info(&ctx->win[i]);
            ctx->win[i].status = HWC_WIN_FREE;
        }
        ctx->num_of_hwc_layer = 0;
        need_swap_buffers = true;

        if (list->sur == NULL && list->dpy == NULL) {
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
            ctx->fb_lay_skip_initialized = 0;
#endif
            return HWC_EGL_ERROR;
        }
    }

    if(ctx->num_of_hwc_layer > NUM_OF_WIN)
        ctx->num_of_hwc_layer = NUM_OF_WIN;

    /*
     * H/W composer documentation states:
     * There is an implicit layer containing opaque black
     * pixels behind all the layers in the list.
     * It is the responsibility of the hwcomposer module to make
     * sure black pixels are output (or blended from).
     *
     * Since we're using a blitter, we need to erase the frame-buffer when
     * switching to all-overlay mode.
     *
     */
    if (ctx->num_of_hwc_layer &&
        ctx->num_of_fb_layer==0 && ctx->num_of_fb_layer_prev) {
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
        if (ctx->num_of_fb_lay_skip == 0)
#endif
        {
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            need_swap_buffers = true;
        }
    }
    ctx->num_of_fb_layer_prev = ctx->num_of_fb_layer;

    //compose hardware layers here
    for (int i = 0; i < ctx->num_of_hwc_layer - ctx->num_2d_blit_layer; i++) {
        win = &ctx->win[i];
        if (win->status == HWC_WIN_RESERVED) {
            cur = &list->hwLayers[win->layer_index];

            if (cur->compositionType == HWC_OVERLAY) {
                if (ctx->layer_prev_buf[i] == (uint32_t)cur->handle) {
                    /*
                     * In android platform, all the graphic buffer are at least
                     * double buffered (2 or more) this buffer is already rendered.
                     * It is the redundant src buffer for FIMC rendering.
                     */

#if defined(BOARD_USES_HDMI)
                    skip_hdmi_rendering = 1;
#endif
                    continue;
                }
                ctx->layer_prev_buf[i] = (uint32_t)cur->handle;
                // initialize the src & dist context for fimc
                set_src_dst_img_rect(cur, win, &src_img, &dst_img,
                                &src_work_rect, &dst_work_rect, i);

                ret = runFimc(ctx,
                            &src_img, &src_work_rect,
                            &dst_img, &dst_work_rect,
                            cur->transform);

                if (ret < 0) {
                    SEC_HWC_Log(HWC_LOG_ERROR, "%s::runFimc fail : ret=%d\n",
                                __func__, ret);
                    skipped_window_mask |= (1 << i);
                    continue;
                }

                window_pan_display(win);

                win->buf_index = (win->buf_index + 1) % NUM_OF_WIN_BUF;
                if (win->power_state == 0)
                    window_show(win);
            } else {
                SEC_HWC_Log(HWC_LOG_ERROR,
                        "%s:: error : layer %d compositionType should have been"
                        " HWC_OVERLAY ", __func__, win->layer_index);
                skipped_window_mask |= (1 << i);
                continue;
            }
        } else {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : window status should have "
                    "been HWC_WIN_RESERVED by now... ", __func__);
             skipped_window_mask |= (1 << i);
             continue;
        }
    }

    if (skipped_window_mask) {
        //turn off the free windows
        for (int i = 0; i < NUM_OF_WIN; i++) {
            if (skipped_window_mask & (1 << i)) {
                window_hide(&ctx->win[i]);
                reset_win_rect_info(&ctx->win[i]);
            }
        }
    }

    if (need_swap_buffers) {
#ifdef CHECK_EGL_FPS
        check_fps();
#endif
#ifdef HWC_HWOVERLAY
        unsigned char pixels[4];
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#endif
        EGLBoolean sucess = eglSwapBuffers((EGLDisplay)list->dpy, (EGLSurface)list->sur);
        if (!sucess)
            return HWC_EGL_ERROR;
    }

#if defined(BOARD_USES_HDMI)
    android::SecHdmiClient *mHdmiClient = android::SecHdmiClient::getInstance();

    if (skip_hdmi_rendering == 1)
        return 0;

    if (list == NULL) {
        // Don't display unnecessary image
        mHdmiClient->setHdmiEnable(0);
        return 0;
    } else {
        mHdmiClient->setHdmiEnable(1);
    }

#ifdef SUPPORT_AUTO_UI_ROTATE
    cur = &list->hwLayers[0];

    if (cur->transform == HAL_TRANSFORM_ROT_90 || cur->transform == HAL_TRANSFORM_ROT_270)
        mHdmiClient->setHdmiRotate(270, ctx->num_of_hwc_layer);
    else
        mHdmiClient->setHdmiRotate(0, ctx->num_of_hwc_layer);
#endif

    // To support S3D video playback (automatic TV mode change to 3D mode)
    if (ctx->num_of_hwc_layer == 1) {
        if (src_img.usage != prev_usage)
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE, android::SecHdmiClient::HDMI_2D);    // V4L2_STD_1080P_60

        if ((src_img.usage & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
            (src_img.usage & GRALLOC_USAGE_PRIVATE_SBS_RL))
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_S3D_SBS_RESOLUTION_VALUE, android::SecHdmiClient::HDMI_S3D_SBS);    // V4L2_STD_TVOUT_720P_60_SBS_HALF
        else if ((src_img.usage & GRALLOC_USAGE_PRIVATE_TB_LR) ||
            (src_img.usage & GRALLOC_USAGE_PRIVATE_TB_RL))
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_S3D_TB_RESOLUTION_VALUE, android::SecHdmiClient::HDMI_S3D_TB);    // V4L2_STD_TVOUT_1080P_24_TB

        prev_usage = src_img.usage;
    } else {
        if ((prev_usage & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_SBS_RL) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_TB_LR) ||
            (prev_usage & GRALLOC_USAGE_PRIVATE_TB_RL))
            mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE, android::SecHdmiClient::HDMI_2D);    // V4L2_STD_1080P_60

        prev_usage = 0;
    }

    if (ctx->num_of_hwc_layer == 1) {
        if ((src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED)||
                (src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP)) {
            ADDRS * addr = (ADDRS *)(src_img.base);
            mHdmiClient->blit2Hdmi(src_work_rect.w, src_work_rect.h,
                                    src_img.format,
                                    (unsigned int)addr->addr_y, (unsigned int)addr->addr_cbcr, (unsigned int)addr->addr_cbcr,
                                    0, 0,
                                    android::SecHdmiClient::HDMI_MODE_VIDEO,
                                    ctx->num_of_hwc_layer);
        } else if ((src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_P) ||
                    (src_img.format == HAL_PIXEL_FORMAT_YV12)) {
            mHdmiClient->blit2Hdmi(src_work_rect.w, src_work_rect.h,
                                    src_img.format,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_rgb_y,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_cb,
                                    (unsigned int)ctx->fimc.params.src.buf_addr_phy_cr,
                                    0, 0,
                                    android::SecHdmiClient::HDMI_MODE_VIDEO,
                                    ctx->num_of_hwc_layer);
        } else {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s: Unsupported format = %d", __func__, src_img.format);
        }
    }
#endif

    return 0;
}

static void hwc_registerProcs(struct hwc_composer_device_1* dev,
        hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ctx->procs = const_cast<hwc_procs_t *>(procs);
}

static int hwc_query(struct hwc_composer_device_1* dev,
        int what, int* value)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we don't support the background layer yet
        value[0] = 0;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = 1000000000.0 / 57;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int hwc_eventControl(struct hwc_composer_device_1* dev, int dpy,
        int event, int enabled)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        int val = !!enabled;
        int err = ioctl(ctx->global_lcd_win.fd, S3CFB_SET_VSYNC_INT, &val);
        if (err < 0)
            return -errno;
        
        return 0;
    }
    return -EINVAL;
}

#ifdef SYSFS_VSYNC_NOTIFICATION
static void *hwc_vsync_sysfs_loop(void *data)
{
    static char buf[4096];
    int vsync_timestamp_fd;
    fd_set exceptfds;
    int res;
    int64_t timestamp = 0;
    hwc_context_t * ctx = (hwc_context_t *)(data);

    vsync_timestamp_fd = open("/sys/devices/platform/samsung-pd.2/s3cfb.0/vsync_time", O_RDONLY);
    char thread_name[64] = "hwcVsyncThread";
    prctl(PR_SET_NAME, (unsigned long) &thread_name, 0, 0, 0);
    setpriority(PRIO_PROCESS, 0, -20);
    memset(buf, 0, sizeof(buf));
    
    SEC_HWC_Log(HWC_LOG_DEBUG,"Using sysfs mechanism for VSYNC notification");

    FD_ZERO(&exceptfds);
    FD_SET(vsync_timestamp_fd, &exceptfds);
    do {
        ssize_t len = read(vsync_timestamp_fd, buf, sizeof(buf));
        timestamp = strtoull(buf, NULL, 0);
        if(ctx->procs)
            ctx->procs->vsync(ctx->procs, 0, timestamp);
        select(vsync_timestamp_fd + 1, NULL, NULL, &exceptfds, NULL);
        lseek(vsync_timestamp_fd, 0, SEEK_SET);
    } while (1);

    return NULL;
}
#endif

void handle_vsync_uevent(hwc_context_t *ctx, const char *buff, int len)
{
    uint64_t timestamp = 0;
    const char *s = buff;

    if(!ctx->procs || !ctx->procs->vsync)
       return;

    s += strlen(s) + 1;

    while(*s) {
        if (!strncmp(s, "VSYNC=", strlen("VSYNC=")))
            timestamp = strtoull(s + strlen("VSYNC="), NULL, 0);

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    ctx->procs->vsync(ctx->procs, 0, timestamp);
}

static void *hwc_vsync_thread(void *data)
{
    hwc_context_t *ctx = (hwc_context_t *)(data);
    char uevent_desc[4096];

    memset(uevent_desc, 0, sizeof(uevent_desc));
    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);
    uevent_init();

    while(true) {

        int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);

        bool vsync = !strcmp(uevent_desc, "change@/devices/platform/samsung-pd.2/s3cfb.0");
        if(vsync)
            handle_vsync_uevent(ctx, uevent_desc, len);
    }

    return NULL;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int ret = 0;
    int i;
    if (ctx) {
        if (destroyFimc(&ctx->fimc) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyFimc fail", __func__);
            ret = -1;
        }

        if (window_close(&ctx->global_lcd_win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_close() fail", __func__);
            ret = -1;
        }

        for (i = 0; i < NUM_OF_WIN; i++) {
            if (window_close(&ctx->win[i]) < 0)
                SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
        }

        free(ctx);
    }
    return ret;
}

static int hwc_blank(struct hwc_composer_device_1 *dev, int dpy, int blank)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (blank) {
        // release our resources, the screen is turning off
        // in our case, there is nothing to do.
        ctx->num_of_fb_layer_prev = 0;
        return 0;
    }
    else {
        // No need to unblank, will unblank on set()
        return 0;
    }
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;
    int err    = 0;
    struct hwc_win_info_t   *win;

    if (strcmp(name, HWC_HARDWARE_COMPOSER))
        return  -EINVAL;

    struct hwc_context_t *dev;
    dev = (hwc_context_t*)malloc(sizeof(*dev));

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->device.common.tag           = HARDWARE_DEVICE_TAG;
    dev->device.common.version       = HWC_DEVICE_API_VERSION_1_0;
    dev->device.common.module        = const_cast<hw_module_t*>(module);
    dev->device.common.close         = hwc_device_close;
    dev->device.prepare              = hwc_prepare;
    dev->device.set                  = hwc_set;
    dev->device.eventControl         = hwc_eventControl;
    dev->device.blank                = hwc_blank;
    dev->device.query                = hwc_query;
    dev->device.registerProcs        = hwc_registerProcs;
    *device = &dev->device.common;

    //initializing
    memset(&(dev->fimc),    0, sizeof(s5p_fimc_t));

    /* open WIN0 & WIN1 here */
    for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_open(&(dev->win[i]), i)  < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "%s:: Failed to open window %d device ", __func__, i);
            status = -EINVAL;
            goto err;
        }
    }

    /* open window 2, used to query global LCD info */
    if (window_open(&dev->global_lcd_win, 2) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s:: Failed to open window 2 device ", __func__);
        status = -EINVAL;
        goto err;
    }

    if (window_get_global_lcd_info(dev) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::window_get_global_lcd_info is failed : %s",
                __func__, strerror(errno));
        status = -EINVAL;
        goto err;
    }

#if defined(BOARD_USES_HDMI)
    lcd_width   = dev->lcd_info.xres;
    lcd_height  = dev->lcd_info.yres;
#endif

    /* initialize the window context */
    for (int i = 0; i < NUM_OF_WIN; i++) {
        win = &dev->win[i];
        memcpy(&win->lcd_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));
        memcpy(&win->var_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));

        win->rect_info.x = 0;
        win->rect_info.y = 0;
        win->rect_info.w = win->var_info.xres;
        win->rect_info.h = win->var_info.yres;

       if (window_set_pos(win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_set_pos is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }

        if (window_get_info(win, i) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_get_info is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }

    }

    //create PP
    if (createFimc(&dev->fimc) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::creatFimc() fail", __func__);
        status = -EINVAL;
        goto err;
    }

#ifndef SYSFS_VSYNC_NOTIFICATION
    err = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (err) {
        ALOGE("%s::pthread_create() failed : %s", __func__, strerror(err));
        status = -err;
        goto err;
    }
#endif

#ifdef SYSFS_VSYNC_NOTIFICATION
    err = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_sysfs_loop, dev);
    if (err) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::pthread_create() failed : %s", __func__, strerror(err));
        status = -err;
        goto err;
    }
#endif

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: hwc_device_open: SUCCESS", __func__);

    return 0;

err:
    if (destroyFimc(&dev->fimc) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::destroyFimc() fail", __func__);

    if (window_close(&dev->global_lcd_win) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_close() fail", __func__);

    for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_close(&dev->win[i]) < 0)
            SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
    }

    return status;
}
