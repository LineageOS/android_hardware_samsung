/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Portions of this code have been modified from the original.
 * These modifications are:
 *    * includes
 *    * gralloc_alloc_buffer()
 *    * gralloc_alloc_framebuffer_locked()
 *    * gralloc_alloc_framebuffer()
 *    * alloc_device_alloc()
 *    * alloc_device_free()
 *    * alloc_device_close()
 *    * alloc_device_open()
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

//#define LOG_NDEBUG 0
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include "sec_format.h"
#include "graphics.h"

#include "gralloc_priv.h"
#include "gralloc_helper.h"
#include "framebuffer_device.h"

#include "ump.h"
#include "ump_ref_drv.h"
#include "secion.h"

/*****************************************************************************/
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "format.h"

#include "videodev2.h"
#include "s5p_fimc.h"

#ifdef SAMSUNG_EXYNOS4x12
#define PFX_NODE_FIMC0   "/dev/video0"
#define PFX_NODE_FIMC1   "/dev/video3"
#endif
#ifdef SAMSUNG_EXYNOS4210
#define PFX_NODE_FIMC1   "/dev/video1"
#endif

#ifndef OMX_COLOR_FormatYUV420Planar
#define OMX_COLOR_FormatYUV420Planar 0x13
#endif

#ifndef OMX_COLOR_FormatYUV420SemiPlanar
#define OMX_COLOR_FormatYUV420SemiPlanar 0x15
#endif

bool ion_dev_open = true;
static pthread_mutex_t l_surface= PTHREAD_MUTEX_INITIALIZER;
static int buffer_offset = 0;
static unsigned int gReservedMemAddr = 0;
static int gReservedMemSize = 0;
static int gFimc1Fd = 0;

#ifdef USE_PARTIAL_FLUSH
extern struct private_handle_rect *rect_list;
extern private_handle_rect *find_rect(int secure_id);
extern private_handle_rect *find_last_rect(int secure_id);
extern int release_rect(int secure_id);
#endif

extern int get_bpp(int format);

#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

static uint64_t next_backing_store_id()
{
    static std::atomic<uint64_t> next_id(1);
    return next_id++;
}

