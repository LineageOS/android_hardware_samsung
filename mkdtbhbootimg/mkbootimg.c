/* tools/mkbootimg/mkbootimg.c
**
** Copyright 2007, The Android Open Source Project
** Copyright 2013, Sony Mobile Communications
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** June 2014, Ketut Putu Kumajaya
** Modified for Samsung Exynos DTBH, based on mkqcdtbootimg from Sony
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <stdint.h>

/* must be provided by the device tree */
#include <samsung_dtbh.h>

#include "libfdt.h"
#include "mincrypt/sha.h"
#include "bootimg.h"

struct dt_blob;

struct dt_entry {
    uint32_t chip;
    uint32_t platform;
    uint32_t subtype;
    uint32_t hw_rev;
    uint32_t hw_rev_end;
    uint32_t offset;
    uint32_t size; /* including padding */
    uint32_t space;

    struct dt_blob *blob;
};

/*
 * Comparator for sorting dt_entries
 */
static int dt_entry_cmp(const void *ap, const void *bp)
{
    struct dt_entry *a = (struct dt_entry*)ap;
    struct dt_entry *b = (struct dt_entry*)bp;

    if (a->chip != b->chip)
        return a->chip - b->chip;
    return a->hw_rev - b->hw_rev;
}

/*
 * In memory representation of a dtb blob
 */
struct dt_blob {
    uint32_t size;
    uint32_t offset;

    void *payload;
    struct dt_blob *next;
};

static void *load_file(const char *fn, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;

    data = 0;
    fd = open(fn, O_RDONLY);
    if(fd < 0) return 0;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);

    if(_sz) *_sz = sz;
    return data;

oops:
    close(fd);
    if(data != 0) free(data);
    return 0;
}

