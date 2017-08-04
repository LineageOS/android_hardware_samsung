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

amplifier_device_t *amp_dev;

/*
 * Loads the vendor amplifier library and grabs the needed functions.
 *
 * @return 0 on success, <0 on error.
 */
int load_amplifier_lib() {
    if (access(TFA_AMPLIFIER_LIBRARY_PATH, R_OK) < 0) {
        return -errno;
    }

    amp_dev = (amplifier_device_t *) malloc(sizeof(amplifier_device_t));
    if (amp_dev == NULL) {
        ALOGE("%s: Not enough memory to load the lib handle", __func__);
        return -ENOMEM;
    }

    amp_dev->lib_handle = dlopen(TFA_AMPLIFIER_LIBRARY_PATH, RTLD_NOW);
    if (amp_dev->lib_handle == NULL) {
        ALOGE("%s: DLOPEN failed for %s (%s)", __func__,
                TFA_AMPLIFIER_LIBRARY_PATH, dlerror());
        return -1;
    }

    ALOGV("%s: DLOPEN successful for %s", __func__,
            TFA_AMPLIFIER_LIBRARY_PATH);
    amp_dev->tfa_device_open =
            (int (*)(tfa_handle_t*, int))dlsym(amp_dev->lib_handle, "tfa_device_open");
    amp_dev->tfa_enable =
            (int (*)(tfa_handle_t*, int))dlsym(amp_dev->lib_handle, "tfa_enable");
    if (amp_dev->tfa_device_open == NULL || amp_dev->tfa_enable == NULL) {
        ALOGE("%s: Error grabbing functions in %s (%s)", __func__,
                TFA_AMPLIFIER_LIBRARY_PATH, dlerror());
        amp_dev->tfa_device_open = 0;
        amp_dev->tfa_enable = 0;
        return -1;
    }

    // allocate memory for tfa handle
    amp_dev->tfa_handle = malloc(sizeof(tfa_handle_t));
    if (amp_dev->tfa_handle == NULL) {
        ALOGE("%s: Not enough memory to load the tfa handle", __func__);
        return -ENOMEM;
    }

    return 0;
 }

/*
 * Initializes the amplifier device.
 *
 * @return 0 on success, <0 on error.
 */
int speaker_amp_device_open() {
    ALOGV("Opening amplifier device");

    int rc = amp_dev->tfa_device_open(amp_dev->tfa_handle, 0);
    if (rc < 0) {
        ALOGE("Failed to open amplifier device");
        return rc;
    }

    pthread_mutex_init(&amp_dev->tfa_lock, (const pthread_mutexattr_t *) NULL);

    speaker_amp_disable();

    return 0;
}

#if 0
/*
 * WTF
 */
int sub_F42A(int a2)
{
  unsigned int v3 = amp_dev->tfa_handle[12];
  //int v4 = amp_dev->tfa_handle[2];
  for (unsigned int i = 0; i < v3; i++)
  {
    if ((amp_dev->tfa_handle[3] * i) == (unsigned int)a2 )
      return 0;
  }
  if ( v3 <= 9 )
  {
    amp_dev->tfa_handle[2+v3] = a2;
    amp_dev->tfa_handle[12] += 1;
    return 0;
  }
  return 3;
}
#endif

/*
 * Enables the amplifier device.
 *
 * @param something ???.
 * @return 0 on success, != 0 on error.
 */
int speaker_amp_enable(int something) {
    int rc = 0;
    ALOGV("Enabling amplifier device");

    pthread_mutex_lock(&amp_dev->tfa_lock);
    // WTF
    if (amp_dev->tfa_handle->a1[0] != 0)
        amp_dev->tfa_handle->a1[0] = 0;

    rc = amp_dev->tfa_enable(amp_dev->tfa_handle, 1);
    if (rc) {
        ALOGE("Failed to enable amplifier device");
        rc = 4;
    } else {
        //TODO
        //sub_F42A(something);
        (void)something;
        //++refcount
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
        rc = 5;
    } else {
        //TODO
        //++refcount
    }
    pthread_mutex_unlock(&amp_dev->tfa_lock);

    return rc;
}
