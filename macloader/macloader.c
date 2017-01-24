/*
 * Copyright (C) 2012, The CyanogenMod Project
 *                     Daniel Hillenbrand <codeworkx@cyanogenmod.com>
 *                     Marco Hillenbrand <marco.hillenbrand@googlemail.com>
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

#ifndef WIFI_DRIVER_NVRAM_PATH
#define WIFI_DRIVER_NVRAM_PATH		NULL
#endif

#ifndef WIFI_DRIVER_NVRAM_PATH_PARAM
#define WIFI_DRIVER_NVRAM_PATH_PARAM "/sys/module/wlan/parameters/nvram_path"
#endif

#define MACADDR_PATH "/efs/wifi/.mac.info"
#define CID_PATH "/data/.cid.info"

enum Type {
    NONE,
    MURATA,
    SEMCOSH,
    SEMCOVE,
    SEMCO3RD,
    SEMCO,
    WISOL
};

static int wifi_change_nvram_calibration(const char *nvram_file,
                                         const char *type)
{
    int len;
    int fd = -1;
    int ret = 0;
    struct stat sb;
    char nvram_str[1024] = { 0 };

    if (nvram_file == NULL || type == NULL) {
        return -1;
    }

    ret = stat(nvram_file, &sb);
    if (ret != 0) {
        ALOGE("Failed to check for NVRAM calibration file '%s' - error: %s",
              nvram_file,
              strerror(errno));
        return -1;
    }

    ALOGD("Using NVRAM calibration file: %s\n", nvram_file);

    fd = TEMP_FAILURE_RETRY(open(WIFI_DRIVER_NVRAM_PATH_PARAM, O_WRONLY));
    if (fd < 0) {
        ALOGE("Failed to open wifi nvram config path %s - error: %s",
              WIFI_DRIVER_NVRAM_PATH_PARAM, strerror(errno));
        return -1;
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

    ret = 0;
out:
    if (fd != -1) {
        close(fd);
    }
    return ret;
}

int main() {
    FILE* file;
    FILE* cidfile;
    char* str;
    char mac_addr_half[9];
    int ret = -1;
    int amode;
    enum Type type = NONE;

    /* open mac addr file */
    file = fopen(MACADDR_PATH, "r");
    if (file == 0) {
        fprintf(stderr, "open(%s) failed\n", MACADDR_PATH);
        ALOGE("Can't open %s\n", MACADDR_PATH);
        return -1;
    }

    /* get and compare mac addr */
    str = fgets(mac_addr_half, 9, file);
    fclose(file);
    if (str == 0) {
        fprintf(stderr, "fgets() from file %s failed\n", MACADDR_PATH);
        ALOGE("Can't read from %s\n", MACADDR_PATH);
        return -1;
    }

    /* murata
       ref: http://hwaddress.com/?q=ACT */
    if (strncasecmp(mac_addr_half, "00:0e:6d", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:13:e0", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:21:e8", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:26:e8", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:37:6d", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:60:57", 9) == 0 ||
        strncasecmp(mac_addr_half, "00:ae:fa", 9) == 0 ||
        strncasecmp(mac_addr_half, "04:46:65", 9) == 0 ||
        strncasecmp(mac_addr_half, "10:5f:06", 9) == 0 ||
        strncasecmp(mac_addr_half, "10:a5:d0", 9) == 0 ||
        strncasecmp(mac_addr_half, "10:d5:42", 9) == 0 ||
        strncasecmp(mac_addr_half, "14:7d:c5", 9) == 0 ||
        strncasecmp(mac_addr_half, "1c:99:4c", 9) == 0 ||
        strncasecmp(mac_addr_half, "20:02:af", 9) == 0 ||
        strncasecmp(mac_addr_half, "40:f3:08", 9) == 0 ||
        strncasecmp(mac_addr_half, "44:a7:cf", 9) == 0 ||
        strncasecmp(mac_addr_half, "5c:da:d4", 9) == 0 ||
        strncasecmp(mac_addr_half, "5c:f8:a1", 9) == 0 ||
        strncasecmp(mac_addr_half, "60:21:c0", 9) == 0 ||
        strncasecmp(mac_addr_half, "60:f1:89", 9) == 0 ||
        strncasecmp(mac_addr_half, "78:4b:87", 9) == 0 ||
        strncasecmp(mac_addr_half, "78:52:1a", 9) == 0 ||
        strncasecmp(mac_addr_half, "88:30:8a", 9) == 0 ||
        strncasecmp(mac_addr_half, "90:b6:86", 9) == 0 ||
        strncasecmp(mac_addr_half, "98:f1:70", 9) == 0 ||
        strncasecmp(mac_addr_half, "a0:cc:2b", 9) == 0 ||
        strncasecmp(mac_addr_half, "a4:08:ea", 9) == 0 ||
        strncasecmp(mac_addr_half, "b0:72:bf", 9) == 0 ||
        strncasecmp(mac_addr_half, "b8:d7:af", 9) == 0 ||
        strncasecmp(mac_addr_half, "c8:14:79", 9) == 0 ||
        strncasecmp(mac_addr_half, "d0:e4:4a", 9) == 0 ||
        strncasecmp(mac_addr_half, "d8:c4:6a", 9) == 0 ||
        strncasecmp(mac_addr_half, "dc:ef:ca", 9) == 0 ||
        strncasecmp(mac_addr_half, "f0:27:65", 9) == 0 ||
        strncasecmp(mac_addr_half, "fc:c2:de", 9) == 0 ||
        strncasecmp(mac_addr_half, "fc:db:b3", 9) == 0) {
        type = MURATA;
    }

    /* semcosh */
    if (strncasecmp(mac_addr_half, "34:23:ba", 9) == 0 ||
        strncasecmp(mac_addr_half, "38:aa:3c", 9) == 0 ||
        strncasecmp(mac_addr_half, "5c:0a:5b", 9) == 0 ||
        strncasecmp(mac_addr_half, "88:32:9b", 9) == 0 ||
        strncasecmp(mac_addr_half, "90:18:7c", 9) == 0 ||
        strncasecmp(mac_addr_half, "cc:3a:61", 9) == 0) {
        type = SEMCOSH;
    }

    /* semco3rd */
    if (strncasecmp(mac_addr_half, "2c:0e:3d", 9) == 0 ||
        strncasecmp(mac_addr_half, "54:88:0e", 9) == 0 ||
        strncasecmp(mac_addr_half, "84:38:38", 9) == 0 ||
        strncasecmp(mac_addr_half, "8c:f5:a3", 9) == 0 ||
        strncasecmp(mac_addr_half, "ac:36:13", 9) == 0 ||
        strncasecmp(mac_addr_half, "ac:5f:3e", 9) == 0 ||
        strncasecmp(mac_addr_half, "b4:79:a7", 9) == 0 ||
        strncasecmp(mac_addr_half, "c0:97:27", 9) == 0 ||
        strncasecmp(mac_addr_half, "c0:bd:d1", 9) == 0 ||
        strncasecmp(mac_addr_half, "c8:ba:94", 9) == 0 ||
        strncasecmp(mac_addr_half, "d0:25:44", 9) == 0 ||
        strncasecmp(mac_addr_half, "e8:50:8b", 9) == 0 ||
        strncasecmp(mac_addr_half, "ec:1f:72", 9) == 0 ||
        strncasecmp(mac_addr_half, "ec:9b:f3", 9) == 0 ||
        strncasecmp(mac_addr_half, "f0:25:b7", 9) == 0 ||
        strncasecmp(mac_addr_half, "f4:09:d8", 9) == 0 ||
        strncasecmp(mac_addr_half, "f8:04:2e", 9) == 0) {
        type = SEMCO3RD;
    }

    /* semco */
    if (strncasecmp(mac_addr_half, "4c:66:41", 9) == 0 ||
        strncasecmp(mac_addr_half, "51:f6:6b", 9) == 0 ||
        strncasecmp(mac_addr_half, "ec:9b:f3", 9) == 0) {
        type = SEMCO;
    }

    /* wisol */
    if (strncasecmp(mac_addr_half, "48:5A:3F", 9) == 0 ||
        strncasecmp(mac_addr_half, "70:2C:1F", 9) == 0) {
        type = WISOL;
    }

    if (type != NONE) {
        const char *nvram_file;
        const char *type_str;
        struct passwd *pwd;
        int fd;

        /* open cid file */
        cidfile = fopen(CID_PATH, "w");
        if(cidfile == 0) {
            fprintf(stderr, "open(%s) failed\n", CID_PATH);
            ALOGE("Can't open %s\n", CID_PATH);
            return -1;
        }

        switch(type) {
            case NONE:
                return -1;
            case MURATA:
                type_str = "murata";
                break;
            case SEMCOSH:
                type_str = "semcosh";
                break;
            case SEMCOVE:
                type_str = "semcove";
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
        }

        ALOGI("Settting wifi type to %s in %s\n", type_str, CID_PATH);

        ret = fputs(type_str, cidfile);
        if (ret != 0) {
            ALOGE("Can't write to %s\n", CID_PATH);
            return 1;
        }

        /* Change permissions of cid file */
        ALOGD("Change permissions of %s\n", CID_PATH);

        fd = fileno(cidfile);
        amode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        ret = fchmod(fd, amode);
        if (ret != 0) {
            fclose(cidfile);
            ALOGE("Can't set permissions on %s - %s\n",
                  CID_PATH, strerror(errno));
            return 1;
        }

        pwd = getpwnam("system");
        if (pwd == NULL) {
            fclose(cidfile);
            ALOGE("Failed to find 'system' user - %s\n",
                  strerror(errno));
            return 1;
        }

        ret = fchown(fd, pwd->pw_uid, pwd->pw_gid);
        fclose(cidfile);
        if (ret != 0) {
            ALOGE("Failed to change owner of %s - %s\n",
                  CID_PATH, strerror(errno));
            return 1;
        }

        nvram_file = WIFI_DRIVER_NVRAM_PATH;
        if (nvram_file != NULL) {
            ret = wifi_change_nvram_calibration(nvram_file, type_str);
            if (ret != 0) {
                return 1;
            }
        }
    } else {
        /* delete cid file if no specific type */
        ALOGD("Deleting file %s\n", CID_PATH);
        remove(CID_PATH);
    }
    return 0;
}
