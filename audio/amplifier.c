/*
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
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

#define LOG_TAG "audio_hw_amplifier"
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "amplifier.h"
#include "audio_hw.h"


amplifier_device_t *amp_dev;

/*
 * Loads the vendor amplifier library and grabs the needed functions.
 *
 * @return 0 on success, <0 on error.
 */
static int load_amplifier_lib(amplifier_device_t *amp_dev) {
    if (access(TFA_AMPLIFIER_LIBRARY_PATH, R_OK) < 0) {
        return -errno;
    }

    amp_dev->lib_handle = dlopen(TFA_AMPLIFIER_LIBRARY_PATH, RTLD_NOW);
    if (amp_dev->lib_handle == NULL) {
        ALOGE("%s: DLOPEN failed for %s (%s)", __func__, TFA_AMPLIFIER_LIBRARY_PATH, dlerror());
        return -1;
    } else {
        ALOGV("%s: DLOPEN successful for %s", __func__, TFA_AMPLIFIER_LIBRARY_PATH);
    }

    amp_dev->tfa_device_open = (tfa_device_open_t)dlsym(amp_dev->lib_handle, "tfa_device_open");
    if (amp_dev->tfa_device_open == NULL) {
        ALOGE("%s: dlsym error %s for tfa_device_open", __func__, dlerror());
        amp_dev->tfa_device_open = 0;
        return -1;
    }

    amp_dev->tfa_enable = (tfa_enable_t)dlsym(amp_dev->lib_handle, "tfa_enable");
    if (amp_dev->tfa_enable == NULL) {
        ALOGE("%s: dlsym error %s for tfa_enable", __func__, dlerror());
        amp_dev->tfa_enable = 0;
        return -1;
    }

    return 0;
}

/*
 * Enables the amplifier device.
 *
 * @return 0 on success, != 0 on error.
 */
int speaker_amp_enable() {
    int rc = 0;

    ALOGV("Enabling amplifier device");
    pthread_mutex_lock(&amp_dev->tfa_lock);
    // ???
    if (amp_dev->tfa_handle->a1 != 0) {
        amp_dev->tfa_handle->a1 = 0;
    }
    rc = amp_dev->tfa_enable(amp_dev->tfa_handle, 1);
    if (rc) {
        ALOGE("Failed to enable amplifier device");
    }
    pthread_mutex_unlock(&amp_dev->tfa_lock);

    return rc;
}

/*
 * Disables the amplifier device.
 *
 * @return 0 on success, != 0 on error.
 */
int speaker_amp_disable() {
    int rc = 0;

    ALOGV("Disabling amplifier device");
    pthread_mutex_lock(&amp_dev->tfa_lock);
    rc = amp_dev->tfa_enable(amp_dev->tfa_handle, 0);
    if (rc) {
        ALOGE("Failed to disable amplifier device");
    }
    pthread_mutex_unlock(&amp_dev->tfa_lock);

    return rc;
}

/*
 * Initializes the amplifier device.
 *
 * @return 0 on success, <0 on error.
 */
int speaker_amp_device_open(struct audio_device *adev) {
    int rc = 0;
    //amplifier_device_t *amp_dev = adev->amplifier_device;
    adev->amplifier_device = amp_dev;

    ALOGV("Opening amplifier device");
    adev->amplifier_state = AMPLIFIER_ABSENT;

    amp_dev = (amplifier_device_t *) malloc(sizeof(amplifier_device_t));
    if (amp_dev == NULL) {
        ALOGE("%s: Not enough memory to load the lib handle", __func__);
        return -ENOMEM;
    }

    // allocate memory for tfa handle
    amp_dev->tfa_handle = malloc(sizeof(tfa_handle_t));
    if (amp_dev->tfa_handle == NULL) {
        ALOGE("%s: Not enough memory to load the tfa handle", __func__);
        return -ENOMEM;
    }

    rc = load_amplifier_lib(amp_dev);
    if (rc < 0) {
        return rc;
    }

    rc = amp_dev->tfa_device_open(amp_dev->tfa_handle, 0);
    if (rc < 0) {
        ALOGE("Failed to open amplifier device");
        return rc;
    }

    pthread_mutex_init(&amp_dev->tfa_lock, (const pthread_mutexattr_t *) NULL);

    rc = speaker_amp_disable(amp_dev);
    if (rc == 0) {
        adev->amplifier_state = AMPLIFIER_OFFLINE;
    }

    return 0;
}

/*
 * De-Initializes the amplifier device.
 *
 * @return 0 on success, <0 on error.
 */
int speaker_amp_device_close(struct audio_device *adev) {
    int rc = 0;
    //amplifier_device_t *amp_dev = adev->amplifier_device;
    adev->amplifier_device = amp_dev;

    ALOGV("Closing amplifier device");
    if (adev->amplifier_state == AMPLIFIER_ONLINE) {
        speaker_amp_disable(amp_dev);
    }

    pthread_mutex_destroy(&amp_dev->tfa_lock);

    if (amp_dev->tfa_handle) {
        free(amp_dev->tfa_handle);
    }

    if (amp_dev->lib_handle) {
        dlclose(amp_dev->lib_handle);
    }

    if (amp_dev) {
        free(amp_dev);
    }

    return rc;
}
