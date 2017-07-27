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

#include <linux/videodev2.h>
#include "s5p_fimc.h"

#ifdef SAMSUNG_EXYNOS4x12
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

#define PFX_NODE_MEM   "/dev/exynos-mem"
static int gMemfd = 0;

bool ion_dev_open = true;
static pthread_mutex_t l_surface= PTHREAD_MUTEX_INITIALIZER;
static int buffer_offset = 0;
static int gfd = 0;

#ifdef USE_PARTIAL_FLUSH
extern struct private_handle_rect *rect_list;
extern private_handle_rect *find_rect(int secure_id);
extern private_handle_rect *find_last_rect(int secure_id);
extern int release_rect(int secure_id);
#endif

#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

static int gralloc_alloc_buffer(alloc_device_t* dev, size_t size, int usage,
                                buffer_handle_t* pHandle, int w, int h,
                                int format, int bpp, int stride_raw, int stride)
{
    ump_handle ump_mem_handle;
    void *cpu_ptr;
    ump_secure_id ump_id;

    size = round_up_to_page_size(size);
#ifdef INSIGNAL_FIMC1
    if (usage & GRALLOC_USAGE_HW_FIMC1) {
        int dev_fd=0;
        char node[20];
        int ret;
        int paddr=0;
        int offset=0;

        struct v4l2_control     vc;
        sprintf(node, "%s", PFX_NODE_FIMC1);

        if (gfd == 0) {
            gfd = open(node, O_RDWR);

            if (gfd < 0) {
                ALOGE("%s:: %s Post processor open error\n", __func__, node);
                return false;
            }
        }

        vc.id = V4L2_CID_RESERVED_MEM_BASE_ADDR;
        vc.value = 0;
        ret = ioctl(gfd, VIDIOC_G_CTRL, &vc);
        if (ret < 0) {
            ALOGE("Error in video VIDIOC_G_CTRL - V4L2_CID_RESERVED_MEM_BAES_ADDR (%d)\n", ret);
            return false;
        }
        paddr = (unsigned int)vc.value;

        if ((buffer_offset + size) >= FIMC1_RESERVED_SIZE * 1024)
            buffer_offset = 0;

        paddr += buffer_offset;
        private_handle_t* hnd = new private_handle_t(private_handle_t::PRIV_FLAGS_USES_IOCTL, size, 0,
                private_handle_t::LOCK_STATE_MAPPED, 0, 0);

        *pHandle = hnd;
        hnd->format = format;
        hnd->usage = usage;
        hnd->width = w;
        hnd->height = h;
        hnd->bpp = bpp;
        hnd->paddr = paddr;
        hnd->offset = buffer_offset;
        hnd->stride = stride;
        hnd->fd = gfd;
        hnd->uoffset = (EXYNOS4_ALIGN((EXYNOS4_ALIGN(hnd->width, 16) * EXYNOS4_ALIGN(hnd->height, 16)), 4096));
        hnd->voffset = (EXYNOS4_ALIGN((EXYNOS4_ALIGN((hnd->width >> 1), 16) * EXYNOS4_ALIGN((hnd->height >> 1), 16)), 4096));
        buffer_offset += size;

        if (gMemfd == 0) {
            gMemfd = open(PFX_NODE_MEM, O_RDWR);
            if (gMemfd < 0) {
                ALOGE("%s:: %s exynos-mem open error\n", __func__, PFX_NODE_MEM);
                return false;
            }
        }

        size_t size = FIMC1_RESERVED_SIZE * 1024;

        void *mappedAddress = mmap(0, size,
               PROT_READ|PROT_WRITE, MAP_SHARED, gMemfd, (hnd->paddr - hnd->offset));
        hnd->base = intptr_t(mappedAddress) + hnd->offset;
        return 0;
    } else {
#endif
        ion_buffer ion_fd = 0;
        unsigned int ion_flags = 0;
        int priv_alloc_flag = private_handle_t::PRIV_FLAGS_USES_UMP;

#ifdef  INSIGNAL_FIMC1
        if (usage & GRALLOC_USAGE_HW_ION) {
#else
        if (usage & GRALLOC_USAGE_HW_ION || usage & GRALLOC_USAGE_HW_FIMC1) {
#endif
            if (!ion_dev_open) {
                ALOGE("ERROR, failed to open ion");
                return -1;
            }

            private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
            ion_flags = ION_HEAP_EXYNOS_MASK;
            ion_fd = ion_alloc(m->ion_client, size, 0, ion_flags);

            if (ion_fd < 0) {
                ALOGE("Failed to ion_alloc");
                return -1;
            }

            cpu_ptr = ion_map(ion_fd, size, 0);

            if (NULL == cpu_ptr) {
                ALOGE("Failed to ion_map");
                ion_free(ion_fd);
                return -1;
            }

            ump_mem_handle = ump_ref_drv_ion_import(ion_fd, UMP_REF_DRV_CONSTRAINT_NONE);

            if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle) {
                priv_alloc_flag = private_handle_t::PRIV_FLAGS_USES_ION;
            } else {
                ALOGE("gralloc_alloc_buffer() failed to import ION memory");
                ion_unmap(cpu_ptr, size);
                ion_free(ion_fd);
                return -1;
            }
        }
#ifdef SAMSUNG_EXYNOS_CACHE_UMP
        else if ((usage&GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN)
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_USE_CACHE);
        else
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_NONE);
#else
        else
            ump_mem_handle = ump_ref_drv_allocate(size, UMP_REF_DRV_CONSTRAINT_NONE);
#endif
        if (UMP_INVALID_MEMORY_HANDLE != ump_mem_handle) {
            if (!(usage & GRALLOC_USAGE_HW_ION || usage & GRALLOC_USAGE_HW_FIMC1))
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
                        hnd->format = format;
                        hnd->usage = usage;
                        hnd->width = w;
                        hnd->height = h;
                        hnd->bpp = bpp;
                        hnd->stride = stride;
                        if(hnd->format == HAL_PIXEL_FORMAT_YV12) {
                            hnd->uoffset = ((EXYNOS4_ALIGN(hnd->width, 16) * hnd->height));
                            hnd->voffset = ((EXYNOS4_ALIGN((hnd->width >> 1), 16) * (hnd->height >> 1)));
                        } else {
                            hnd->uoffset = ((EXYNOS4_ALIGN(hnd->width, 16) * EXYNOS4_ALIGN(hnd->height, 16)));
                            hnd->voffset = ((EXYNOS4_ALIGN((hnd->width >> 1), 16) * EXYNOS4_ALIGN((hnd->height >> 1), 16)));
                        }
                        return 0;
                    } else {
                        ALOGE("gralloc_alloc_buffer() failed to allocate handle");
                    }
                } else {
                    ALOGE("gralloc_alloc_buffer() failed to retrieve valid secure id");
                }

                ump_mapped_pointer_release(ump_mem_handle);
            } else {
                ALOGE("gralloc_alloc_buffer() failed to map UMP memory");
            }

            ump_reference_release(ump_mem_handle);
        } else {
            ALOGE("gralloc_alloc_buffer() failed to allcoate UMP memory");
        }