static int gralloc_alloc_buffer(alloc_device_t* dev, size_t size, int usage,
                                buffer_handle_t* pHandle, int w, int h,
                                int format, int bpp, int stride_raw, int stride)
{
    ALOGV("%s: size:%d usage:%d w:%d h:%d format:%d bpp:%d stride_raw:%d stride:%d",
        __func__, size, usage, w,h, format, bpp, stride_raw, stride);

    ump_handle ump_mem_handle;
    void *cpu_ptr;
    ump_secure_id ump_id;
    ion_buffer ion_fd = 0;
    ion_phys_addr_t ion_paddr = 0;
    unsigned int ion_flags = 0;
    int priv_alloc_flag = private_handle_t::PRIV_FLAGS_USES_UMP;
    int orig_size=size;
    private_module_t* m;

    ALOGD_IF(debug_level > 0, "%s size=%d usage=0x%x format=0x%x\n", __func__, size, usage, format);

    size = round_up_to_page_size(size);

    if (usage & GRALLOC_USAGE_HW_ION) {
        ALOGD_IF(debug_level > 0, "%s usage = GRALLOC_USAGE_HW_ION", __func__);

        char node[20];
        int ret;
        int offset=0;
        struct v4l2_control vc;
        struct v4l2_format fmt;
        int rc;
        int fd;
        int lock_state=0;
        private_handle_t* hnd;
        unsigned int current_address = 0;

        if (gReservedMemAddr == 0) {
            sprintf(node, "%s", PFX_NODE_FIMC0);

            fd = open(node, O_RDWR);
            if (fd < 0) {
                ALOGE("%s:: %s Post processor open error\n", __func__, node);
                return false;
            }

            fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

            rc = ioctl(fd, VIDIOC_G_FMT, &fmt);
            if (rc < 0) {
                ALOGE("%s:: %s VIDIOC_G_FMT error\n", __func__, node);
                return false;
            }

            vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
            vc.value = 0;
            ret = ioctl(fd, VIDIOC_G_CTRL, &vc);
            if (ret < 0) {
                ALOGE("%s Error in video VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BASE_ADDR (%d)\n", __func__, ret);
                return false;
            }
            gReservedMemAddr = (unsigned int)vc.value;

            vc.id = V4L2_CID_RESERVED_MEM_SIZE;
            vc.value = 0;
            ret = ioctl(fd, VIDIOC_G_CTRL, &vc);
            if (ret < 0) {
                ALOGE("%s Error in video VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_SIZE (%d)\n", __func__, ret);
                return false;
            }
            gReservedMemSize = vc.value;

            if (gReservedMemSize == 0) {
                ALOGE("%s Error in video VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_SIZE is 0\n", __func__);
                return false;
            }

            if (gReservedMemSize >= 4096) {
                gReservedMemAddr += ((gReservedMemSize - 4096) * 1024);
                gReservedMemSize = 4096;
            }

            close(fd);

            if (gFimc1Fd == 0) {
                sprintf(node, "%s", PFX_NODE_FIMC1);

                gFimc1Fd = open(node, O_RDWR);

                if (gFimc1Fd < 0) {
                    ALOGE("%s:: %s Post processor open error\n", __func__, node);
                    return false;
                }
            } // fimc3 setup
        } // fimc1 setup

        if ((unsigned int) (buffer_offset + size) >= (unsigned int) (gReservedMemSize * 1024))
            buffer_offset = 0;

        current_address = gReservedMemAddr + buffer_offset;

        if ( (usage < 0 || usage & (GRALLOC_USAGE_HWC_HWOVERLAY | GRALLOC_USAGE_HW_ION)) &&
             (format == (int)HAL_PIXEL_FORMAT_RGBA_8888 || format == (int)HAL_PIXEL_FORMAT_RGB_565) ) {

            if (usage & GRALLOC_USAGE_PRIVATE_NONECACHE) {
                hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_NONE_CACHED | private_handle_t::PRIV_FLAGS_USES_HDMI,
                                           size, 0, private_handle_t::LOCK_STATE_MAPPED, 0, 0);
            } else {
                hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_HDMI,
                                           size, 0, private_handle_t::LOCK_STATE_MAPPED, 0, 0);
            }
        } else {

            if (usage & GRALLOC_USAGE_PRIVATE_NONECACHE) {
                hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_NONE_CACHED | private_handle_t::PRIV_FLAGS_USES_IOCTL,
                                           size, 0, private_handle_t::LOCK_STATE_MAPPED, 0, 0);
            } else {
                hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_IOCTL,
                                           size, 0, private_handle_t::LOCK_STATE_MAPPED, 0, 0);
            }

        }

        *pHandle = hnd;
        hnd->backing_store = next_backing_store_id();
        hnd->format = format;
        hnd->usage = usage;
        hnd->width = w;
        hnd->height = h;
        hnd->bpp = bpp;
        hnd->paddr = current_address;
        hnd->yaddr = gReservedMemSize;
        hnd->offset = buffer_offset;
        hnd->stride = stride;
        hnd->fd = gFimc1Fd;
        hnd->uoffset = (EXYNOS4_ALIGN((EXYNOS4_ALIGN(hnd->width, 16) * EXYNOS4_ALIGN(hnd->height, 16)), 4096));
        hnd->voffset = (EXYNOS4_ALIGN((EXYNOS4_ALIGN((hnd->width >> 1), 16) * EXYNOS4_ALIGN((hnd->height >> 1), 16)), 4096));

        buffer_offset += size;

        ALOGD_IF(debug_level > 0, "%s hnd=%x paddr=%x yaddr=%x offset=%x", __func__, hnd, current_address, gReservedMemSize, buffer_offset);
        return 0;
    }

    if (usage & GRALLOC_USAGE_HW_FIMC1) {
        ALOGD_IF(debug_level > 0, "%s usage = GRALLOC_USAGE_HW_FIMC1", __func__);

        if (!ion_dev_open) {
            ALOGE("%s ERROR, failed to open ion", __func__);
            return -1;
        }

        m = reinterpret_cast<private_module_t*>(dev->common.module);

        if (usage < 0 || usage & GRALLOC_USAGE_HWC_HWOVERLAY )
        {
            if ((format == HAL_PIXEL_FORMAT_RGBA_8888) || (format == HAL_PIXEL_FORMAT_RGB_565)) {
                priv_alloc_flag |= (private_handle_t::PRIV_FLAGS_USES_ION | private_handle_t::PRIV_FLAGS_USES_HDMI);
            } else {
                priv_alloc_flag |= private_handle_t::PRIV_FLAGS_USES_ION;
            }
        } else {
            priv_alloc_flag |= private_handle_t::PRIV_FLAGS_USES_ION;
        }

        if (usage & GRALLOC_USAGE_PRIVATE_NONECACHE) {
            priv_alloc_flag |= private_handle_t::PRIV_FLAGS_NONE_CACHED;
            ion_flags = ION_EXYNOS_NONCACHE_MASK | ION_HEAP_EXYNOS_CONTIG_MASK;
        } else {
            ion_flags = ION_HEAP_EXYNOS_CONTIG_MASK;
        }

        ion_fd = ion_alloc(m->ion_client, size, 0, ion_flags);
        if (ion_fd < 0) {
            ALOGE("%s Failed to ion_alloc", __func__);
            return -1;
        }

        ion_paddr = ion_getphys(m->ion_client, ion_fd);

/* TODO: #ifdef SAMSUNG_EXYNOS_CACHE_UMP here...*/
        if (usage & GRALLOC_USAGE_PRIVATE_NONECACHE) {
            ALOGD_IF(debug_level > 0, "%s FIMC1 none", __func__);
            ump_mem_handle = ump_ref_drv_ion_import(ion_fd, UMP_REF_DRV_CONSTRAINT_NONE);
        } else {
            ALOGD_IF(debug_level > 0, "%s FIMC1 cached", __func__);
            ump_mem_handle = ump_ref_drv_ion_import(ion_fd, UMP_REF_DRV_CONSTRAINT_USE_CACHE);
            ump_cpu_msync_now((ump_handle)ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, NULL, 0);
        }

    } else {

#ifdef SAMSUNG_EXYNOS_CACHE_UMP
        if ((usage & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN) {
            ALOGD_IF(debug_level > 0, "%s UMP cached", __func__);
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_USE_CACHE);
            ump_cpu_msync_now((ump_handle)ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, NULL, 0);
        } else {
            ALOGD_IF(debug_level > 0, "%s UMP none", __func__);
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_NONE);
        }
