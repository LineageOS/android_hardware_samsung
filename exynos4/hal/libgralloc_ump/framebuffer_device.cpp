/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Portions of this code have been modified from the original.
 * These modifications are:
 *    * includes
 *    * enums
 *    * fb_set_swap_interval()
 *    * fb_post()
 *    * init_frame_buffer_locked()
 *    * init_frame_buffer()
 *    * fb_close()
 *    * framebuffer_device_open()
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <GLES/gl.h>

#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
#include "gralloc_vsync_report.h"
#endif

#include "gralloc_priv.h"
#include "gralloc_helper.h"

#include "linux/fb.h"

/* numbers of buffers for page flipping */
#define NUM_BUFFERS 2

enum {
    PAGE_FLIP = 0x00000001,
};

static int fb_set_swap_interval(struct framebuffer_device_t* dev, int interval)
{
    if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
        return -EINVAL;

    /* Currently not implemented */
    return 0;
}

static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    if (private_handle_t::validate(buffer) < 0)
        return -EINVAL;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    if (m->currentBuffer) {
        m->base.unlock(&m->base, m->currentBuffer);
        m->currentBuffer = 0;
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        m->base.lock(&m->base, buffer, private_module_t::PRIV_USAGE_LOCKED_FOR_POST,
                0, 0, m->info.xres, m->info.yres, NULL);

        const size_t offset = hnd->base - m->framebuffer->base;
        int interrupt;
        m->info.activate = FB_ACTIVATE_VBL;
        m->info.yoffset = offset / m->finfo.line_length;

#ifdef STANDARD_LINUX_SCREEN
#define FBIO_WAITFORVSYNC       _IOW('F', 0x20, __u32)
#define S3CFB_SET_VSYNC_INT     _IOW('F', 206, unsigned int)
        if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1) {
            ALOGE("FBIOPAN_DISPLAY failed");
            m->base.unlock(&m->base, buffer);
            return 0;
        }

        if (m->enableVSync) {
            /* enable VSYNC */
            interrupt = 1;
            if (ioctl(m->framebuffer->fd, S3CFB_SET_VSYNC_INT, &interrupt) < 0) {
                ALOGE("S3CFB_SET_VSYNC_INT enable failed");
                return 0;
            }
            /* wait for VSYNC */

#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
            gralloc_mali_vsync_report(MALI_VSYNC_EVENT_BEGIN_WAIT);
#endif
            int crtc;
            crtc = 0;
            if (ioctl(m->framebuffer->fd, FBIO_WAITFORVSYNC, &crtc) < 0) {
                ALOGE("FBIO_WAITFORVSYNC failed");
#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
                gralloc_mali_vsync_report(MALI_VSYNC_EVENT_END_WAIT);
#endif
                return 0;
            }
#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
            gralloc_mali_vsync_report(MALI_VSYNC_EVENT_END_WAIT);
#endif
            // disable VSYNC
            interrupt = 0;
            if (ioctl(m->framebuffer->fd, S3CFB_SET_VSYNC_INT, &interrupt) < 0) {
                ALOGE("S3CFB_SET_VSYNC_INT disable failed");
                return 0;
            }
        }
#else
        /*Standard Android way*/
#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
        gralloc_mali_vsync_report(MALI_VSYNC_EVENT_BEGIN_WAIT);
#endif
        if (ioctl(m->framebuffer->fd, FBIOPUT_VSCREENINFO, &m->info) == -1) {
            ALOGE("FBIOPUT_VSCREENINFO failed");
#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
            gralloc_mali_vsync_report(MALI_VSYNC_EVENT_END_WAIT);
#endif
            m->base.unlock(&m->base, buffer);
            return -errno;
        }
#ifdef MALI_VSYNC_EVENT_REPORT_ENABLE
        gralloc_mali_vsync_report(MALI_VSYNC_EVENT_END_WAIT);
#endif
#endif

        m->currentBuffer = buffer;
    } else {
        /*
         * If we can't do the page_flip, just copy the buffer to the front
         * FIXME: use copybit HAL instead of memcpy
         */
        void* fb_vaddr;
        void* buffer_vaddr;

        m->base.lock(&m->base, m->framebuffer,  GRALLOC_USAGE_SW_WRITE_RARELY,
                     0, 0, m->info.xres, m->info.yres, &fb_vaddr);

        m->base.lock(&m->base, buffer,  GRALLOC_USAGE_SW_READ_RARELY,
                     0, 0, m->info.xres, m->info.yres, &buffer_vaddr);

        memcpy(fb_vaddr, buffer_vaddr, m->finfo.line_length * m->info.yres);

        m->base.unlock(&m->base, buffer);
        m->base.unlock(&m->base, m->framebuffer);
    }
    return 0;
}

