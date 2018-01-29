/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2014 The CyanogenMod Project
 * Copyright (C) 2014-2015 Andreas Schneider <asn@cryptomilk.org>
 * Copyright (C) 2014-2017 Christopher N. Hesse <raymanfx@gmail.com>
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
#define LOG_TAG "SamsungPowerHelper"
/* #define LOG_NDEBUG 0 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <utils/Log.h>

#include <samsung_power.h>
#include <power/samsung_power_helper.h>

int sysfs_read(char *path, char *s, int num_bytes)
{
    char errno_str[64];
    int len;
    int ret = 0;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error opening %s: %s", path, errno_str);

        return -1;
    }

    len = read(fd, s, num_bytes - 1);
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error reading from %s: %s", path, errno_str);

        ret = -1;
    } else {
        // do not store newlines, but terminate the string instead
        if (s[len-1] == '\n') {
            s[len-1] = '\0';
        } else {
            s[len] = '\0';
        }
    }

    close(fd);

    return ret;
}

void sysfs_write(const char *path, char *s)
{
    char errno_str[64];
    int len;
    int fd;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error opening %s: %s", path, errno_str);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error writing to %s: %s", path, errno_str);
    }

    close(fd);
}

void cpu_sysfs_read(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN])
{
    char path[PATH_MAX];

    for (unsigned int i = 0; i < ARRAY_SIZE(CPU_SYSFS_PATHS); i++) {
        sprintf(path, "%s%s", CPU_SYSFS_PATHS[i], param);
        sysfs_read(path, s[i], PARAM_MAXLEN);
    }
}

void cpu_sysfs_write(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN])
{
    char path[PATH_MAX];

    for (unsigned int i = 0; i < ARRAY_SIZE(CPU_SYSFS_PATHS); i++) {
        sprintf(path, "%s%s", CPU_SYSFS_PATHS[i], param);
        sysfs_write(path, s[i]);
    }
}

void cpu_interactive_read(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN])
{
    char path[PATH_MAX];

    for (unsigned int i = 0; i < ARRAY_SIZE(CPU_INTERACTIVE_PATHS); i++) {
        sprintf(path, "%s%s", CPU_INTERACTIVE_PATHS[i], param);
        sysfs_read(path, s[i], PARAM_MAXLEN);
    }
}

void cpu_interactive_write(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN])
{
    char path[PATH_MAX];

    for (unsigned int i = 0; i < ARRAY_SIZE(CPU_INTERACTIVE_PATHS); i++) {
        sprintf(path, "%s%s", CPU_INTERACTIVE_PATHS[i], param);
        sysfs_write(path, s[i]);
    }
}

void send_boost(int boost_fd, int32_t duration_us)
{
    int len;

    if (boost_fd < 0) {
        return;
    }

    len = write(boost_fd, "1", 1);
    if (len < 0) {
        ALOGE("Error writing to %s%s: %s", CPU_INTERACTIVE_PATHS[0], BOOST_PATH, strerror(errno));
        return;
    }

    usleep(duration_us);
    len = write(boost_fd, "0", 1);
}

void send_boostpulse(int boostpulse_fd)
{
    int len;

    if (boostpulse_fd < 0) {
        return;
    }

    len = write(boostpulse_fd, "1", 1);
    if (len < 0) {
        ALOGE("Error writing to %s%s: %s", CPU_INTERACTIVE_PATHS[0], BOOSTPULSE_PATH,
                strerror(errno));
    }
}

