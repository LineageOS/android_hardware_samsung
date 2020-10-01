/*
 * Copyright (C) 2010 ARM Limited. All rights reserved.
 *
 * Portions of this code have been modified from the original.
 * These modifications are:
 *    * includes
 *    * enums
 *    * gralloc_device_open()
 *    * gralloc_register_buffer()
 *    * gralloc_unregister_buffer()
 *    * gralloc_lock()
 *    * gralloc_unlock()
 *    * gralloc_module_methods
 *    * HAL_MODULE_INFO_SYM
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

#include <errno.h>
#include <pthread.h>

#include <sys/mman.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <fcntl.h>

#include "gralloc_priv.h"
#include "alloc_device.h"
#include "framebuffer_device.h"

#include "ump.h"
#include "ump_ref_drv.h"
#include "secion.h"
#include "s5p_fimc.h"
#include "exynos_mem.h"
static pthread_mutex_t s_map_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sMapLock = PTHREAD_MUTEX_INITIALIZER;

static int s_ump_is_open = 0;
static int gMemfd = 0;
#define PFX_NODE_MEM   "/dev/exynos-mem"

/* we need this for now because pmem cannot mmap at an offset */
#define PMEM_HACK   1
#ifdef USE_PARTIAL_FLUSH
struct private_handle_rect *rect_list;

private_handle_rect *find_rect(int secure_id)
{
    private_handle_rect *psRect;

    for (psRect = rect_list; psRect; psRect = psRect->next)
        if (psRect->handle == secure_id)
            break;
    if (!psRect)
        return NULL;

    return psRect;
}

private_handle_rect *find_last_rect(int secure_id)
{
    private_handle_rect *psRect;
    private_handle_rect *psFRect;

    if (rect_list == NULL) {
        rect_list = (private_handle_rect *)calloc(1, sizeof(private_handle_rect));
        return rect_list;
    }

    for (psRect = rect_list; psRect; psRect = psRect->next) {
        if (psRect->handle == secure_id)
            return psFRect;
        psFRect = psRect;
    }
    return psFRect;
}

int release_rect(int secure_id)
{
    private_handle_rect *psRect;
    private_handle_rect *psTRect;

    for (psRect = rect_list; psRect; psRect = psRect->next) {
        if (psRect->next) {
            if (psRect->next->handle == secure_id) {
                if (psRect->next->next)
                    psTRect = psRect->next->next;
                else
                    psTRect = NULL;

                free(psRect->next);
                psRect->next = psTRect;
                return 1;
            }
        }
    }

    return 0;
}
#endif

static int gralloc_map(gralloc_module_t const* module,
        buffer_handle_t handle, void** vaddr)
{
    private_handle_t* hnd = (private_handle_t*)handle;
    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)) {
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
            size_t size = FIMC1_RESERVED_SIZE * 1024;
            void *mappedAddress = mmap(0, size,
                    PROT_READ|PROT_WRITE, MAP_SHARED, gMemfd, (hnd->paddr - hnd->offset));
            if (mappedAddress == MAP_FAILED) {
                ALOGE("Could not mmap %s fd(%d)", strerror(errno),hnd->fd);
                return -errno;
            }
            hnd->base = intptr_t(mappedAddress) + hnd->offset;
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
            size_t size = hnd->size;
            hnd->ion_client = ion_client_create();
            void *mappedAddress = ion_map(hnd->fd, size, 0);

            if (mappedAddress == MAP_FAILED) {
                ALOGE("Could not ion_map %s fd(%d)", strerror(errno), hnd->fd);
                return -errno;
            }

            hnd->base = intptr_t(mappedAddress) + hnd->offset;
        } else {
            size_t size = hnd->size;
#if PMEM_HACK
            size += hnd->offset;
#endif
            void *mappedAddress = mmap(0, size,
                    PROT_READ|PROT_WRITE, MAP_SHARED, hnd->fd, 0);
            if (mappedAddress == MAP_FAILED) {
                ALOGE("Could not mmap %s fd(%d)", strerror(errno),hnd->fd);
                return -errno;
            }
            hnd->base = intptr_t(mappedAddress) + hnd->offset;
        }
    }
    *vaddr = (void*)hnd->base;
    return 0;
}

static int gralloc_unmap(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    private_handle_t* hnd = (private_handle_t*)handle;
    if (!(hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)) {
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
            void* base = (void*)(intptr_t(hnd->base) - hnd->offset);
            size_t size = FIMC1_RESERVED_SIZE * 1024;
            if (munmap(base, size) < 0)
                ALOGE("Could not unmap %s", strerror(errno));
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
            void* base = (void*)hnd->base;
            size_t size = hnd->size;
            if (ion_unmap(base, size) < 0)
                ALOGE("Could not ion_unmap %s", strerror(errno));
            ion_client_destroy(hnd->ion_client);
        } else {
            void* base = (void*)hnd->base;
            size_t size = hnd->size;
#if PMEM_HACK
            base = (void*)(intptr_t(base) - hnd->offset);
            size += hnd->offset;
#endif
            if (munmap(base, size) < 0)
                ALOGE("Could not unmap %s", strerror(errno));
        }
    }
    hnd->base = 0;
    return 0;
}

