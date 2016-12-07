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
 * @return The Integer with decimal base, -1 on error.
 */
int read_int(char const *path)
{
    int fd, len;
    int num_bytes = 10;
    char buf[11];
    int retval;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s: failed to open %s\n", __func__, path);
        goto fail;
    }

    len = read(fd, buf, num_bytes - 1);
    if (len < 0) {
        ALOGE("%s: failed to read from %s\n", __func__, path);
        goto fail;
    }

    buf[len] = '\0';
    close(fd);

    // no endptr, decimal base
    retval = strtol(buf, NULL, 10);
    return retval == 0 ? -1 : retval;

fail:
    if (fd >= 0)
        close(fd);
    return -1;
}

/*
 * Writes an Integer to a file.
 *
 * @param path The absolute path string.
 * @param value The Integer value to be written.
 * @return 0 on success, -1 or errno on error.
 */
int write_int(char const *path, const int value)
{
    int fd;
    static int already_warned;

    already_warned = 0;

    ALOGV("write_int: path %s, value %d", path, value);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

/*
 * Set the current button brightness via sysfs.
 *
 * @param brightness The brightness value.
 * @return 0 on success, -1 or errno on error.
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
 * @return 0 on success, -1 or errno on error.
 */
inline int set_cur_panel_brightness(const int brightness)
{
    return write_int(PANEL_BRIGHTNESS_NODE, brightness);
}

/*
 * Set the maximum panel brightness via sysfs.
 *
 * @param brightness The brightness value.
 * @return 0 on success, -1 or errno on error.
 */
inline int set_max_panel_brightness(const int brightness)
{
    return write_int(PANEL_MAX_BRIGHTNESS_NODE, brightness);
}