#else
        else
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_NONE);
#endif

    }

    if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle) {
        cpu_ptr = ump_mapped_pointer_get(ump_mem_handle);
        if (NULL != cpu_ptr) {
            ump_id = ump_secure_id_get(ump_mem_handle);
            if (UMP_INVALID_SECURE_ID != ump_id) {
                private_handle_t* hnd;

                hnd = new private_handle_t(priv_alloc_flag, size, (int)cpu_ptr,
                private_handle_t::LOCK_STATE_MAPPED, ump_id, ump_mem_handle, ion_fd, 0, 0);

                if (NULL != hnd) {
                    *pHandle = hnd;

#ifdef USE_PARTIAL_FLUSH
                    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
                        private_handle_rect *psRect;
                        private_handle_rect *psFRect;
                        psRect = (private_handle_rect *)calloc(1, sizeof(private_handle_rect));
                        psRect->handle = (int)hnd->ump_id;
                        psRect->stride = stride_raw;
                        psFRect = find_last_rect((int)hnd->ump_id);
                        psFRect->next = psRect;
                    }
#endif

                    hnd->backing_store = next_backing_store_id();
                    hnd->format = format;
                    hnd->usage = usage;
                    hnd->width = w;
                    hnd->height = h;
                    hnd->bpp = bpp;
                    hnd->stride = stride;
                    hnd->uoffset = ((EXYNOS4_ALIGN(hnd->width, 16) * EXYNOS4_ALIGN(hnd->height, 16)));
                    hnd->voffset = ((EXYNOS4_ALIGN((hnd->width / 2), 16) * EXYNOS4_ALIGN((hnd->height / 2), 16)));
                    hnd->paddr = ion_paddr;

                    ALOGD_IF(debug_level > 0, "%s hnd->format=0x%x hnd->uoffset=%d hnd->voffset=%d hnd->paddr=%x hnd->bpp=%d", __func__, hnd->format, hnd->uoffset, hnd->voffset, hnd->paddr, hnd->bpp);
                    return 0;
                } else {
                    ALOGE("%s failed to allocate handle", __func__);
                }
            } else {
                ALOGE("%s failed to retrieve valid secure id", __func__);
            }

            ump_mapped_pointer_release(ump_mem_handle);
        } else {
            ALOGE("%s failed to map UMP memory", __func__);
        }

        ump_reference_release(ump_mem_handle);
    } else {
        ALOGE("%s failed to allocate UMP memory", __func__);
    }

    return -1;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev, size_t size, int usage,
                                            buffer_handle_t* pHandle, int w, int h,
                                            int format, int bpp, int stride)
{
    ALOGV("%s: size:%d usage:%d w:%d h:%d format:%d bpp:%d ",
        __func__, size, usage, w,h, format, bpp);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    ALOGD_IF(debug_level > 0, "%s size=0x%x usage=0x%x w=%d h=%d format=0x%x(%d) bpp=%d stride=%d", __func__, size, usage, w, h, format, format, bpp, stride);

    /* allocate the framebuffer */
    if (m->framebuffer == NULL) {
        /* initialize the framebuffer, the framebuffer is mapped once and forever. */
        int err = init_frame_buffer_locked(m);
        if (err < 0)
        {
            return err;
        }
    }

    const uint32_t bufferMask = m->bufferMask;
    const uint32_t numBuffers = m->numBuffers;
    const size_t bufferSize = m->finfo.line_length * m->info.yres;
    if (numBuffers == 1) {
        /*
         * If we have only one buffer, we never use page-flipping. Instead,
         * we return a regular buffer which will be memcpy'ed to the main
         * screen when post is called.
         */
        int newUsage = (usage & ~GRALLOC_USAGE_HW_FB) | GRALLOC_USAGE_HW_2D;
        ALOGE("%s fallback to single buffering", __func__);
        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle, w, h, format, bpp, 0, 0);
    }

    if (bufferMask >= ((1LU<<numBuffers)-1))
    {
        // We ran out of buffers.
        ALOGE("%s Ran out of buffers",__func__); //wjj added
        return -ENOMEM;
    }

    int current = m->framebuffer->base;
    int vaddr = m->finfo.smem_start - current;
    int l_paddr = m->finfo.smem_start;

    ALOGD_IF(debug_level > 0, "%s current=0x%x vaddr=0x%x l_paddr=0x%x before", __func__, current, vaddr, l_paddr);

    /* find a free slot */
    for (uint32_t i = 0; i < numBuffers; i++) {
        if ((bufferMask & (1LU<<i)) == 0) {
            m->bufferMask |= (1LU<<i);
            break;
        }
        current += bufferSize;
        l_paddr = vaddr + current;
    }

    ALOGD_IF(debug_level > 0, "%s current=0x%x vaddr=0x%x l_paddr=0x%x after", __func__, current, vaddr, l_paddr);

    /*
     * The entire framebuffer memory is already mapped,
     * now create a buffer object for parts of this memory
     */
    private_handle_t* hnd = new private_handle_t
            (private_handle_t::PRIV_FLAGS_FRAMEBUFFER, size, current,
             0, dup(m->framebuffer->fd), current - m->framebuffer->base);

    hnd->format = format;
    hnd->usage = usage;
    hnd->width = w;
    hnd->height = h;
    hnd->bpp = bpp;
    hnd->backing_store = next_backing_store_id();
    hnd->paddr = l_paddr;
    hnd->stride = stride;

    *pHandle = hnd;

    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev, size_t size, int usage,
                                     buffer_handle_t* pHandle, int w, int h,
                                     int format, int bpp, int stride)
{
    ALOGV("%s: size:%d usage:%d w:%d h:%d format:%d bpp:%d",
        __func__, size, usage, w,h, format, bpp);

    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle, w, h, format, bpp, stride);
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int alloc_device_alloc(alloc_device_t* dev, int w, int h, int format,
                              int usage, buffer_handle_t* pHandle, int* pStride)
{
    ALOGV("%s: usage:%d w:%d h:%d format:%d ",
        __func__, usage, w,h, format);

    int l_usage = usage;
    int bpp = 0;

    if (!pHandle || !pStride || w < 0 || h < 0)
        return -EINVAL;

    ALOGD_IF(debug_level > 0, "%s w=%d h=%d format=0x%x usage=0x%x", __func__, w, h, format, usage);

    size_t size = 0;
    size_t stride = 0;
    size_t stride_raw = 0;

    if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP ||
        format == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_422_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_420_P  ||
        format == HAL_PIXEL_FORMAT_YV12 ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED ||
        format == GGL_PIXEL_FORMAT_L_8 ||
        format == OMX_COLOR_FormatYUV420Planar ||
        format == OMX_COLOR_FormatYUV420SemiPlanar ||
        /* TODO: find constants for these literals, it's no problem if they are duplicated */
        format == 0x200 ||
        format == 0x201 ||
        format == 0x202 ||
        format == 0x203 ||
        format == 0x204 ||
        format == 0x205 ||
        format == 0x206 ||
        format == 0x207 ||
        format == 0x208 ||
        format == 0x209 ||
        format == 0x210 ||
        format == 0x211 ||
        format == 0x212 ||
        format == 0x213 ||
        format == 0x214 ||
        format == 0x215 ) {

        /* FIXME: there is no way to return the vstride */
        int vstride = 0;

        stride = EXYNOS4_ALIGN(w, 16);
        vstride = EXYNOS4_ALIGN(h, 16);

        /* take borrowed logic which was deactivated in https://github.com/NamelessRom/android_frameworks_native/commit/4860d41c044e0cff732fbfa13d83af76fc37e1b5 */
        if ((format == HAL_PIXEL_FORMAT_YCbCr_420_P) ||
            (format == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
            (format == HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED)) {
            // 0x101 = HAL_PIXEL_FORMAT_YCbCr_420_P (Samsung-specific pixel format)
            // 0x105 = HAL_PIXEL_FORMAT_YCbCr_420_SP (Samsung-specific pixel format)
            // 0x107 = HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED (Samsung-specific pixel format)

            /* We need at least 0x1802930 for playing video */
            if (!(l_usage & GRALLOC_USAGE_HW_FIMC1)) {
                l_usage |= GRALLOC_USAGE_HW_FIMC1; // Exynos HWC wants FIMC-friendly memory allocation
                ALOGD("%s added usage GRALLOC_USAGE_HW_FIMC1 because of format\n", __func__);
            }

            if (!(l_usage & GRALLOC_USAGE_PRIVATE_NONECACHE)) {
                l_usage |= GRALLOC_USAGE_PRIVATE_NONECACHE; // Exynos HWC wants FIMC-friendly memory allocation
                ALOGD("%s added usage GRALLOC_USAGE_PRIVATE_NONECACHE because of format\n", __func__);
            }

            if (!(l_usage & GRALLOC_USAGE_SW_WRITE_OFTEN)) {
                l_usage |= GRALLOC_USAGE_SW_WRITE_OFTEN; // Exynos HWC wants FIMC-friendly memory allocation
                ALOGD("%s added usage GRALLOC_USAGE_SW_WRITE_OFTEN because of format\n", __func__);
            }
        }

        switch (format) {
        case HAL_PIXEL_FORMAT_YV12: //0x32315659
            l_usage |= GRALLOC_USAGE_HW_FIMC1;
            ALOGD("%s added GRALLOC_USAGE_HW_FIMC1 because of YV12", __func__);

        case 0x200:
        case 0x201:
        case 0x202:
        case 0x203:
        case 0x204:
        case 0x205:
        case 0x206:
        case 0x207:
        case 0x208:
        case 0x209:
        case 0x210:
        case 0x211:
        case 0x212:
        case 0x213:
        case 0x214:
        case 0x215:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP: //0x111
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED: //0x112
        case HAL_PIXEL_FORMAT_YCbCr_420_SP: //0x105
        case HAL_PIXEL_FORMAT_YCrCb_420_SP: //0x11
        case HAL_PIXEL_FORMAT_YCbCr_420_P: //0x101
            //lc_20a4
            size = (stride * vstride) + ((EXYNOS4_ALIGN(w/2,16) * EXYNOS4_ALIGN(h/2,16)) * 2);
            //stride_raw = l_usage & GRALLOC_USAGE_HW_FIMC1; //It is later overwritten to 0

            if (l_usage & GRALLOC_USAGE_HW_FIMC1)
                size += (PAGE_SIZE * 2);

            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_SP: //0x10
            size = (stride * vstride) + EXYNOS4_ALIGN(w * h / 2, 2);
            break;

        case GGL_PIXEL_FORMAT_L_8: //0x9 , defined in system/core/include/pixelflinger/format.h
            size = (stride * vstride);
            break;

        /* These two formats are not in stock XXUUMK6 */
        case OMX_COLOR_FormatYUV420Planar: //0x13
        case OMX_COLOR_FormatYUV420SemiPlanar: //0x15
            size = stride * vstride + EXYNOS4_ALIGN((w / 2), 16) * EXYNOS4_ALIGN((h / 2), 16) * 2;
            if (l_usage & GRALLOC_USAGE_HW_FIMC1)
                size += PAGE_SIZE * 2;

            break;

        default:
            return -EINVAL;
        }

    } else {
        ALOGD_IF(debug_level > 0, "%s about to get bpp", __func__);
        bpp = get_bpp(format);
        if (bpp == 0)
            return -EINVAL;

        stride_raw = EXYNOS4_ALIGN(w*bpp,8);
        size = stride_raw * h;
        stride = stride_raw / bpp;
    }

    int err;

    pthread_mutex_lock(&l_surface);
    if (l_usage & GRALLOC_USAGE_HW_FB)
        err = gralloc_alloc_framebuffer(dev, size, l_usage, pHandle, w, h, format, 32, stride);
    else
        err = gralloc_alloc_buffer(dev, size, l_usage, pHandle, w, h, format, 0, (int)stride_raw, (int)stride);

    pthread_mutex_unlock(&l_surface);

    if (err < 0)
        return err;

    *pStride = stride;
    return 0;
}

