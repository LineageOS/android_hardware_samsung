/*
 * Copyright (c) 2015      Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2017      Christopher N. Hesse <raymanfx@gmail.com>
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

#define LOG_TAG "wifiloader"
#define LOG_NDEBUG 0

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define DEFERRED_INITCALLS "/proc/deferred_initcalls"
#define WIFI_MODULE "/system/lib/modules/wlan.ko"

#define finit_module(fd, params, flags) syscall(__NR_finit_module, fd, params, flags)

int main(void)
{
    char buf[8] = { '\0' };
    FILE *fp;
    size_t r;
    int fd;
    struct stat st;

    if (stat(WIFI_MODULE, &st) == 0) {
        ALOGD("Loading WiFi kernel module: %s", WIFI_MODULE);
        fd = open(WIFI_MODULE, O_RDONLY);
        if (fd == -1) {
            ALOGE("Failed to open %s - error: %s",
                WIFI_MODULE,
                strerror(errno));
            return -errno;
        }

        // load the .ko image
        if (finit_module(fd, "", 0) != 0) {
            ALOGE("Failed to load module %s", WIFI_MODULE);
            close(fd);
            return -1;
        }
        close(fd);
    }

    ALOGD("Trigger initcall of deferred modules\n");

    fp = fopen(DEFERRED_INITCALLS, "r");
    if (fp == NULL) {
        ALOGE("Failed to open %s - error: %s\n",
              DEFERRED_INITCALLS,
              strerror(errno));
        return -errno;
    }

    r = fread(buf, sizeof(buf), 1, fp);
    fclose(fp);

    ALOGV("%s=%s\n", DEFERRED_INITCALLS, buf);

    return 0;
}
