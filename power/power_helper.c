/*
 * Copyright (C) 2014 The LineageOS Project
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#define LOG_TAG "SamsungPowerHAL"
/* #define LOG_NDEBUG 0 */
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>
#include <liblights/samsung_lights_helper.h>

struct samsung_power_module {
    /* only the data we need, the rest is stripped */
    char* touchscreen_power_path;
    char* touchkey_power_path;
};

static void find_input_nodes(struct samsung_power_module *samsung_pwr, char *dir)
{
    const char filename[] = "name";
    char errno_str[64];
    struct dirent *de;
    char file_content[20];
    char *path = NULL;
    char *node_path = NULL;
    size_t pathsize;
    size_t node_pathsize;
    DIR *d;

    d = opendir(dir);
    if (d == NULL) {
        return;
    }

    while ((de = readdir(d)) != NULL) {
        if (strncmp(filename, de->d_name, sizeof(filename)) == 0) {
            pathsize = strlen(dir) + strlen(de->d_name) + 2;
            node_pathsize = strlen(dir) + strlen("enabled") + 2;

            path = malloc(pathsize);
            node_path = malloc(node_pathsize);
            if (path == NULL || node_path == NULL) {
                strerror_r(errno, errno_str, sizeof(errno_str));
                ALOGE("Out of memory: %s", errno_str);
                return;
            }

            snprintf(path, pathsize, "%s/%s", dir, filename);
            sysfs_read(path, file_content, sizeof(file_content));

            snprintf(node_path, node_pathsize, "%s/%s", dir, "enabled");

            if (strncmp(file_content, "sec_touchkey", 12) == 0) {
                ALOGV("%s: found touchkey path: %s", __func__, node_path);
                samsung_pwr->touchkey_power_path = malloc(node_pathsize);
                if (samsung_pwr->touchkey_power_path == NULL) {
                    strerror_r(errno, errno_str, sizeof(errno_str));
                    ALOGE("Out of memory: %s", errno_str);
                    return;
                }
                snprintf(samsung_pwr->touchkey_power_path, node_pathsize,
                         "%s", node_path);
            }

            if (strncmp(file_content, "sec_touchscreen", 15) == 0) {
                ALOGV("%s: found touchscreen path: %s", __func__, node_path);
                samsung_pwr->touchscreen_power_path = malloc(node_pathsize);
                if (samsung_pwr->touchscreen_power_path == NULL) {
                    strerror_r(errno, errno_str, sizeof(errno_str));
                    ALOGE("Out of memory: %s", errno_str);
                    return;
                }
                snprintf(samsung_pwr->touchscreen_power_path, node_pathsize,
                         "%s", node_path);
            }
        }
    }

    if (path)
        free(path);
    if (node_path)
        free(node_path);
    closedir(d);
}

static int touchscreen_power(bool enable)
{
    char tscreen_path[100];
    char tkey_path[100];

    samsung_power_module container = {
        .touchscreen_power_path = tscreen_path;
        .touchkey_power_path = tkey_path;
    }
}