static int gralloc_device_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    int status = -EINVAL;

    if (!strcmp(name, GRALLOC_HARDWARE_GPU0))
        status = alloc_device_open(module, name, device);
    else if (!strcmp(name, GRALLOC_HARDWARE_FB0))
        status = framebuffer_device_open(module, name, device);

    return status;
}

static int gralloc_register_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
    int err = 0;
    int retval = -EINVAL;
    void *vaddr;
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Registering invalid buffer, returning error");
        return -EINVAL;
    }

    /* if this handle was created in this process, then we keep it as is. */
    private_handle_t* hnd = (private_handle_t*)handle;

#ifdef USE_PARTIAL_FLUSH
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        private_handle_rect *psRect;
        private_handle_rect *psFRect;
        psRect = (private_handle_rect *)calloc(1, sizeof(private_handle_rect));
        psRect->handle = (int)hnd->ump_id;
        psRect->stride = (int)hnd->stride;
        psFRect = find_last_rect((int)hnd->ump_id);
        psFRect->next = psRect;
    }
#endif

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
        err = gralloc_map(module, handle, &vaddr);

    pthread_mutex_lock(&s_map_lock);

    if (!s_ump_is_open) {
        ump_result res = ump_open(); /* TODO: Fix a ump_close() somewhere??? */
        if (res != UMP_OK) {
            pthread_mutex_unlock(&s_map_lock);
            ALOGE("Failed to open UMP library");
            return retval;
        }
        s_ump_is_open = 1;
    }

    hnd->pid = getpid();

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
        hnd->ump_mem_handle = (int)ump_handle_create_from_secure_id(hnd->ump_id);
        if (UMP_INVALID_MEMORY_HANDLE != (ump_handle)hnd->ump_mem_handle) {
            hnd->base = (int)ump_mapped_pointer_get((ump_handle)hnd->ump_mem_handle);
            if (0 != hnd->base) {
                hnd->lockState = private_handle_t::LOCK_STATE_MAPPED;
                hnd->writeOwner = 0;
                hnd->lockState = 0;

                pthread_mutex_unlock(&s_map_lock);
                return 0;
            } else {
                ALOGE("Failed to map UMP handle");
            }

            ump_reference_release((ump_handle)hnd->ump_mem_handle);
        } else {
            ALOGE("Failed to create UMP handle");
        }
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_PMEM) {
        pthread_mutex_unlock(&s_map_lock);
        return 0;
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
        void* vaddr = NULL;

        if (gMemfd == 0) {
            gMemfd = open(PFX_NODE_MEM, O_RDWR);
            if (gMemfd < 0) {
                ALOGE("%s:: %s exynos-mem open error\n", __func__, PFX_NODE_MEM);
                return false;
            }
        }

        gralloc_map(module, handle, &vaddr);
        pthread_mutex_unlock(&s_map_lock);
        return 0;
    } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
        hnd->ump_mem_handle = (int)ump_handle_create_from_secure_id(hnd->ump_id);
        if (UMP_INVALID_MEMORY_HANDLE != (ump_handle)hnd->ump_mem_handle) {
            vaddr = (void*)ump_mapped_pointer_get((ump_handle)hnd->ump_mem_handle);
            if (0 != vaddr) {
                hnd->lockState = private_handle_t::LOCK_STATE_MAPPED;
                hnd->writeOwner = 0;
                hnd->lockState = 0;

                pthread_mutex_unlock(&s_map_lock);
                return 0;
            } else {
                ALOGE("Failed to map UMP handle");
            }
            ump_reference_release((ump_handle)hnd->ump_mem_handle);
        } else {
            ALOGE("Failed to create UMP handle");
        }
    } else {
        ALOGE("registering non-UMP buffer not supported");
    }

    pthread_mutex_unlock(&s_map_lock);
    return retval;
}

static int gralloc_unregister_buffer(gralloc_module_t const* module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("unregistering invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;

#ifdef USE_PARTIAL_FLUSH
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP)
        if (!release_rect((int)hnd->ump_id))
            ALOGE("secureID: 0x%x, release error", (int)hnd->ump_id);
#endif
    ALOGE_IF(hnd->lockState & private_handle_t::LOCK_STATE_READ_MASK,
            "[unregister] handle %p still locked (state=%08x)", hnd, hnd->lockState);

    /* never unmap buffers that were not registered in this process */
    if (hnd->pid == getpid()) {
        pthread_mutex_lock(&s_map_lock);
        if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
            ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
            hnd->base = 0;
            ump_reference_release((ump_handle)hnd->ump_mem_handle);
            hnd->ump_mem_handle = (int)UMP_INVALID_MEMORY_HANDLE;
            hnd->lockState  = 0;
            hnd->writeOwner = 0;
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
            if(hnd->base != 0)
                gralloc_unmap(module, handle);

            pthread_mutex_unlock(&s_map_lock);
            if (0 < gMemfd) {
                close(gMemfd);
                gMemfd = 0;
            }
            return 0;
        } else if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION) {
            ump_mapped_pointer_release((ump_handle)hnd->ump_mem_handle);
            ump_reference_release((ump_handle)hnd->ump_mem_handle);
            if (hnd->base)
                gralloc_unmap(module, handle);

            hnd->base = 0;
            hnd->ump_mem_handle = (int)UMP_INVALID_MEMORY_HANDLE;
            hnd->lockState  = 0;
            hnd->writeOwner = 0;
        } else {
            ALOGE("unregistering non-UMP buffer not supported");
        }

        pthread_mutex_unlock(&s_map_lock);
    }

    return 0;
}

