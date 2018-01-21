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
#include <cutils/properties.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#define DEFERRED_INITCALLS        "/proc/deferred_initcalls"

#ifndef WIFI_DRIVER_MODULE_NAME
#define WIFI_DRIVER_MODULE_NAME   "wlan"
#endif

#ifndef WIFI_DRIVER_MODULE_PATH
#define WIFI_DRIVER_MODULE_PATH   "/system/lib/modules/" WIFI_DRIVER_MODULE_NAME ".ko"
#endif

#define finit_module(fd, params, flags) syscall(__NR_finit_module, fd, params, flags)


static int check_module_loaded(char const *tag)
{
    FILE *proc;
    char line[126];

    if ((proc = fopen("/proc/modules", "r")) == NULL) {
        ALOGW("Could not open %s: %s", "/proc/modules", strerror(errno));
        return -errno;
    }

    while ((fgets(line, sizeof(line), proc)) != NULL) {
        if (strncmp(line, tag, strlen(tag)) == 0) {
            fclose(proc);
            return 1;
        }
    }

    fclose(proc);
    return 0;
}

static int load_module(char const *path)
{
    int fd;

    if (check_module_loaded(WIFI_DRIVER_MODULE_NAME) > 0) {
        ALOGE("Driver: %s already loaded", path);
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        ALOGE("Failed to open %s - error: %s", path, strerror(errno));
        return -errno;
    }

    // load the .ko image
    if (finit_module(fd, "", 0) != 0) {
        ALOGE("Failed to load module %s - error: %s", path, strerror(errno));
        close(fd);
        return -errno;
    }

    // let wifi HAL know we succeeded
    ALOGV("Successfully loaded WLAN module: %s", WIFI_DRIVER_MODULE_NAME);
    property_set("wlan.driver.status", "ok");

    close(fd);
    return 0;
}

int main(void)
{
    char buf[8] = { '\0' };
    FILE *fp;
    size_t r;
    int fd;
    struct stat st;

    if (stat(WIFI_DRIVER_MODULE_PATH, &st) == 0) {
        ALOGD("Loading WiFi kernel module: %s", WIFI_DRIVER_MODULE_PATH);
        load_module(WIFI_DRIVER_MODULE_PATH);
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
