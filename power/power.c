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

#include "samsung_power.h"

struct samsung_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
    char cpu0_hispeed_freq[10];
    char cpu0_max_freq[10];
    char cpu4_hispeed_freq[10];
    char cpu4_max_freq[10];
    char* touchscreen_power_path;
    char* touchkey_power_path;
    bool touchkey_blocked;
};

enum power_profile_e {
    PROFILE_POWER_SAVE = 0,
    PROFILE_BALANCED,
    PROFILE_HIGH_PERFORMANCE
};
static enum power_profile_e current_power_profile = PROFILE_BALANCED;

/**********************************************************
 *** HELPER FUNCTIONS
 **********************************************************/

static int sysfs_read(char *path, char *s, int num_bytes)
{
    char errno_str[64];
    int len;
    int ret = 0;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error opening %s: %s\n", path, errno_str);

        return -1;
    }

    len = read(fd, s, num_bytes - 1);
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error reading from %s: %s\n", path, errno_str);

        ret = -1;
    } else {
        s[len] = '\0';
    }

    close(fd);

    return ret;
}

static void sysfs_write(const char *path, char *s)
{
    char errno_str[64];
    int len;
    int fd;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error opening %s: %s\n", path, errno_str);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, errno_str, sizeof(errno_str));
        ALOGE("Error writing to %s: %s\n", path, errno_str);
    }

    close(fd);
}

/**********************************************************
 *** POWER FUNCTIONS
 **********************************************************/

/* You need to request the powerhal lock before calling this function */
static int boostpulse_open(struct samsung_power_module *samsung_pwr)
{
    char errno_str[64];

    if (samsung_pwr->boostpulse_fd < 0) {
        samsung_pwr->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);
        if (samsung_pwr->boostpulse_fd < 0) {
            if (!samsung_pwr->boostpulse_warned) {
                strerror_r(errno, errno_str, sizeof(errno_str));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, errno_str);
                samsung_pwr->boostpulse_warned = 1;
            }
        }
    }

    return samsung_pwr->boostpulse_fd;
}

static void set_power_profile(struct samsung_power_module *samsung_pwr,
                              enum power_profile_e profile)
{
    int rc;
    struct stat sb;

    if (current_power_profile == profile) {
        return;
    }

    ALOGV("%s: profile=%d", __func__, profile);

    switch (profile) {
        case PROFILE_POWER_SAVE:
            // Limit to hispeed freq
            sysfs_write(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_hispeed_freq);
            rc = stat(CPU4_MAX_FREQ_PATH, &sb);
            if (rc == 0) {
                sysfs_write(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_hispeed_freq);
            }
            ALOGV("%s: set powersave mode", __func__);
            break;
        case PROFILE_BALANCED:
            // Restore normal max freq
            sysfs_write(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq);
            rc = stat(CPU4_MAX_FREQ_PATH, &sb);
            if (rc == 0) {
                sysfs_write(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq);
            }
            ALOGV("%s: set balanced mode", __func__);
            break;
        case PROFILE_HIGH_PERFORMANCE:
            // Restore normal max freq
            sysfs_write(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq);
            rc = stat(CPU4_MAX_FREQ_PATH, &sb);
            if (rc == 0) {
                sysfs_write(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq);
            }
            ALOGV("%s: set performance mode", __func__);
            break;
    }

