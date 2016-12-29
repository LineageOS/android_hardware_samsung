/* tools/mkbootimg/mkdtbimg.c
**
** Copyright 2016-2017, The LineageOS Project
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
** December 2016, Christopher N. Hesse
** Separate dt creation from mkbootimg program.
**
** January 2017, Christopher N. Hesse
** Adjust to dtbToolCM call syntax.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <stdint.h>

#include <dtbimg.h>

int usage(void)
{
    fprintf(stderr,"usage: mkdtimg\n"
            "       --dt_dir <dtb path>\n"
            "       -o|--output <filename>\n"
            );
    return 1;
}



int main(int argc, char **argv)
{
    char *dtimg = 0;
    char *dt_dir = 0;
    void *dt_data = 0;
    unsigned pagesize = 0;
    unsigned default_pagesize = 2048;
    uint32_t dt_size;
    int fd;
    struct stat sbuf;

    argc--;
    argv++;

    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        if(argc < 1) {
            return usage();
        }
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            dtimg = val;
        } else if (!strcmp(arg, "--dt_dir")) {
            dt_dir = val;
        } else if (!strcmp(arg, "-p")) {
            // Ignore this parameter
        } else if (!strcmp(arg, "-s")) {
            pagesize = strtol(val, NULL, 10);
            if (pagesize == 0)
                pagesize = default_pagesize;
        } else {
            // Check if this is the dtb path
            int err = stat(arg, &sbuf);
            if (err != 0 || !S_ISDIR(sbuf.st_mode))
                return usage();
            dt_dir = arg;
        }
    }

    if(dtimg == 0) {
        fprintf(stderr,"error: no output filename specified\n");
        return usage();
    }

    if(dt_dir == 0) {
        fprintf(stderr,"error: no dtb path specified\n");
        return usage();
    }

    dt_data = load_dtbh_block(dt_dir, pagesize, &dt_size);
    if (dt_data == 0) {
        fprintf(stderr, "error: could not load device tree blobs '%s'\n", dt_dir);
        return 1;
    }

    fd = open(dtimg, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if(fd < 0) {
        fprintf(stderr,"error: could not create '%s'\n", dtimg);
        return 1;
    }

    if(write(fd, dt_data, dt_size) != dt_size) goto fail;

    return 0;

fail:
    unlink(dtimg);
    close(fd);
    fprintf(stderr,"error: failed writing '%s': %s\n", dtimg,
            strerror(errno));
    return 1;
}