#ifdef INSIGNAL_FIMC1
    }
#endif
    return -1;
}

static int gralloc_alloc_framebuffer_locked(alloc_device_t* dev, size_t size, int usage,
                                            buffer_handle_t* pHandle, int w, int h,
                                            int format, int bpp)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    /* allocate the framebuffer */
    if (m->framebuffer == NULL) {
        /* initialize the framebuffer, the framebuffer is mapped once and forever. */
        int err = init_frame_buffer_locked(m);
        if (err < 0)
            return err;
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
        ALOGE("fallback to single buffering");
        return gralloc_alloc_buffer(dev, bufferSize, newUsage, pHandle, w, h, format, bpp, 0, 0);
    }

    if (bufferMask >= ((1LU<<numBuffers)-1))
        return -ENOMEM;

    int vaddr = m->framebuffer->base;
    /* find a free slot */
    for (uint32_t i = 0; i < numBuffers; i++) {
        if ((bufferMask & (1LU<<i)) == 0) {
            m->bufferMask |= (1LU<<i);
            break;
        }
        vaddr += bufferSize;
    }

    /*
     * The entire framebuffer memory is already mapped,
     * now create a buffer object for parts of this memory
     */
    private_handle_t* hnd = new private_handle_t
            (private_handle_t::PRIV_FLAGS_FRAMEBUFFER, size, vaddr,
             0, dup(m->framebuffer->fd), vaddr - m->framebuffer->base);

    hnd->format = format;
    hnd->usage = usage;
    hnd->width = w;
    hnd->height = h;
    hnd->bpp = bpp;

    *pHandle = hnd;

    return 0;
}

static int gralloc_alloc_framebuffer(alloc_device_t* dev, size_t size, int usage,
                                     buffer_handle_t* pHandle, int w, int h,
                                     int format, int bpp)
{
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&m->lock);
    int err = gralloc_alloc_framebuffer_locked(dev, size, usage, pHandle, w, h, format, bpp);
    pthread_mutex_unlock(&m->lock);
    return err;
}