    current_power_profile = profile;
}

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
                ALOGE("Out of memory: %s\n", errno_str);
                return;
            }

            snprintf(path, pathsize, "%s/%s", dir, filename);
            sysfs_read(path, file_content, sizeof(file_content));

            snprintf(node_path, node_pathsize, "%s/%s", dir, "enabled");

            if (strncmp(file_content, "sec_touchkey", 12) == 0) {
                ALOGV("%s: found touchkey path: %s\n", __func__, node_path);
                samsung_pwr->touchkey_power_path = malloc(node_pathsize);
                if (samsung_pwr->touchkey_power_path == NULL) {
                    strerror_r(errno, errno_str, sizeof(errno_str));
                    ALOGE("Out of memory: %s\n", errno_str);
                    return;
                }
                snprintf(samsung_pwr->touchkey_power_path, node_pathsize,
                         "%s", node_path);
            }

            if (strncmp(file_content, "sec_touchscreen", 15) == 0) {
                ALOGV("%s: found touchscreen path: %s\n", __func__, node_path);
                samsung_pwr->touchscreen_power_path = malloc(node_pathsize);
                if (samsung_pwr->touchscreen_power_path == NULL) {
                    strerror_r(errno, errno_str, sizeof(errno_str));
                    ALOGE("Out of memory: %s\n", errno_str);
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

/**********************************************************
 *** INIT FUNCTIONS
 **********************************************************/

static void init_cpufreqs(struct samsung_power_module *samsung_pwr)
{
    int rc;
    struct stat sb;

    sysfs_read(CPU0_HISPEED_FREQ_PATH, samsung_pwr->cpu0_hispeed_freq,
               sizeof(samsung_pwr->cpu0_hispeed_freq));
    sysfs_read(CPU0_MAX_FREQ_PATH, samsung_pwr->cpu0_max_freq,
               sizeof(samsung_pwr->cpu0_max_freq));
    ALOGV("%s: CPU 0 hispeed freq: %s\n", __func__, samsung_pwr->cpu0_hispeed_freq);
    ALOGV("%s: CPU 0 max freq: %s\n", __func__, samsung_pwr->cpu0_max_freq);

    rc = stat(CPU4_HISPEED_FREQ_PATH, &sb);
    if (rc == 0) {
        sysfs_read(CPU4_HISPEED_FREQ_PATH, samsung_pwr->cpu4_hispeed_freq,
                   sizeof(samsung_pwr->cpu4_hispeed_freq));
        sysfs_read(CPU4_MAX_FREQ_PATH, samsung_pwr->cpu4_max_freq,
                   sizeof(samsung_pwr->cpu4_max_freq));
        ALOGV("%s: CPU 4 hispeed freq: %s\n", __func__, samsung_pwr->cpu4_hispeed_freq);
        ALOGV("%s: CPU 4 max freq: %s\n", __func__, samsung_pwr->cpu4_max_freq);
    }
}

static void init_touch_input_power_path(struct samsung_power_module *samsung_pwr)
{
    char dir[1024];
    char errno_str[64];
    uint32_t i;

    for (i = 0; i < 20; i++) {
        snprintf(dir, sizeof(dir), "/sys/class/input/input%d", i);
        find_input_nodes(samsung_pwr, dir);
    }
}

static void samsung_power_init(struct power_module *module)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;

    init_cpufreqs(samsung_pwr);
    init_touch_input_power_path(samsung_pwr);
}

/**********************************************************
 *** API FUNCTIONS
 ***
 *** Refer to power.h for documentation.
 **********************************************************/

static void samsung_power_set_interactive(struct power_module *module, int on)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;
    struct stat sb;
    char touchkey_node[2];
    int rc;

    ALOGV("power_set_interactive: %d\n", on);

    // Get panel backlight brightness from lights HAL
    // Do not disable any input devices if the screen is on but we are in a non-interactive state
    if (!on) {
        if (get_cur_panel_brightness() > 0) {
            ALOGV("%s: Moving to non-interactive state, but screen is still on,"
                  " not disabling input devices\n", __func__);
            goto out;
        }
    }

    sysfs_write(samsung_pwr->touchscreen_power_path, on ? "1" : "0");

    rc = stat(samsung_pwr->touchkey_power_path, &sb);
    if (rc < 0) {
        goto out;
    }

    if (!on) {
        if (sysfs_read(samsung_pwr->touchkey_power_path, touchkey_node,
                       sizeof(touchkey_node)) == 0) {
            /*
             * If touchkey_node is 0, the keys have been disabled by another component
             * (for example cmhw), which means we don't want them to be enabled when resuming
             * from suspend.
             */
            if (touchkey_node[0] == '0') {
                samsung_pwr->touchkey_blocked = true;
            } else {
                samsung_pwr->touchkey_blocked = false;
                sysfs_write(samsung_pwr->touchkey_power_path, "0");
            }
        }
    } else if (!samsung_pwr->touchkey_blocked) {
        sysfs_write(samsung_pwr->touchkey_power_path, "1");
    }

out:
    sysfs_write(IO_IS_BUSY_PATH, on ? "1" : "0");
    ALOGV("power_set_interactive: %d done\n", on);
}

static void samsung_power_hint(struct power_module *module,
                                  power_hint_t hint,
                                  void *data)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;
    char errno_str[64];
    int len;

    switch (hint) {
        case POWER_HINT_INTERACTION: {
            if (current_power_profile == PROFILE_POWER_SAVE) {
                return;
            }

            ALOGV("%s: POWER_HINT_INTERACTION", __func__);

            if (boostpulse_open(samsung_pwr) >= 0) {
                len = write(samsung_pwr->boostpulse_fd, "1", 1);

                if (len < 0) {
                    strerror_r(errno, errno_str, sizeof(errno_str));
                    ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, errno_str);
                }
            }

            break;
        }
        case POWER_HINT_VSYNC: {
            ALOGV("%s: POWER_HINT_VSYNC", __func__);
            break;
        }
        case POWER_HINT_SET_PROFILE: {
            int profile = *((intptr_t *)data);

            ALOGV("%s: POWER_HINT_SET_PROFILE", __func__);

            set_power_profile(samsung_pwr, profile);
            break;
        }
        default:
            break;
    }
}

static int samsung_get_feature(struct power_module *module __unused,
                               feature_t feature)
{
    if (feature == POWER_FEATURE_SUPPORTED_PROFILES) {
        return 3;
    }

    return -1;
}

static void samsung_set_feature(struct power_module *module, feature_t feature, int state __unused)
{
    struct samsung_power_module *samsung_pwr = (struct samsung_power_module *) module;

    switch (feature) {
#ifdef TARGET_TAP_TO_WAKE_NODE
        case POWER_FEATURE_DOUBLE_TAP_TO_WAKE:
            ALOGV("%s: %s double tap to wake", __func__, state ? "enabling" : "disabling");
            sysfs_write(TARGET_TAP_TO_WAKE_NODE, state > 0 ? "1" : "0");
            break;
#endif
        default:
            break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct samsung_power_module HAL_MODULE_INFO_SYM = {
    .base = {
        .common = {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = POWER_MODULE_API_VERSION_0_2,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = POWER_HARDWARE_MODULE_ID,
            .name = "Samsung Power HAL",
            .author = "The LineageOS Project",
            .methods = &power_module_methods,
        },

        .init = samsung_power_init,
        .setInteractive = samsung_power_set_interactive,
        .powerHint = samsung_power_hint,
        .getFeature = samsung_get_feature,
        .setFeature = samsung_set_feature
    },

    .lock = PTHREAD_MUTEX_INITIALIZER,
    .boostpulse_fd = -1,
    .boostpulse_warned = 0,
};
