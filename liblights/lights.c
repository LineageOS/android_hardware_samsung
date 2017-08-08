/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2015-2016 The CyanogenMod Project
 * Copyright (C) 2017 The LineageOS Project
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

#define LOG_TAG "SamsungLightsHAL"
/* #define LOG_NDEBUG 0 */

#include <cutils/log.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>
#include <liblights/samsung_lights_helper.h>

#include "samsung_lights.h"

#define COLOR_MASK 0x00ffffff

#define MAX_INPUT_BRIGHTNESS 255

enum component_mask_t {
    COMPONENT_BACKLIGHT = 0x1,
    COMPONENT_BUTTON_LIGHT = 0x2,
    COMPONENT_LED = 0x4,
    COMPONENT_BLN = 0x8,
};

enum light_t {
    TYPE_BATTERY = 0,
    TYPE_NOTIFICATION = 1,
    TYPE_ATTENTION = 2,
};

// Assume backlight is always supported
static int hw_components = COMPONENT_BACKLIGHT;

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

struct backlight_config {
    int cur_brightness, max_brightness;
};

struct led_config {
    unsigned int color;
    int delay_on, delay_off;
};

static struct backlight_config g_backlight; // For panel backlight
static struct led_config g_leds[3]; // For battery, notifications, and attention.
static int g_cur_led = -1;          // Presently showing LED of the above.

void check_component_support()
{
    if (access(BUTTON_BRIGHTNESS_NODE, W_OK) == 0)
        hw_components |= COMPONENT_BUTTON_LIGHT;
    if (access(LED_BLINK_NODE, W_OK) == 0)
        hw_components |= COMPONENT_LED;
#ifdef LED_BLN_NODE
    if (access(LED_BLN_NODE, W_OK) == 0)
        hw_components |= COMPONENT_BLN;
#endif
}

void init_g_lock(void)
{
    pthread_mutex_init(&g_lock, NULL);
}

