/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright (C) 2011 The CyanogenMod Project
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


#define LOG_TAG "lights"
#define LOG_NDEBUG 0

#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

char const*const PANEL_FILE
        = "/sys/class/backlight/panel/brightness";

#ifndef EXYNOS4210_TABLET
char const*const BUTTON_FILE
        = "/sys/class/sec/sec_touchkey/brightness";
#endif

#ifdef LED_NOTIFICATION
static char const RED_LED_DIR[]   = "/sys/class/leds/red";
static char const BLUE_LED_DIR[]  = "/sys/class/leds/blue";
#endif // LED_NOTIFICATION
void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

#ifdef LED_NOTIFICATION
static struct led_state {
    unsigned int enabled;
    int delay_on, delay_off;
} battery_red, battery_blue, notifications_red, notifications_blue;
#endif // LED_NOTIFICATION
static int
write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

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

#ifdef LED_NOTIFICATION
static int write_str(char const *path, char const *str)
{
    int fd;
    static int already_warned = 0;

    ALOGV("write_str: path=\"%s\", str=\"%s\".", path, str);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        int amt = write(fd, str, strlen(str));
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_str failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

/* Should check for snprintf truncation, but as these functions only use
 * internal paths, meh. */
static int write_df_int(char const *dir, char const *file, int value)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    return write_int(path, value);
}

static int write_df_str(char const *dir, char const *file, char const *str)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    return write_str(path, str);
}
#endif // LED_NOTIFICATION

static int
is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

#ifdef LED_NOTIFICATION
static void comp_led_states(struct led_state *red, struct led_state *blue,
             struct light_state_t const* state)
{
    unsigned int color = state->color;
    int delay_on, delay_off;

    switch (state->flashMode) {
    case LIGHT_FLASH_TIMED:
        delay_on  = state->flashOnMS;
        delay_off = state->flashOffMS;
        break;
    default:
        ALOGI("Unsuported flashMode %d, default to NONE.", state->flashMode);
    case LIGHT_FLASH_NONE:
        delay_on = delay_off = 0;
        break;
    }

    red->enabled   = !!(color >> 16 & 0xff);
    red->delay_on  = delay_on;
    red->delay_off = delay_off;

    blue->enabled   = !!(color & 0xff);
    blue->delay_on  = delay_on;
    blue->delay_off = delay_off;

    ALOGV("comp_led_states: red=(%u, %d, %d), blue=(%u, %d, %d).",
         red->enabled, red->delay_on, red->delay_off, blue->enabled,
            blue->delay_on, blue->delay_off);
}

static int set_led(char const *dir, struct led_state const *battery,
            struct led_state const *notifications)
{

    struct led_state const *state = NULL;
    int res;

    if (notifications->enabled)
        state = notifications;
    else if (battery->enabled)
        state = battery;

    if (state != NULL) {
        int delay_on  = state->delay_on;
        int delay_off = state->delay_off;

        if (delay_on > 0 && delay_off > 0) {
             /* Handling of blink_count is wrong in the kernel, blinking indefinitely
             * for any non-zero value.  TW lights just sets it to 1. */
            if ((res = write_df_str(dir, "trigger",     "notification")) < 0) return res;
            if ((res = write_df_str(dir, "brightness",  "255"         )) < 0) return res;
            if ((res = write_df_str(dir, "blink_count", "1"           )) < 0) return res;
            if ((res = write_df_int(dir, "delay_on",    delay_on      )) < 0) return res;
            if ((res = write_df_int(dir, "delay_off",   delay_off     )) < 0) return res;
        } else {
            if ((res = write_df_str(dir, "trigger",    "none")) < 0) return res;
            if ((res = write_df_str(dir, "brightness", "255" )) < 0) return res;
        }
    } else {
        if ((res = write_df_str(dir, "trigger",    "none")) < 0) return res;
        if ((res = write_df_str(dir, "brightness", "0"   )) < 0) return res;
    }

    return 0;
}
#endif // LED_NOTIFICATION

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    static int s_previous_brightness = -1;
    int brightness = rgb_to_brightness(state);

    pthread_mutex_lock(&g_lock);
    err = write_int(PANEL_FILE, brightness);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
#ifdef EXYNOS4210_TABLET
    return 0;
#else
    int err = 0;
    int brightness = rgb_to_brightness(state);

    pthread_mutex_lock(&g_lock);
    ALOGD("set_light_buttons: %d\n", brightness > 0 ? 1 : 2);
    err = write_int(BUTTON_FILE, brightness > 0 ? 1 : 2);
    pthread_mutex_unlock(&g_lock);

    return err;
#endif
}

static int
set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
   int res = 0;

#ifdef LED_NOTIFICATION
    ALOGD("set_light_battery: color=%#010x, fM=%u, fOnMS=%d, fOffMs=%d.",
          state->color, state->flashMode, state->flashOnMS, state->flashOffMS);

    pthread_mutex_lock(&g_lock);

    comp_led_states(&battery_red, &battery_blue, state);

    if ((res = set_led(RED_LED_DIR,  &battery_red,  &notifications_red)) >= 0)
           res = set_led(BLUE_LED_DIR, &battery_blue, &notifications_blue);

    pthread_mutex_unlock(&g_lock);
#endif // LED_NOTIFICATION

    return res;
}

static int
set_light_notification(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int res = 0;

#ifdef LED_NOTIFICATION
     ALOGD("set_light_notification: color=%#010x, fM=%u, fOnMS=%d, fOffMs=%d.",
         state->color, state->flashMode, state->flashOnMS, state->flashOffMS);

    pthread_mutex_lock(&g_lock);

    comp_led_states(&notifications_red, &notifications_blue, state);

    if ((res = set_led(RED_LED_DIR,  &battery_red,  &notifications_red)) >= 0)
           res = set_led(BLUE_LED_DIR, &battery_blue, &notifications_blue);

    pthread_mutex_unlock(&g_lock);
#endif // LED_NOTIFICATION

    return res;
}

static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/******************************************************************************/
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
        set_light = set_light_backlight;
    }
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        set_light = set_light_buttons;
    }
    else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
        set_light = set_light_battery;
    }
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        set_light = set_light_notification;
    }
    else {
        return -EINVAL;
    }

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}


static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Samsung Exynos4210 Lights Module",
    .author = "The CyanogenMod Project",
    .methods = &lights_module_methods,
};