static void *load_dtbh_block(const char *dtb_path, unsigned pagesize, unsigned *_sz)
{
    const unsigned pagemask = pagesize - 1;
    struct dt_entry *new_entries;
    struct dt_entry *entries = NULL;
    struct dt_entry *entry;
    struct dt_blob *blob;
    struct dt_blob *blob_list = NULL;
    struct dt_blob *last_blob = NULL;
    struct dirent *de;
    unsigned new_count;
    unsigned entry_count = 0;
    unsigned offset;
    unsigned dtb_sz;
    unsigned hdr_sz = DT_HEADER_PHYS_SIZE;
    uint32_t version = DTBH_VERSION;
    unsigned blob_sz = 0;
    char fname[PATH_MAX];
    const unsigned *prop_chip;
    const unsigned *prop_platform;
    const unsigned *prop_subtype;
    const unsigned *prop_hw_rev;
    const unsigned *prop_hw_rev_end;
    int namlen;
    int len;
    void *dtb;
    char *dtbh;
    DIR *dir;
    unsigned c;

    dir = opendir(dtb_path);

    if (dir == NULL)
        err(1, "failed to open '%s'", dtb_path);

    while ((de = readdir(dir)) != NULL) {
        namlen = strlen(de->d_name);
        if (namlen < 4 || strcmp(&de->d_name[namlen - 4], ".dtb"))
            continue;

        snprintf(fname, sizeof(fname), "%s/%s", dtb_path, de->d_name);

        dtb = load_file(fname, &dtb_sz);
        if (dtb == NULL)
            err(1, "failed to read dtb '%s'", fname);

        if (fdt_check_header(dtb) != 0) {
            warnx("'%s' is not a valid dtb, skipping", fname);
            free(dtb);
            continue;
        }

        offset = fdt_path_offset(dtb, "/");
        prop_chip = fdt_getprop(dtb, offset, "model_info-chip", &len);
        if (len % (sizeof(uint32_t)) != 0) {
            warnx("model_info-chip of %s is of invalid size, skipping", fname);
            free(dtb);
            continue;
        }

        prop_platform = fdt_getprop(dtb, offset, "model_info-platform", &len);
        if (strcmp((char *)&prop_platform[0], DTBH_PLATFORM)) {
            warnx("model_info-platform of %s is invalid, skipping", fname);
            free(dtb);
            continue;
        }

        prop_subtype = fdt_getprop(dtb, offset, "model_info-subtype", &len);
        if (strcmp((char *)&prop_subtype[0], DTBH_SUBTYPE)) {
            warnx("model_info-subtype of %s is invalid, skipping", fname);
            free(dtb);
            continue;
        }

        prop_hw_rev = fdt_getprop(dtb, offset, "model_info-hw_rev", &len);
        if (len % (sizeof(uint32_t)) != 0) {
            warnx("model_info-hw_rev of %s is of invalid size, skipping", fname);
            free(dtb);
            continue;
        }

        prop_hw_rev_end = fdt_getprop(dtb, offset, "model_info-hw_rev_end", &len);
        if (len % (sizeof(uint32_t)) != 0) {
            warnx("model_info-hw_rev_end of %s is of invalid size, skipping", fname);
            free(dtb);
            continue;
        }

        blob = calloc(1, sizeof(struct dt_blob));
        if (blob == NULL)
            err(1, "failed to allocate memory");

        blob->payload = dtb;
        blob->size = dtb_sz;
        if (blob_list == NULL) {
            blob_list = blob;
            last_blob = blob;
        } else {
            last_blob->next = blob;
            last_blob = blob;
        }

        blob_sz += (blob->size + pagemask) & ~pagemask;
        new_count = entry_count + 1;
        new_entries = realloc(entries, new_count * sizeof(struct dt_entry));
        if (new_entries == NULL)
            err(1, "failed to allocate memory");

        entries = new_entries;
        entry = &entries[entry_count];
        memset(entry, 0, sizeof(*entry));
        entry->chip = ntohl(prop_chip[0]);
        entry->platform = DTBH_PLATFORM_CODE;
        entry->subtype = DTBH_SUBTYPE_CODE;
        entry->hw_rev = ntohl(prop_hw_rev[0]);
        entry->hw_rev_end = ntohl(prop_hw_rev_end[0]);
        entry->space = 0x20; /* space delimiter */
        entry->blob = blob;

        entry_count++;

        hdr_sz += entry_count * DT_ENTRY_PHYS_SIZE;
    }

    closedir(dir);

    if (entry_count == 0) {
        warnx("unable to locate any dtbs in the given path");
        return NULL;
    }

    hdr_sz += sizeof(uint32_t); /* eot marker */
    hdr_sz = (hdr_sz + pagemask) & ~pagemask;

    qsort(entries, entry_count, sizeof(struct dt_entry), dt_entry_cmp);

    /* The size of the dt header is now known, calculate the blob offsets... */
    offset = hdr_sz;
    for (blob = blob_list; blob; blob = blob->next) {
        blob->offset = offset;
        offset += (blob->size + pagemask) & ~pagemask;
    }

    /* ...and update the entries */
    for (c = 0; c < entry_count; c++) {
        entry = &entries[c];

        entry->offset = entry->blob->offset;
        entry->size = (entry->blob->size + pagemask) & ~pagemask;
    }

    /*
     * All parts are now gathered, so build the dt block
     */
    dtbh = calloc(hdr_sz + blob_sz, 1);
    if (dtbh == NULL)
            err(1, "failed to allocate memory");

    offset = 0;

    memcpy(dtbh, DTBH_MAGIC, sizeof(uint32_t));
    memcpy(dtbh + sizeof(uint32_t), &version, sizeof(uint32_t));
    memcpy(dtbh + (sizeof(uint32_t) * 2), &entry_count, sizeof(uint32_t));

    offset += DT_HEADER_PHYS_SIZE;

    /* add dtbh entries */
    for (c = 0; c < entry_count; c++) {
        entry = &entries[c];
        memcpy(dtbh + offset, entry, DT_ENTRY_PHYS_SIZE);
        offset += DT_ENTRY_PHYS_SIZE;
    }

    /* add padding after dt header */
    offset += pagesize - (offset & pagemask);

    for (blob = blob_list; blob; blob = blob->next) {
        memcpy(dtbh + offset, blob->payload, blob->size);
        offset += (blob->size + pagemask) & ~pagemask;
    }

    *_sz = hdr_sz + blob_sz;

    return dtbh;
}

