/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#define LOG_TAG "SamsungLightsHelper"
/* #define LOG_NDEBUG 0 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <liblights/samsung_lights_helper.h>

/*
 * Reads an Integer from a file.
 *
 * @param path The absolute path string.
 * @return The Integer with decimal base, -1 or -errno on error.
 */
static int read_int(char const *path)
{
    int fd, len;
    int ret = 0;
    const int INT_MAX_STRLEN = 10;
    char buffer[INT_MAX_STRLEN+1];

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ret = -errno;
        ALOGE("%s: failed to open %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

    len = read(fd, buffer, INT_MAX_STRLEN-1);
    if (len < 0) {
        ret = -errno;
        ALOGE("%s: failed to read from %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

    buffer[len] = '\0';

    /* now parse the integer from buffer */
    char *endptr = NULL;
    ret = (int) strtol(buffer, &endptr, 10);
    if (ret == 0 && endptr == NULL) {
        ret = -1;
        ALOGE("%s: failed to extract number from string %s", __func__, buffer);
        goto exit;
    }

exit:
    if (fd >= 0)
        close(fd);
    return ret;
}

/*
 * Writes an Integer to a file.
 *
 * @param path The absolute path string.
 * @param value The Integer value to be written.
 * @return 0 on success, -errno on error.
 */
static int write_int(char const *path, const int value)
{
    int fd, len, num_bytes;
    int ret = 0;
    const int INT_MAX_STRLEN = 10;
    char buffer[INT_MAX_STRLEN+1];

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        ret = -errno;
        ALOGE("%s: failed to open %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

    num_bytes = sprintf(buffer, "%d", value);
    len = write(fd, buffer, num_bytes);
    if (len < 0) {
        ret = -errno;
        ALOGE("%s: failed to write to %s (%s)", __func__, path, strerror(errno));
        goto exit;
    }

exit:
    if (fd >= 0)
        close(fd);
    return ret;
}

/*
 * Set the current button brightness via sysfs.
 *
 * @param brightness The brightness value.
 * @return 0 on success, errno on error.
 */
inline int set_cur_button_brightness(const int brightness)
{
    return write_int(BUTTON_BRIGHTNESS_NODE, brightness);
}

/*
 * Read the current panel brightness from sysfs.
 *
 * @return The brightness as Integer, -1 on error.
 */
inline int get_cur_panel_brightness()
{
    return read_int(PANEL_BRIGHTNESS_NODE);
}

/*
 * Read the maximum panel brightness from sysfs.
 *
 * @return The brightness as Integer, -1 on error.
 */
inline int get_max_panel_brightness()
{
    return read_int(PANEL_MAX_BRIGHTNESS_NODE);
}

/*
 * Set the current panel brightness via sysfs.
 *
 * @param brightness The (scaled) brightness value.
 * @return 0 on success, errno on error.
 */
inline int set_cur_panel_brightness(const int brightness)
{
    return write_int(PANEL_BRIGHTNESS_NODE, brightness);
}

/*
 * Set the maximum panel brightness via sysfs.
 *
 * @param brightness The brightness value.
 * @return 0 on success, errno on error.
 */
inline int set_max_panel_brightness(const int brightness)
{
    return write_int(PANEL_MAX_BRIGHTNESS_NODE, brightness);
}