int init_frame_buffer_locked(struct private_module_t* module)
{
    /* Nothing to do, already initialized */
    if (module->framebuffer)
        return 0;

    char const * const device_template[] = {
        "/dev/graphics/fb%u",
        "/dev/fb%u",
        NULL
    };

    int fd = -1;
    int i = 0;
    char name[64];

    while ((fd == -1) && device_template[i]) {
        snprintf(name, 64, device_template[i], 0);
        fd = open(name, O_RDWR, 0);
        i++;
    }

    if (fd < 0)
        return -errno;

    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

    info.reserved[0] = 0;
    info.reserved[1] = 0;
    info.reserved[2] = 0;
    info.xoffset = 0;
    info.yoffset = 0;
    info.activate = FB_ACTIVATE_NOW;

#ifdef GRALLOC_16_BITS
    /*
     * Explicitly request 5/6/5
     */
    info.bits_per_pixel = 16;
    info.red.offset     = 11;
    info.red.length     = 5;
    info.green.offset   = 5;
    info.green.length   = 6;
    info.blue.offset    = 0;
    info.blue.length    = 5;
    info.transp.offset  = 0;
    info.transp.length  = 0;
#else
    /*
     * Explicitly request 8/8/8
     */
    info.bits_per_pixel = 32;
    info.red.offset     = 16;
    info.red.length     = 8;
    info.green.offset   = 8;
    info.green.length   = 8;
    info.blue.offset    = 0;
    info.blue.length    = 8;
    info.transp.offset  = 0;
    info.transp.length  = 0;
#endif

    /*
     * Request NUM_BUFFERS screens (at lest 2 for page flipping)
     */
    info.yres_virtual = info.yres * NUM_BUFFERS;

    uint32_t flags = PAGE_FLIP;
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1) {
        info.yres_virtual = info.yres;
        flags &= ~PAGE_FLIP;
        ALOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
    }

    if (info.yres_virtual < info.yres * 2) {
        // we need at least 2 for page-flipping
        info.yres_virtual = info.yres;
        flags &= ~PAGE_FLIP;
        ALOGW("page flipping not supported (yres_virtual=%d, requested=%d)",
                info.yres_virtual, info.yres * 2);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

    int refreshRate = 1000000000000000LLU /
    (
        uint64_t( info.upper_margin + info.lower_margin + info.vsync_len + info.yres )
        * ( info.left_margin  + info.right_margin + info.hsync_len + info.xres )
        * info.pixclock
    );

    if (refreshRate == 0)
        refreshRate = 60 * 1000;  /* 60 Hz */

    if (int(info.width) <= 0 || int(info.height) <= 0) {
        /* the driver doesn't return that information. default to 160 dpi */
        info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
        info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
    }

    float xdpi = (info.xres * 25.4f) / info.width;
    float ydpi = (info.yres * 25.4f) / info.height;
    float fps  = refreshRate / 1000.0f;

    ALOGI("using (fd=%d)\n"
         "id           = %s\n"
         "xres         = %d px\n"
         "yres         = %d px\n"
         "xres_virtual = %d px\n"
         "yres_virtual = %d px\n"
         "bpp          = %d\n"
         "r            = %2u:%u\n"
         "g            = %2u:%u\n"
         "b            = %2u:%u\n",
         fd,
         finfo.id,
         info.xres,
         info.yres,
         info.xres_virtual,
         info.yres_virtual,
         info.bits_per_pixel,
         info.red.offset, info.red.length,
         info.green.offset, info.green.length,
         info.blue.offset, info.blue.length);

    ALOGI("width        = %d mm (%f dpi)\n"
         "height       = %d mm (%f dpi)\n"
         "refresh rate = %.2f Hz\n",
         info.width,  xdpi,
         info.height, ydpi,
         fps);

    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    if (finfo.smem_len <= 0)
        return -errno;

    module->flags = flags;
    module->info = info;
    module->finfo = finfo;
    module->xdpi = xdpi;
    module->ydpi = ydpi;
    module->fps = fps;

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.gralloc.vsync", value, "1");
    module->enableVSync = atoi(value);
    /*
     * map the framebuffer
     */
    size_t fbSize = round_up_to_page_size(finfo.line_length * info.yres_virtual);
    void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (vaddr == MAP_FAILED) {
        ALOGE("Error mapping the framebuffer (%s)", strerror(errno));
        return -errno;
    }

    memset(vaddr, 0, fbSize);

    /*
     * Create a "fake" buffer object for the entire frame buffer memory,
     * and store it in the module
     */
    module->framebuffer = new private_handle_t(private_handle_t::PRIV_FLAGS_FRAMEBUFFER,
            fbSize, intptr_t(vaddr), 0, dup(fd), 0);

    module->numBuffers = info.yres_virtual / info.yres;
    module->bufferMask = 0;

    return 0;
}

