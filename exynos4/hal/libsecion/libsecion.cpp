/*
 * Copyright (C) 2012 Samsung Electronics Co., Ltd.
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

#define LOG_TAG "libsecion"

#include <secion.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cutils/log.h>

#define ION_IOC_MAGIC 'I'
#define ION_IOC_ALLOC _IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)
#define ION_IOC_FREE _IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)
#define ION_IOC_MAP _IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)
#define ION_IOC_SHARE _IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)
#define ION_IOC_IMPORT _IOWR(ION_IOC_MAGIC, 5, int)
#define ION_IOC_CUSTOM _IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)

typedef unsigned long ion_handle;

struct ion_allocation_data {
    size_t len;
    size_t align;
    unsigned int flags;
    ion_handle *handle;
};

struct ion_fd_data {
    ion_handle *handle;
    int fd;
};

struct ion_handle_data {
    ion_handle *handle;
};

struct ion_custom_data {
    unsigned int cmd;
    unsigned long arg;
};

struct ion_msync_data {
    enum ION_MSYNC_FLAGS dir;
    int fd;
    size_t size;
    off_t offset;
};

struct ion_phys_data {
    int fd;
    ion_phys_addr_t phys;
    size_t size;
};

enum ION_EXYNOS_CUSTOM_CMD {
    ION_EXYNOS_CUSTOM_MSYNC,
    ION_EXYNOS_CUSTOM_PHYS
};

ion_client ion_client_create(void)
{
    return open("/dev/ion", O_RDWR);
}

void ion_client_destroy(ion_client client)
{
    close(client);
}

ion_buffer ion_alloc(ion_client client, size_t len, size_t align, unsigned int flags)
{
    int ret;
    struct ion_handle_data arg_free;
    struct ion_fd_data arg_share;
    struct ion_allocation_data arg_alloc;

    arg_alloc.len = len;
    arg_alloc.align = align;
    arg_alloc.flags = flags;

    ret = ioctl(client, ION_IOC_ALLOC, &arg_alloc);
    if (ret < 0)
        return ret;

    arg_share.handle = arg_alloc.handle;
    ret = ioctl(client, ION_IOC_SHARE, &arg_share);

    arg_free.handle = arg_alloc.handle;
    ioctl(client, ION_IOC_FREE, &arg_free);

    if (ret < 0)
        return ret;

    return arg_share.fd;
}

void ion_free(ion_buffer buffer)
{
    close(buffer);
}

void *ion_map(ion_buffer buffer, size_t len, off_t offset)
{
    return mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
                buffer, offset);
}

int ion_unmap(void *addr, size_t len)
{
    return munmap(addr, len);
}

int ion_msync(ion_client client, ion_buffer buffer, long flags, size_t size, off_t offset)
{
    struct ion_msync_data arg_cdata;
    arg_cdata.size = size;
    arg_cdata.dir = (ION_MSYNC_FLAGS) flags;
    arg_cdata.fd = buffer;
    arg_cdata.offset = offset;

    struct ion_custom_data arg_custom;
    arg_custom.cmd = ION_EXYNOS_CUSTOM_MSYNC;
    arg_custom.arg = (unsigned long) &arg_cdata;

    return ioctl(client, ION_IOC_CUSTOM, &arg_custom);
}

ion_phys_addr_t ion_getphys(ion_client client, ion_buffer buffer)
{
    struct ion_phys_data arg_cdata;
    arg_cdata.fd = buffer;

    struct ion_custom_data arg_custom;
    arg_custom.cmd = ION_EXYNOS_CUSTOM_PHYS;
    arg_custom.arg = (unsigned long) &arg_cdata;

    if(ioctl(client, ION_IOC_CUSTOM, &arg_custom) < 0)
        return 0;

    return arg_cdata.phys;
}

int createIONMem(struct secion_param *param, size_t size, unsigned int flags)
{
    if(param->client < 0 && (param->client = ion_client_create()) < 0) {
        ALOGE("createIONMem:: ion_client_create fail\n");
        goto fail;
    }

    if(param->buffer < 0 && (param->buffer = ion_alloc(param->client, size, 0x10000, flags)) < 0) {
        ALOGE("createIONMem:: ion_alloc fail\n");
        goto fail;
    }

    if((param->physaddr = ion_getphys(param->client, param->buffer)) == 0) {
        ALOGE("createIONMem:: ion_getphys fail, phys_addr = 0\n");
        goto fail;
    }

    if((param->memory = ion_map(param->buffer, size, 0)) == (void*)-1) {
        ALOGE("createIONMem:: ion_map fail\n");
        goto fail;
    } else {
        param->size = size;
        return 0;
    }

fail:
    if(param->memory > 0) munmap(param->memory, size);
    if(param->buffer > 0) ion_free(param->buffer);
    param->buffer = -1;
    param->size = 0;
    param->memory = 0;
    param->physaddr = 0;
    return -1;
}

int destroyIONMem(struct secion_param *param)
{
    if(param->memory != 0) munmap(param->memory, param->size);
    if(param->buffer >= 0) ion_free(param->buffer);
    param->buffer = -1;
    param->size = 0;
    param->memory = 0;
    param->physaddr = 0;
    return 0;
}
