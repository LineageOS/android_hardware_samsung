/* tools/mkbootimg/unpackdtbimg.c
**
** Copyright 2018, The LineageOS Project
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
** January 2018, Christopher N. Hesse
** Initial version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

const char DTB_HEADER[] = "\xd0\x0d\xfe\xed";
#define DTBH_HEADER_LEN sizeof(DTB_HEADER)/sizeof(DTB_HEADER[0])

int usage(void)
{
    fprintf(stderr,"usage: unpackdtimg\n"
            "       -i|--input <dt image path>\n"
            "       -o|--output <directory>\n"
            );
    return 1;
}

static void dump_dts(char *buffer, char *dest, unsigned long start, unsigned long end)
{
    FILE *output;
    char output_path[2048];

    snprintf(output_path, 2047, "%s/%lu_to_%lu.dts", dest, start, end);
    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "error: could not open %s\n", output_path);
        return;
    }

    fwrite(buffer+start, end-start, 1, output);
    fclose(output);
    printf("Successfully dumped %s\n", output_path);
}

int main(int argc, char **argv)
{
    char *dtimg = 0;
    char *output_dir = 0;
    FILE *dtimg_fp = NULL;
    unsigned long dtimg_len;
    unsigned long start = 0, end = 0;
    char *dtimg_buffer;

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
            dtimg = val;
        } else if (!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            output_dir = val;
        }
    }

    if (dtimg == 0) {
        fprintf(stderr,"error: no input filename specified\n");
        return usage();
    }

    if (output_dir == 0) {
        fprintf(stderr,"error: no output path specified\n");
        return usage();
    }

    dtimg_fp = fopen(dtimg, "rb");
    if (dtimg_fp == NULL) {
        fprintf(stderr, "error: could not open %s\n", dtimg);
    }

    fseek(dtimg_fp, 0, SEEK_END);
    dtimg_len = ftell(dtimg_fp);
    fseek(dtimg_fp, 0, SEEK_SET);

    dtimg_buffer = (char*) malloc(dtimg_len + 1);
    if (dtimg_buffer == NULL) {
        fprintf(stderr, "error: could not allocate %lu bytes", dtimg_len + 1);
        fclose(dtimg_fp);
        return -ENOMEM;
    }

    fread(dtimg_buffer, dtimg_len, 1, dtimg_fp);

    for (unsigned long i = 0; i < dtimg_len; i++) {
        if (memcmp(dtimg_buffer+i, DTB_HEADER, DTBH_HEADER_LEN) == 0) {
            printf("Found DT start at offset: %lu\n", i);
            if (start) {
                end = i;
            } else {
                start = i;
            }
        }

        if (start && (i == dtimg_len - 1)) {
            end = i;
        }

        if (start && end) {
            dump_dts(dtimg_buffer, output_dir, start, end);
            start = i;
            end = 0;
        }
    }

    free(dtimg_buffer);
    fclose(dtimg_fp);
}