static int write_str(char const *path, const char* value)
{
    int fd;
    static int already_warned;

    already_warned = 0;

    ALOGV("write_str: path %s, value %s", path, value);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        int amt = write(fd, value, strlen(value));
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_str failed to open %s", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int rgb_to_brightness(struct light_state_t const *state)
{
    int color = state->color & COLOR_MASK;

    return ((77*((color>>16) & 0x00ff))
        + (150*((color>>8) & 0x00ff)) + (29*(color & 0x00ff))) >> 8;
}

static int set_light_backlight(struct light_device_t *dev __unused,
                               struct light_state_t const *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    int max_brightness = g_backlight.max_brightness;

    /*
     * If max panel brightness is not the default (255),
     * apply linear scaling across the accepted range.
     */
    if (max_brightness != MAX_INPUT_BRIGHTNESS) {
        int old_brightness = brightness;
        brightness = brightness * max_brightness / MAX_INPUT_BRIGHTNESS;
        ALOGV("%s: scaling brightness %d => %d", __func__,
            old_brightness, brightness);
    }

    pthread_mutex_lock(&g_lock);
    err = set_cur_panel_brightness(brightness);
    if (err == 0)
        g_backlight.cur_brightness = brightness;

    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_light_buttons(struct light_device_t* dev __unused,
                             struct light_state_t const* state)
{
    int err = 0;
    pthread_mutex_lock(&g_lock);
    int brightness = (state->color & COLOR_MASK) ? 1 : 0;

#ifdef VAR_BUTTON_BRIGHTNESS
    brightness = rgb_to_brightness(state);
#endif
    err = set_cur_button_brightness(brightness);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int close_lights(struct light_device_t *dev)
{
    ALOGV("close_light is called");
    if (dev)
        free(dev);

    return 0;
}

/* LEDs */
static int write_leds(const struct led_config *led)
{
    static const struct led_config led_off = {0, 0, 0};

    char blink[32];
    int count, err;
    int color;

    if (led == NULL)
        led = &led_off;

    count = snprintf(blink,
                     sizeof(blink) - 1,
                     "0x%08x %d %d",
                     led->color,
                     led->delay_on,
                     led->delay_off);
    if (count < 0) {
        return -errno;
    } else if ((unsigned int)count >= sizeof(blink)-1) {
        ALOGE("%s: Truncated string: blink=\"%s\".", __func__, blink);
        return -EINVAL;
    }

    ALOGV("%s: color=0x%08x, delay_on=%d, delay_off=%d, blink=%s",
          __func__, led->color, led->delay_on, led->delay_off, blink);

    /* Add '\n' here to make the above log message clean. */
    blink[count]   = '\n';
    blink[count+1] = '\0';

    pthread_mutex_lock(&g_lock);
    err = write_str(LED_BLINK_NODE, blink);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int calibrate_color(int color, int brightness)
{
    int red = ((color >> 16) & 0xFF) * LED_ADJUSTMENT_R;
    int green = ((color >> 8) & 0xFF) * LED_ADJUSTMENT_G;
    int blue = (color & 0xFF) * LED_ADJUSTMENT_B;

    return (((red * brightness) / 255) << 16) + (((green * brightness) / 255) << 8) + ((blue * brightness) / 255);
}

static int set_light_leds(struct light_state_t const *state, int type)
{
    struct led_config *led;
    int err = 0;
    int adjusted_brightness;

    ALOGV("%s: type=%d, color=0x%010x, fM=%d, fOnMS=%d, fOffMs=%d.", __func__,
          type, state->color,state->flashMode, state->flashOnMS, state->flashOffMS);

    if (type < 0 || (size_t)type >= sizeof(g_leds)/sizeof(g_leds[0])) {
        return -EINVAL;
    }

    /* type is one of:
     *   0. battery
     *   1. notifications
     *   2. attention
     * which are multiplexed onto the same physical LED in the above order. */
    led = &g_leds[type];

    switch (state->flashMode) {
    case LIGHT_FLASH_NONE:
        /* Set LED to a solid color, spec is unclear on the exact behavior here. */
        led->delay_on = led->delay_off = 0;
        break;
    case LIGHT_FLASH_TIMED:
    case LIGHT_FLASH_HARDWARE:
        led->delay_on  = state->flashOnMS;
        led->delay_off = state->flashOffMS;
        break;
    default:
        return -EINVAL;
    }

    switch (type) {
    case TYPE_BATTERY:
        adjusted_brightness = LED_BRIGHTNESS_BATTERY;
        break;
    case TYPE_NOTIFICATION:
        adjusted_brightness = LED_BRIGHTNESS_NOTIFICATION;
        break;
    case TYPE_ATTENTION:
        adjusted_brightness = LED_BRIGHTNESS_ATTENTION;
        break;
    default:
        adjusted_brightness = 255;
    }



    led->color = calibrate_color(state->color & COLOR_MASK, adjusted_brightness);

    if (led->color > 0) {
        /* This LED is lit. */
        if (type >= g_cur_led) {
            /* And it has the highest priority, so show it. */
            err = write_leds(led);
            g_cur_led = type;
        }
    } else {
        /* This LED is not (any longer) lit. */
        if (type == g_cur_led) {
            /* But it is currently showing, switch to a lower-priority LED. */
            int i;

            for (i = type-1; i >= 0; i--) {
                if (g_leds[i].color > 0) {
                    /* Found a lower-priority LED to switch to. */
                    err = write_leds(&g_leds[i]);
                    goto switched;
                }
            }

            /* No LEDs are lit, turn off. */
            err = write_leds(NULL);
switched:
            g_cur_led = i;
        }
    }

    return err;
}

#ifdef LED_BLN_NODE
static int set_light_bln_notifications(struct light_device_t *dev __unused,
                                  struct light_state_t const *state)
{
    int err = 0;

    if (hw_components & COMPONENT_LED) {
        ALOGD("%s: BLN LED_BLINK color=0x%010x, fM=%d, fOnMS=%d, fOffMs=%d.", __func__,
            state->color, state->flashMode, state->flashOnMS, state->flashOffMS);

        struct led_config led;
        switch (state->flashMode) {
            case LIGHT_FLASH_TIMED:
            case LIGHT_FLASH_HARDWARE:
                led.delay_on  = state->flashOnMS;
                led.delay_off = state->flashOffMS;
                break;
            default:
                /* Set LED to a solid color, spec is unclear on the exact behavior here. */
                led.delay_on = led.delay_off = 0;
                break;
        }
        led.color = LED_BRIGHTNESS_NOTIFICATION;
        write_leds(&led);
    }

    pthread_mutex_lock(&g_lock);
    err = write_str(LED_BLN_NODE, state->color & COLOR_MASK ? "1" : "0");
    pthread_mutex_unlock(&g_lock);

    return err;
}
#endif
static int set_light_leds_battery(struct light_device_t *dev __unused,
                                  struct light_state_t const *state)
{
    return set_light_leds(state, TYPE_BATTERY);
}

static int set_light_leds_notifications(struct light_device_t *dev __unused,
                                        struct light_state_t const *state)
{
    return set_light_leds(state, TYPE_NOTIFICATION);
}

static int set_light_leds_attention(struct light_device_t *dev __unused,
                                    struct light_state_t const *state)
{
    struct light_state_t fixed;

    memcpy(&fixed, state, sizeof(fixed));

    /* The framework does odd things with the attention lights, fix them up to
     * do something sensible here. */
    switch (fixed.flashMode) {
    case LIGHT_FLASH_NONE:
        /* LightsService.Light::stopFlashing calls with non-zero color. */
        fixed.color = 0;
        break;
    case LIGHT_FLASH_HARDWARE:
        /* PowerManagerService::setAttentionLight calls with onMS=3, offMS=0, which
         * just makes for a slightly-dimmer LED. */
        if (fixed.flashOnMS > 0 && fixed.flashOffMS == 0)
            fixed.flashMode = LIGHT_FLASH_NONE;
            fixed.color = 0x000000ff;
        break;
    }

    return set_light_leds(&fixed, TYPE_ATTENTION);
}

static int open_lights(const struct hw_module_t *module, char const *name,
                        struct hw_device_t **device)
{
    int requested_component;
    int (*set_light)(struct light_device_t *dev,
        struct light_state_t const *state);

    check_component_support();

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
        requested_component = COMPONENT_BACKLIGHT;
        set_light = set_light_backlight;
    } else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        requested_component = COMPONENT_BUTTON_LIGHT;
        set_light = set_light_buttons;
    } else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
        requested_component = COMPONENT_LED;
        set_light = set_light_leds_battery;
    } else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        requested_component = COMPONENT_LED;
        set_light = set_light_leds_notifications;
#ifdef LED_BLN_NODE
        if (hw_components & COMPONENT_BLN) {
            requested_component = COMPONENT_BLN;
            set_light = set_light_bln_notifications;
        }
#endif
    } else if (0 == strcmp(LIGHT_ID_ATTENTION, name)) {
        requested_component = COMPONENT_LED;
        set_light = set_light_leds_attention;
    } else {
        return -EINVAL;
    }

    if ((hw_components & requested_component) == 0) {
        ALOGV("%s: component 0x%x not supported by device", __func__,
            requested_component);
        return -EINVAL;
    }

    int max_brightness = get_max_panel_brightness();
    if (max_brightness < 0) {
        ALOGE("%s: failed to read max panel brightness, fallback to 255!",
            __func__);
        max_brightness = 255;
    }
    g_backlight.max_brightness = max_brightness;

    pthread_once(&g_init, init_g_lock);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t *))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t *)dev;

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
    .name = "Samsung Lights Module",
    .author = "The CyanogenMod Project",
    .methods = &lights_module_methods,
};
