/*
 * Copyright (C) 2012 The CyanogenMod Project
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

#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>
#include <hardware/hardware.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/** Paths to light files **/
char const *const BACKLIGHT_FILE = "/sys/class/backlight/panel/brightness";
char const *const BUTTON_FILE = "/sys/class/sec/sec_touchkey/brightness";
char const *const NOTIFICATION_FILE = "/sys/class/misc/backlightnotification/notification_led";

/** Write integer to file **/
static int write_int(char const *path, int value)
{
	int fd;
	static int already_warned = -1;
	fd = open(path, O_RDWR);
	if (fd >= 0) {
		char buffer[20];
		int bytes = sprintf(buffer, "%d\n", value);
		int amt = write(fd, buffer, bytes);
		close(fd);
		return amt == -1 ? -errno : 0;
	} else {
		if (already_warned == -1) {
			ALOGE("write_int failed to open %s\n", path);
			already_warned = 1;
		}
		return -errno;
	}
}

/** Convert RGB to 0-255 integer **/
static int rgb_to_brightness(struct light_state_t const *state)
{
	int color = state->color & 0x00ffffff;
	return ((77 * ((color >> 16) & 0x00ff))
		+ (150 * ((color >> 8) & 0x00ff)) +
		(29 * (color & 0x00ff))) >> 8;
}

/** Is light already enabled? **/
static int is_lit(struct light_state_t const* state)
{
	return state->color & 0x00ffffff;
}

/** Set LCD backlight **/
static int set_light_backlight(struct light_device_t *dev, struct light_state_t const *state)
{
	pthread_mutex_lock(&g_lock);
	int err = write_int(BACKLIGHT_FILE, rgb_to_brightness(state));
	pthread_mutex_unlock(&g_lock);
	return err;
}

/** Set buttons backlight **/
static int set_light_buttons(struct light_device_t* dev, struct light_state_t const* state)
{
	int err = 0;
	pthread_mutex_lock (&g_lock);
	if(is_lit(state))
		err = write_int(BUTTON_FILE, 1);
	else
		err = write_int(BUTTON_FILE, 0);
	pthread_mutex_unlock (&g_lock);
	return err;
}

/** Set buttons backlight as BLN **/
static int set_light_notifications(struct light_device_t* dev, struct light_state_t const* state)
{
	int err = 0;
	pthread_mutex_lock (&g_lock);
	if(is_lit(state))
		err = write_int(NOTIFICATION_FILE, 1);
	else
		err = write_int(NOTIFICATION_FILE, 0);
	pthread_mutex_unlock (&g_lock);
	return err;
}

/** Close the lights device **/
static int close_lights(struct light_device_t *dev)
{
	if (dev)
		free(dev);
	return 0;
}

/** Open a new instance of a lights device using name **/
static int open_lights(const struct hw_module_t *module,
			char const *name, struct hw_device_t **device)
{
	pthread_t lighting_poll_thread;

	int (*set_light) (struct light_device_t *dev,
			  struct light_state_t const *state);

	if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
		set_light = set_light_backlight;
	else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
		set_light = set_light_buttons;
	else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
		set_light = set_light_notifications;
	else
		return -EINVAL;

	pthread_mutex_init(&g_lock, NULL);

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

/** Method list **/
static struct hw_module_methods_t lights_methods =
{
	.open =  open_lights,
};

/** The backlight module **/

struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "Lights module",
	.author = "The CyanogenMod Project",
	.methods = &lights_methods,
};
