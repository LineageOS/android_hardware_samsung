/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2015-2017 Christopher N. Hesse <raymanfx@gmail.com>
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

#define LOG_TAG "modemloader"
#define LOG_NDEBUG 0

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cutils/properties.h>
#include <cutils/log.h>

void parse_hardware_revision(unsigned int *revision)
{
    const char *cpuinfo = "/proc/cpuinfo";
    char *data = NULL;
    size_t len = 0, limit = 1024;
    int fd, n;
    char *x, *rev;

    fd = open(cpuinfo, O_RDONLY);
    if (fd < 0) return;

    for (;;) {
        x = (char*)realloc(data, limit);
        if (!x) {
            ALOGE("Failed to allocate memory to read %s\n", cpuinfo);
            goto done;
        }
        data = x;

        n = read(fd, data + len, limit - len);
        if (n < 0) {
            ALOGE("Failed reading %s: %s (%d)\n", cpuinfo, strerror(errno), errno);
            goto done;
        }
        len += n;

        if (len < limit)
            break;

        /* We filled the buffer, so increase size and loop to read more */
        limit *= 2;
    }

    data[len] = 0;
    rev = strstr(data, "\nRevision");

    if (rev) {
        x = strstr(rev, ": ");
        if (x) {
            *revision = strtoul(x + 2, 0, 16);
        }
    }

done:
    close(fd);
    free(data);
}

int main(void)
{
    unsigned int revision;
    char hardware_revision[PROP_VALUE_MAX];
    const char *properties[] = {"hw.revision", "ro.cbd.dt_revision", "ril.cbd.dt_revision"};

    // try to read the revision from the kernel cmdline
    revision = property_get_int32("ro.boot.hw_rev", 0);

    // if the property was not exported, try to parse /proc/cpuinfo
    if (revision == 0) {
        parse_hardware_revision(&revision);
    }

    snprintf(hardware_revision, PROP_VALUE_MAX, "%d", revision);

    // set all the properties
    const int array_length = sizeof(properties) / sizeof(properties[0]);
    for (int i = 0; i < array_length; i++) {
        property_set(properties[i], hardware_revision);
    }

    // indicate that we are done
    property_set("ro.modemloader.done", "1");

    return 0;
}
