/*
 * Copyright 2018, The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_PAGE_SIZE 2048
#define DT_NAME_LEN 64

struct dt_entry {
    uint32_t chip;
    uint32_t platform;
    uint32_t subtype;
    uint32_t hw_rev;
    uint32_t hw_rev_end;
    uint32_t offset;
    uint32_t size; /* including padding */
    uint32_t space;
};

struct dtbh_header {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;

    struct dt_entry entries[];
};

static void show_dt_entry(struct dt_entry *ent) {
    printf("chip: %u, platform: 0x%x, subtype: 0x%x, hw_rev: %u, hw_rev_end: %u, offset: %u, "
            "size: %u, space: %u\n", ent->chip, ent->platform, ent->subtype, ent->hw_rev,
            ent->hw_rev_end, ent->offset, ent->size, ent->space);
}

static void dump_dtb(void *buffer, uint32_t size, char *dest)
{
    FILE *output;
    ssize_t len;

    output = fopen(dest, "wb");
    if (output == NULL) {
        fprintf(stderr, "error: could not open %s\n", dest);
        return;
    }

    len = fwrite(buffer, size, 1, output);
    fclose(output);

    if (len < 0) {
        printf("error: failed to write %s (%s)\n", dest, strerror(errno));
        return;
    }

    printf("Successfully dumped %s (size=%zu)\n", dest, size);
}

int usage(void)
{
    fprintf(stderr,"usage: unpackdtbhimg\n"
            "       -i|--input <dtbh image path>\n"
            "       -o|--output <directory>\n"
            "       -p|--pagesize <pagesize>\n"
            );
    return 1;
}

int main(int argc, char *argv[]) {
    char *dtbhimg = 0;
    char *output_dir = 0;
    FILE *dtbhimg_fp;
    ssize_t len;
    int page_size = DEFAULT_PAGE_SIZE;
    struct dtbh_header *header;
    void *dt_blob;

    argc--;
    argv++;

    while (argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        if (argc < 1) {
            return usage();
        }
        argc -= 2;
        argv += 2;
        if (!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            dtbhimg = val;
        } else if (!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            output_dir = val;
        } else if (!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            page_size = atoi(val);
        }
    }

    if (dtbhimg == 0) {
        fprintf(stderr,"error: no input filename specified\n");
        return usage();
    }

    if (output_dir == 0) {
        fprintf(stderr,"error: no output path specified\n");
        return usage();
    }

    printf("using page_size: %d\n", page_size);

    dtbhimg_fp = fopen(dtbhimg, "r");
    if (dtbhimg_fp == NULL) {
        fprintf(stderr, "error: could not open %s\n", dtbhimg);
        goto err_open;
    }

    header = calloc(page_size, 1);
    if (header == NULL) {
        goto err_calloc;
    }

    len = fread(header, page_size, 1, dtbhimg_fp);
    if (len != 1) {
        fprintf(stderr, "Failed to read DTBH header: %s\n", strerror(errno));
        goto err_read;
    }

    printf("DTBH_MAGIC = '%.4s'\n", &header->magic);
    printf("DTBH_VERSION = %u\n", header->version);
    printf("Number of entries: %u\n", header->entry_count);

    for (uint32_t i = 0; i < header->entry_count; i++) {
        printf("DTB %d: ", i);
        show_dt_entry(&header->entries[i]);
        dt_blob = malloc(header->entries[i].size);
        if (fseek(dtbhimg_fp, header->entries[i].offset, SEEK_SET) != 0) {
            fprintf(stderr, "failed to set offset to 0x%x: %s\n", header->entries[i].offset,
                    strerror(errno));
            goto err_loop;
        }
        len = fread(dt_blob, header->entries[i].size, 1, dtbhimg_fp);
        if (len != 1) {
            fprintf(stderr, "Failed to read DTB: %s\n", strerror(errno));
        }

        size_t dest_file_len = DT_NAME_LEN + strlen(output_dir);
        char dest_file[dest_file_len];
        snprintf(dest_file, dest_file_len, "%s/chip%u-0x%x-0x%x_rev%u-%u.dtb", output_dir,
                 header->entries[i].chip, header->entries[i].platform, header->entries[i].subtype,
                 header->entries[i].hw_rev, header->entries[i].hw_rev_end);
        dump_dtb(dt_blob, header->entries[i].size, dest_file);
        free(dt_blob);
    }

    free(header);
    fclose(dtbhimg_fp);

    return 0;

err_loop:
    free(dt_blob);
err_read:
    free(header);
err_calloc:
    fclose(dtbhimg_fp);
err_open:
    return 1;
}