static int alloc_device_alloc(alloc_device_t* dev, int w, int h, int format,
                              int usage, buffer_handle_t* pHandle, int* pStride)
{
    if (!pHandle || !pStride)
        return -EINVAL;

    size_t size = 0;
    size_t stride = 0;
    size_t stride_raw = 0;

    if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED ||
        format == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_422_SP ||
        format == HAL_PIXEL_FORMAT_YCbCr_420_P  ||
        format == HAL_PIXEL_FORMAT_YV12 ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP ||
        format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED ||
        format == GGL_PIXEL_FORMAT_L_8 ||
        format == OMX_COLOR_FormatYUV420Planar ||
        format == OMX_COLOR_FormatYUV420SemiPlanar) {
        /* FIXME: there is no way to return the vstride */
        int vstride;
        stride = EXYNOS4_ALIGN(w, 16);
        vstride = EXYNOS4_ALIGN(h, 16);
        switch (format) {
        case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            size = stride * vstride + EXYNOS4_ALIGN((w / 2), 16) * EXYNOS4_ALIGN((h / 2), 16) * 2;
#ifdef  INSIGNAL_FIMC1
            if (usage & GRALLOC_USAGE_HW_FIMC1) {
#else
            if (usage & GRALLOC_USAGE_HW_ION || usage & GRALLOC_USAGE_HW_FIMC1) {
#endif
                size += PAGE_SIZE * 2;
            }
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            size = (stride * vstride) + (w/2 * h/2) * 2;
            break;
        case GGL_PIXEL_FORMAT_L_8:
            size = (stride * vstride);
            break;
        default:
            return -EINVAL;
        }
    } else {
        int bpp = 0;
        switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            bpp = 3;
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
        case HAL_PIXEL_FORMAT_RGBA_5551:
        case HAL_PIXEL_FORMAT_RGBA_4444:
            bpp = 2;
            break;
        default:
            return -EINVAL;
        }
        size_t bpr = EXYNOS4_ALIGN((w*bpp), 8);
        size = bpr * h;
        stride = bpr / bpp;
        stride_raw = bpr;
    }

    int err;
    pthread_mutex_lock(&l_surface);
    if (usage & GRALLOC_USAGE_HW_FB)
        err = gralloc_alloc_framebuffer(dev, size, usage, pHandle, w, h, format, 32);
    else
        err = gralloc_alloc_buffer(dev, size, usage, pHandle, w, h, format, 0, (int)stride_raw, (int)stride);

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

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(handle);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    pthread_mutex_lock(&l_surface);
    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {
        /* free this buffer */
        const size_t bufferSize = m->finfo.line_length * m->info.yres;
        int index = (hnd->base - m->framebuffer->base) / bufferSize;
        m->bufferMask &= ~(1<<index);
        close(hnd->fd);
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
        void* base = (void*)(intptr_t(hnd->base) - hnd->offset);
        size_t size = FIMC1_RESERVED_SIZE * 1024;
        if (munmap(base, size) < 0)
            ALOGE("Could not unmap %s", strerror(errno));
        if (0 < gMemfd) {
            close(gMemfd);
            gMemfd = 0;
        }
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
#ifdef USE_PARTIAL_FLUSH
        if (!release_rect((int)hnd->ump_id))
            ALOGE("secure id: 0x%x, release error",(int)hnd->ump_id);
#endif
        ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
        ump_reference_release((ump_handle)hnd->ump_mem_handle);
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
#ifdef USE_PARTIAL_FLUSH
        if (!release_rect((int)hnd->ump_id))
            ALOGE("secure id: 0x%x, release error",(int)hnd->ump_id);
#endif
        ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
        ump_reference_release((ump_handle)hnd->ump_mem_handle);

        ion_unmap((void*)hnd->base, hnd->size);
        ion_free(hnd->fd);
    }
    pthread_mutex_unlock(&l_surface);
    delete hnd;

    return 0;
}

static int alloc_device_close(struct hw_device_t *device)
{
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

int alloc_device_open(hw_module_t const* module, const char* name, hw_device_t** device)
{
    alloc_device_t *dev;

    dev = new alloc_device_t;
    if (NULL == dev)
        return -1;

    dev->common.module = const_cast<hw_module_t*>(module);
    private_module_t* m = reinterpret_cast<private_module_t*>(dev->common.module);
    m->ion_client = ion_client_create();
    ump_result ump_res = ump_open();
    if (0 > m->ion_client)
        ion_dev_open = false;
    if (UMP_OK != ump_res) {
        ALOGE("UMP open failed ump_res %d", ump_res);
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