int enableScreen(struct framebuffer_device_t* dev, int enable)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    if (enable == 1) {
        if (ioctl(m->framebuffer->fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
            ALOGE("%s: FBIOBLANK failed : (%d:%s)",
                    __func__, m->framebuffer->fd, strerror(errno));
            return -EINVAL;
        }
    } else if (enable == 0) {
        if (ioctl(m->framebuffer->fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0) {
            ALOGE("%s: FBIOBLANK failed : (%d:%s)",
                    __func__, m->framebuffer->fd, strerror(errno));
            return -EINVAL;
        }
    } else {
        return -EINVAL;
    }
    return 0;
}
static int init_frame_buffer(struct private_module_t* module)
{
    pthread_mutex_lock(&module->lock);
    int err = init_frame_buffer_locked(module);
    pthread_mutex_unlock(&module->lock);
    return err;
}

static int fb_close(struct hw_device_t *device)
{
    framebuffer_device_t* dev = reinterpret_cast<framebuffer_device_t*>(device);
    if (dev) {
        ump_close();
        delete dev;
    }
    return 0;
}

int compositionComplete(struct framebuffer_device_t* dev)
{
#ifndef HWC_HWOVERLAY
    unsigned char pixels[4];
    /* By doing a readpixel here we force the GL driver to start rendering
       all the drawcalls up to this point, and to wait for the rendering to be complete.
       Readpixel() also reads a dummy pixel, but this is not used. We only use this
       function here to flush the render pipeline. */
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    /* The rendering of the backbuffer is now completed.
       When SurfaceFlinger later does a call to eglSwapBuffer(), the swap will be done
       synchronously in the same thread, and not asynchronoulsy in a background thread later.
       The SurfaceFlinger requires this behaviour since it releases the lock on all the
       SourceBuffers (Layers) after the compositionComplete() function returns.
       However this "bad" behaviour by SurfaceFlinger should not affect performance,
       since the Applications that render the SourceBuffers (Layers) still get the
       full renderpipeline using asynchronouls rendering. So they perform at maximum speed,
       and because of their complexity compared to the Surface flinger jobs, the Surface flinger
       is normally faster even if it does everyhing synchronous and serial.
       */
#endif
    return 0;
}

int framebuffer_device_open(hw_module_t const* module, const char* name, hw_device_t** device)
{
    int status = -EINVAL;

    alloc_device_t* gralloc_device;
    status = gralloc_open(module, &gralloc_device);
    if (status < 0) {
        ALOGE("Fail to Open gralloc device");
        return status;
    }

    /* initialize our state here */
    framebuffer_device_t *dev = (framebuffer_device_t *)malloc(sizeof(framebuffer_device_t));
    if (dev == NULL) {
        ALOGE("Failed to allocate memory for dev");
        gralloc_close(gralloc_device);
        return status;
    }    

    private_module_t* m = (private_module_t*)module;
    status = init_frame_buffer(m);
    if (status < 0) {
        ALOGE("Fail to init framebuffer");
        free(dev);
        gralloc_close(gralloc_device);
        return status;
    }

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = const_cast<hw_module_t*>(module);
    dev->common.close = fb_close;
    dev->setSwapInterval = fb_set_swap_interval;
    dev->post = fb_post;
    dev->setUpdateRect = 0;
    dev->compositionComplete = &compositionComplete;
    dev->enableScreen = &enableScreen;

    int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
    const_cast<uint32_t&>(dev->flags) = 0;
    const_cast<uint32_t&>(dev->width) = m->info.xres;
    const_cast<uint32_t&>(dev->height) = m->info.yres;
    const_cast<int&>(dev->stride) = stride;
#ifdef GRALLOC_16_BITS
    const_cast<int&>(dev->format) = HAL_PIXEL_FORMAT_RGB_565;
#else
    const_cast<int&>(dev->format) = HAL_PIXEL_FORMAT_BGRA_8888;
#endif
    const_cast<float&>(dev->xdpi) = m->xdpi;
    const_cast<float&>(dev->ydpi) = m->ydpi;
    const_cast<float&>(dev->fps) = m->fps;
    const_cast<int&>(dev->minSwapInterval) = 1;
    const_cast<int&>(dev->maxSwapInterval) = 1;
    *device = &dev->common;
    status = 0;

    return status;
}