static int gralloc_lock(gralloc_module_t const* module, buffer_handle_t handle,
                        int usage, int l, int t, int w, int h, void** vaddr)
{
    int err = 0;
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Locking invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;

#ifdef SAMSUNG_EXYNOS_CACHE_UMP
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
#ifdef USE_PARTIAL_FLUSH
        private_handle_rect *psRect;
        psRect = find_rect((int)hnd->ump_id);
        psRect->l = l;
        psRect->t = t;
        psRect->w = w;
        psRect->h= h;
        psRect->locked = 1;
#endif
    }
#endif
    if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
        *vaddr = (void*)hnd->base;

    if (usage & GRALLOC_USAGE_YUV_ADDR) {
        vaddr[0] = (void*)hnd->base;
        vaddr[1] = (void*)(hnd->base + hnd->uoffset);
        vaddr[2] = (void*)(hnd->base + hnd->uoffset + hnd->voffset);
    }
    return err;
}

static int gralloc_unlock(gralloc_module_t const* module, buffer_handle_t handle)
{
    if (private_handle_t::validate(handle) < 0) {
        ALOGE("Unlocking invalid buffer, returning error");
        return -EINVAL;
    }

    private_handle_t* hnd = (private_handle_t*)handle;

#ifdef SAMSUNG_EXYNOS_CACHE_UMP
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_UMP) {
#ifdef USE_PARTIAL_FLUSH
        private_handle_rect *psRect;
        psRect = find_rect((int)hnd->ump_id);
        ump_cpu_msync_now((ump_handle)hnd->ump_mem_handle, UMP_MSYNC_CLEAN,
                (void *)(hnd->base + (psRect->stride * psRect->t)), psRect->stride * psRect->h );
        return 0;
#endif
        ump_cpu_msync_now((ump_handle)hnd->ump_mem_handle, UMP_MSYNC_CLEAN_AND_INVALIDATE, NULL, 0);
    }
#endif
    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_ION)
        ion_msync(hnd->ion_client, hnd->fd, (ION_MSYNC_FLAGS) (IMSYNC_DEV_TO_RW | IMSYNC_SYNC_FOR_DEV), hnd->size, hnd->offset);

    if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_IOCTL) {
        int ret;
        exynos_mem_flush_range mem;
        mem.start = hnd->paddr;
        mem.length = hnd->size;

        ret = ioctl(gMemfd, EXYNOS_MEM_PADDR_CACHE_FLUSH, &mem);
        if (ret < 0) {
            ALOGE("Error in exynos-mem : EXYNOS_MEM_PADDR_CACHE_FLUSH (%d)\n", ret);
            return false;
        }
    }

    return 0;
}

static int gralloc_getphys(gralloc_module_t const* module, buffer_handle_t handle, void** paddr)
{
    private_handle_t* hnd = (private_handle_t*)handle;
    paddr[0] = (void*)hnd->paddr;
    paddr[1] = (void*)(hnd->paddr + hnd->uoffset);
    paddr[2] = (void*)(hnd->paddr + hnd->uoffset + hnd->voffset);
    return 0;
}

/* There is one global instance of the module */
static struct hw_module_methods_t gralloc_module_methods =
{
    open: gralloc_device_open
};

struct private_module_t HAL_MODULE_INFO_SYM =
{
    base:
    {
        common:
        {
            tag: HARDWARE_MODULE_TAG,
            version_major: 1,
            version_minor: 0,
            id: GRALLOC_HARDWARE_MODULE_ID,
            name: "Graphics Memory Allocator Module",
            author: "ARM Ltd.",
            methods: &gralloc_module_methods,
            dso: NULL,
        },
        registerBuffer: gralloc_register_buffer,
        unregisterBuffer: gralloc_unregister_buffer,
        lock: gralloc_lock,
        unlock: gralloc_unlock,
        getphys: gralloc_getphys,
        perform: NULL,
    },
    framebuffer: NULL,
    flags: 0,
    numBuffers: 0,
    bufferMask: 0,
    lock: PTHREAD_MUTEX_INITIALIZER,
    currentBuffer: NULL,
};