static int alloc_device_free(alloc_device_t* dev, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0)
        return -EINVAL;

    ALOGD_IF(debug_level > 0, "%s",__func__);

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    pthread_mutex_lock(&l_surface);

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        /* free this buffer */
        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;

        m->bufferMask &= ~(1<<index);
        close(hnd->fd);

    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        ALOGD_IF(debug_level > 0, "%s hnd->ump_mem_handle=%d hnd->ump_id=%d", __func__, hnd->ump_mem_handle, hnd->ump_id);

#ifdef USE_PARTIAL_FLUSH
        if (!release_rect((int)hnd->ump_id))
            ALOGE("%s secure id: 0x%x, release error", __func__, (int)hnd->ump_id);
#endif

        ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
        ump_reference_release((ump_handle)hnd->ump_mem_handle);
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
        ion_free(hnd->fd);
    }

    pthread_mutex_unlock(&l_surface);
    delete hnd;

    return 0;
}

static int alloc_device_close(struct hw_device_t *device)
{
    ALOGD_IF(debug_level > 0, "%s",__func__);

    alloc_device_t* dev = reinterpret_cast<alloc_device_t*>(device);
    if (dev) {
        private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
        if (ion_dev_open)
            ion_client_destroy(m->ion_client);
        delete dev;
        ump_close();
    }
    return 0;
}

int alloc_device_open(hw_module_t const* module, const char* name __unused, hw_device_t** device)
{
    alloc_device_t *dev;

    dev = new alloc_device_t;
    if (NULL == dev)
        return -1;

    ALOGD_IF(debug_level > 0, "%s",__func__);

    dev->common.module = const_cast<hw_module_t*>(module);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);

    m->ion_client = ion_client_create();
    ump_result ump_res = ump_open();

    if (0 > m->ion_client)
        ion_dev_open = false;

    if (UMP_OK != ump_res) {
        ALOGE("%s UMP open failed ump_res %d", __func__, ump_res);
        delete dev;
        return -1;
    }

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = const_cast<hw_module_t*>(module);
    dev->common.close = alloc_device_close;
    dev->alloc = alloc_device_alloc;
    dev->free = alloc_device_free;

    *device = &dev->common;

    return 0;
}
