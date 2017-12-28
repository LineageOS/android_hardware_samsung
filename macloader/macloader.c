/*
 * Copyright (C) 2012, The CyanogenMod Project
 *                     Daniel Hillenbrand <codeworkx@cyanogenmod.com>
 *                     Marco Hillenbrand <marco.hillenbrand@googlemail.com>
 * Copyright (C) 2017, The LineageOS Project
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

#define LOG_TAG "macloader"
#define LOG_NDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>

#include <cutils/log.h>

#include <samsung_macloader.h>

#include "macaddr_mappings.h"

static int wifi_change_nvram_calibration(const char *nvram_file,
                                         const char *type)
{
    int len;
    int fd = -1;
    int ret = 0;
    struct stat sb;
    char nvram_str[1024] = { 0 };

    if (nvram_file == NULL || type == NULL) {
        ret = -1;
        goto out;
    }

    ret = stat(nvram_file, &sb);
    if (ret != 0) {
        ALOGE("Failed to check for NVRAM calibration file '%s' - error: %s",
              nvram_file,
              strerror(errno));
        ret = -1;
        goto out;
    }

    ALOGD("Using NVRAM calibration file: %s\n", nvram_file);

    fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_NVRAM_PATH_PARAM, O_WRONLY));
    if (fd < 0) {
        ALOGE("Failed to open wifi nvram config path %s - error: %s",
              WIFI_DRIVER_NVRAM_PATH_PARAM, strerror(errno));
        ret = -1;
        goto out;
    }

    len = strlen(nvram_file) + 1;
    if (TEMP_FAILURE_RETRY(write(fd, nvram_file, len)) != len) {
        ALOGE("Failed to write to wifi config path %s - error: %s",
              WIFI_DRIVER_NVRAM_PATH_PARAM, strerror(errno));
        ret = -1;
        goto out;
    }

    snprintf(nvram_str, sizeof(nvram_str), "%s_%s",
             nvram_file, type);

    ALOGD("Changing NVRAM calibration file for %s chipset\n", type);

    ret = stat(nvram_str, &sb);
    if (ret != 0) {
        ALOGW("NVRAM calibration file '%s' doesn't exist", nvram_str);
        /*
         * We were able to write the default calibration file. So
         * continue here without returning an error.
         */
        ret = 0;
        goto out;
    }

    len = strlen(nvram_str) + 1;
    if (TEMP_FAILURE_RETRY(write(fd, nvram_str, len)) != len) {
        ALOGW("Failed to write to wifi config path %s - error: %s",
              WIFI_DRIVER_NVRAM_PATH_PARAM, strerror(errno));
        /*
         * We were able to write the default calibration file. So
         * continue here without returning an error.
         */
        ret = 0;
        goto out;
    }

    ALOGD("NVRAM calibration file set to '%s'\n", nvram_str);

out:
    if (fd != -1) {
        close(fd);
    }
    return ret;
}

static int classify_macaddr_half(char const *macaddr_half)
{
    int type = NONE;
    unsigned int i, j;
    char const *macaddr;

    for (i = 0; i < TYPE_MAX; i++) {
        for (j = 0; j < MAX_RANGE_ENTRIES; j++) {
            macaddr = all_ranges[i]->macaddrs[j];
            if (macaddr[0] == '\0') {
                break;
            }
            // macaddr_half is guaranteed to be null terminated
            if (strcasecmp(macaddr_half, macaddr) == 0) {
                type = all_ranges[i]->type;
                goto exit;
            }
        }
    }

exit:
    if (type != NONE) {
        ALOGV("Found CID type: %d", type);
    }
    return type;
}

int main() {
    FILE* file = NULL;
    FILE* cidfile = NULL;
    char* str;
    char mac_addr_half[RANGE_ENTRY_LEN + 1] = {0};
    int ret = 0;
    int amode;
    enum Type type = NONE;

    /* open mac addr file */
    file = fopen(MACADDR_PATH, "r");
    if (file == 0) {
        fprintf(stderr, "open(%s) failed\n", MACADDR_PATH);
        ALOGE("Can't open %s\n", MACADDR_PATH);
        ret = -1;
        goto out;
    }

    /* get and compare mac addr */
    str = fgets(mac_addr_half, RANGE_ENTRY_LEN, file);
    fclose(file);
    if (str == 0) {
        fprintf(stderr, "fgets() from file %s failed\n", MACADDR_PATH);
        ALOGE("Can't read from %s\n", MACADDR_PATH);
        ret = -1;
        goto out;
    }

    type = classify_macaddr_half(mac_addr_half);
    if (type == NONE) {
        /* delete cid file if no specific type */
        ALOGD("Deleting file %s\n", CID_PATH);
        remove(CID_PATH);
        ret = 0;
        goto out;
    }

    const char *nvram_file;
    const char *type_str;
    struct passwd *pwd;
    int fd;

    switch(type) {
        case MURATA:
            type_str = "murata";
            break;
        case SEMCOSH:
            type_str = "semcosh";
            break;
        case SEMCO3RD:
            type_str = "semco3rd";
            break;
        case SEMCO:
            type_str = "semco";
            break;
        case WISOL:
            type_str = "wisol";
            break;
        default:
            ALOGE("Unknown CID type: %d", type);
            ret = -1;
            goto out;
    }

    ALOGI("Settting wifi type to %s in %s\n", type_str, CID_PATH);

    /* open cid file */
    cidfile = fopen(CID_PATH, "w");
    if (cidfile == NULL) {
        fprintf(stderr,
                "open(%s) failed: %s\n",
                CID_PATH,
                strerror(errno));
        ALOGE("Can't open %s: %s\n", CID_PATH, strerror(errno));
        ret = -1;
        goto out;
    }

    ret = fputs(type_str, cidfile);
    if (ret != 0) {
        ALOGE("Can't write to %s\n", CID_PATH);
        ret = -1;
        goto out;
    }

    /* Change permissions of cid file */
    ALOGD("Change permissions of %s\n", CID_PATH);

    fd = fileno(cidfile);
    amode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    ret = fchmod(fd, amode);
    if (ret != 0) {
        ALOGE("Can't set permissions on %s - %s\n",
              CID_PATH, strerror(errno));
        ret = -1;
        goto out;
    }

    pwd = getpwnam("system");
    if (pwd == NULL) {
        ALOGE("Failed to find 'system' user - %s\n",
              strerror(errno));
        ret = -1;
        goto out;
    }

    ret = fchown(fd, pwd->pw_uid, pwd->pw_gid);
    if (ret != 0) {
        ALOGE("Failed to change owner of %s - %s\n",
              CID_PATH, strerror(errno));
        ret = -1;
        goto out;
    }

    nvram_file = WIFI_DRIVER_NVRAM_PATH;
    if (nvram_file != NULL) {
        ret = wifi_change_nvram_calibration(nvram_file, type_str);
        if (ret != 0) {
            ret = -1;
            goto out;
        }
    }

out:
    if (file) {
        fclose(file);
    }
    if (cidfile) {
        fclose(cidfile);
    }
    if (ret < 0) {
        ALOGE("Macloader error return code: %d", ret);
    }

    return ret;
}