int usage(void)
{
    fprintf(stderr,"usage: mkbootimg\n"
            "       --kernel <filename>\n"
            "       --ramdisk <filename>\n"
            "       [ --second <2ndbootloader-filename> ]\n"
            "       [ --cmdline <kernel-commandline> ]\n"
            "       [ --board <boardname> ]\n"
            "       [ --base <address> ]\n"
            "       [ --pagesize <pagesize> ]\n"
            "       [ --ramdisk_offset <address> ]\n"
            "       [ --dt_dir <dtb path> ]\n"
            "       [ --dt <filename> ]\n"
            "       [ --signature <filename> ]\n"
            "       -o|--output <filename>\n"
            );
    return 1;
}



static unsigned char padding[131072] = { 0, };

int write_padding(int fd, unsigned pagesize, unsigned itemsize)
{
    unsigned pagemask = pagesize - 1;
    unsigned count;

    if((itemsize & pagemask) == 0) {
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    if(write(fd, padding, count) != count) {
        return -1;
    } else {
        return 0;
    }
}

int main(int argc, char **argv)
{
    boot_img_hdr hdr;

    char *kernel_fn = 0;
    void *kernel_data = 0;
    char *ramdisk_fn = 0;
    void *ramdisk_data = 0;
    char *second_fn = 0;
    void *second_data = 0;
    char *cmdline = "";
    char *bootimg = 0;
    char *board = "";
    char *dt_dir = 0;
    char *dt_fn = 0;
    void *dt_data = 0;
    char *sig_fn = 0;
    void *sig_data = 0;
    unsigned pagesize = 2048;
    int fd;
    SHA_CTX ctx;
    uint8_t* sha;
    unsigned base           = 0x10000000;
    unsigned kernel_offset  = 0x00008000;
    unsigned ramdisk_offset = 0x01000000;
    unsigned second_offset  = 0x00f00000;
    unsigned tags_offset    = 0x00000100;

    argc--;
    argv++;

    memset(&hdr, 0, sizeof(hdr));

    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        if(argc < 2) {
            return usage();
        }
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            bootimg = val;
        } else if(!strcmp(arg, "--kernel")) {
            kernel_fn = val;
        } else if(!strcmp(arg, "--ramdisk")) {
            ramdisk_fn = val;
        } else if(!strcmp(arg, "--second")) {
            second_fn = val;
        } else if(!strcmp(arg, "--cmdline")) {
            cmdline = val;
        } else if(!strcmp(arg, "--base")) {
            base = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--kernel_offset")) {
            kernel_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--ramdisk_offset")) {
            ramdisk_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--second_offset")) {
            second_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--tags_offset")) {
            tags_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--board")) {
            board = val;
        } else if(!strcmp(arg,"--pagesize")) {
            pagesize = strtoul(val, 0, 10);
            if ((pagesize != 2048) && (pagesize != 4096) && (pagesize != 8192) && (pagesize != 16384) && (pagesize != 32768) && (pagesize != 65536) && (pagesize != 131072)) {
                fprintf(stderr,"error: unsupported page size %d\n", pagesize);
                return -1;
            }
        } else if (!strcmp(arg, "--dt_dir")) {
            dt_dir = val;
        } else if(!strcmp(arg, "--dt")) {
            dt_fn = val;
        } else if(!strcmp(arg, "--signature")) {
            sig_fn = val;
        } else {
            return usage();
        }
    }
    hdr.page_size = pagesize;

    hdr.kernel_addr =  base + kernel_offset;
    hdr.ramdisk_addr = base + ramdisk_offset;
    hdr.second_addr =  base + second_offset;
    hdr.tags_addr =    base + tags_offset;

    if (dt_dir && dt_fn) {
        fprintf(stderr,"error: don't use both --dt_dir and --dt option\n");
        return usage();
    }

    if(bootimg == 0) {
        fprintf(stderr,"error: no output filename specified\n");
        return usage();
    }

    if(kernel_fn == 0) {
        fprintf(stderr,"error: no kernel image specified\n");
        return usage();
    }

    if(ramdisk_fn == 0) {
        fprintf(stderr,"error: no ramdisk image specified\n");
        return usage();
    }

    if(strlen(board) >= BOOT_NAME_SIZE) {
        fprintf(stderr,"error: board name too large\n");
        return usage();
    }

    strcpy(hdr.name, board);

    memcpy(hdr.magic, BOOT_MAGIC, BOOT_MAGIC_SIZE);

    if(strlen(cmdline) > (BOOT_ARGS_SIZE - 1)) {
        fprintf(stderr,"error: kernel commandline too large\n");
        return 1;
    }
    strcpy((char*)hdr.cmdline, cmdline);

    kernel_data = load_file(kernel_fn, &hdr.kernel_size);
    if(kernel_data == 0) {
        fprintf(stderr,"error: could not load kernel '%s'\n", kernel_fn);
        return 1;
    }

    if(!strcmp(ramdisk_fn,"NONE")) {
        ramdisk_data = 0;
        hdr.ramdisk_size = 0;
    } else {
        ramdisk_data = load_file(ramdisk_fn, &hdr.ramdisk_size);
        if(ramdisk_data == 0) {
            fprintf(stderr,"error: could not load ramdisk '%s'\n", ramdisk_fn);
            return 1;
        }
    }

    if(second_fn) {
        second_data = load_file(second_fn, &hdr.second_size);
        if(second_data == 0) {
            fprintf(stderr,"error: could not load secondstage '%s'\n", second_fn);
            return 1;
        }
    }

    if (dt_dir) {
        dt_data = load_dtbh_block(dt_dir, pagesize, &hdr.dt_size);
        if (dt_data == 0) {
            fprintf(stderr, "error: could not load device tree blobs '%s'\n", dt_dir);
            return 1;
        }
    }

    if(dt_fn) {
        dt_data = load_file(dt_fn, &hdr.dt_size);
        if (dt_data == 0) {
            fprintf(stderr,"error: could not load device tree image '%s'\n", dt_fn);
            return 1;
        }
    }

    if(sig_fn) {
        sig_data = load_file(sig_fn, 0);
        if (sig_data == 0) {
            fprintf(stderr,"error: could not load signature '%s'\n", sig_fn);
            return 1;
        }
    }

    /* put a hash of the contents in the header so boot images can be
     * differentiated based on their first 2k.
     */
    SHA_init(&ctx);
    SHA_update(&ctx, kernel_data, hdr.kernel_size);
    SHA_update(&ctx, &hdr.kernel_size, sizeof(hdr.kernel_size));
    SHA_update(&ctx, ramdisk_data, hdr.ramdisk_size);
    SHA_update(&ctx, &hdr.ramdisk_size, sizeof(hdr.ramdisk_size));
    SHA_update(&ctx, second_data, hdr.second_size);
    SHA_update(&ctx, &hdr.second_size, sizeof(hdr.second_size));
    if(dt_data) {
        SHA_update(&ctx, dt_data, hdr.dt_size);
        SHA_update(&ctx, &hdr.dt_size, sizeof(hdr.dt_size));
    }
    sha = SHA_final(&ctx);
    memcpy(hdr.id, sha,
           SHA_DIGEST_SIZE > sizeof(hdr.id) ? sizeof(hdr.id) : SHA_DIGEST_SIZE);

    fd = open(bootimg, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if(fd < 0) {
        fprintf(stderr,"error: could not create '%s'\n", bootimg);
        return 1;
    }

    if(write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) goto fail;
    if(write_padding(fd, pagesize, sizeof(hdr))) goto fail;

    if(write(fd, kernel_data, hdr.kernel_size) != hdr.kernel_size) goto fail;
    if(write_padding(fd, pagesize, hdr.kernel_size)) goto fail;

    if(write(fd, ramdisk_data, hdr.ramdisk_size) != hdr.ramdisk_size) goto fail;
    if(write_padding(fd, pagesize, hdr.ramdisk_size)) goto fail;

    if(second_data) {
        if(write(fd, second_data, hdr.second_size) != hdr.second_size) goto fail;
        if(write_padding(fd, pagesize, hdr.ramdisk_size)) goto fail;
    }

    if(dt_data) {
        if(write(fd, dt_data, hdr.dt_size) != hdr.dt_size) goto fail;
        if(write_padding(fd, pagesize, hdr.dt_size)) goto fail;
    }

    if(sig_data) {
        if(write(fd, sig_data, 256) != 256) goto fail;
    }

    return 0;

fail:
    unlink(bootimg);
    close(fd);
    fprintf(stderr,"error: failed writing '%s': %s\n", bootimg,
            strerror(errno));
    return 1;
}
