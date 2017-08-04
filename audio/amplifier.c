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


amplifier_device_t *amp_dev = NULL;
amplifier_mode_t current_audio_mode = Audio_Mode_None;

/*
 * Loads the vendor amplifier library and grabs the needed functions.
 *
 * @param amp_dev Device handle.
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
 * Determines the appropriate amplifier mode for the current device & mode combo.
 *
 * @param amp_dev Device handle.
 *
 * @return amplifier_mode_t.
 */
static amplifier_mode_t get_audio_mode(amplifier_device_t *amp_dev)
{
    amplifier_mode_t audio_mode = Audio_Mode_None;
    struct listnode *node;
    struct audio_usecase *usecase;
    audio_mode_t mode = amp_dev->adev->mode;
    int i = 0;

    for (i = 0; i < Audio_Mode_Max; i++) {
        amp_dev->route_cnt[i] = 0;
    }

    list_for_each(node, &amp_dev->adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->devices & AUDIO_DEVICE_OUT_SPEAKER) {
            if (mode == AUDIO_MODE_IN_CALL) {
                if (amp_dev->adev->snd_dev_ref_cnt[usecase->out_snd_device] > 0) {
                    audio_mode = Audio_Mode_Voice;
                    amp_dev->route_cnt[audio_mode]++;
                    ALOGV("%s: audio_mode voice", __func__);
                }
            } else {
                if (amp_dev->adev->snd_dev_ref_cnt[usecase->out_snd_device] > 0) {
                    audio_mode = Audio_Mode_Music_Normal;
                    amp_dev->route_cnt[audio_mode]++;
                    ALOGV("%s: audio_mode music", __func__);
                }
            }
        } else {
            ALOGE("%s: No device match for mask 0x%X", __func__, usecase->devices);
        }
    }

    return audio_mode;
}

/*
 * Hooks into the vendor library and enables/disables the amplifier IC.
 *
 * @param amp_dev Device handle.
 * @param on true or false for enabling/disabling of the IC.
 *
 * @return 0 on success, != 0 on error.
 */
static int tfa_power(amplifier_device_t *amp_dev, bool on) {
    int rc = 0;

    ALOGV("%s: %s amplifier device", __func__, on ? "Enabling" : "Disabling");
    pthread_mutex_lock(&amp_dev->tfa_lock);
    if (on) {
        // ???
        if (amp_dev->tfa_handle->a1 != 0) {
            amp_dev->tfa_handle->a1 = 0;
        }
    }
    amp_dev->tfa_state = on ? Amplifier_State_Online : Amplifier_State_Offline;
    rc = amp_dev->tfa_enable(amp_dev->tfa_handle, on ? 1 : 0);
    if (rc) {
        ALOGE("%s: Failed to %s amplifier device", __func__, on ? "enable" : "disable");
    }
    pthread_mutex_unlock(&amp_dev->tfa_lock);

    return rc;
}

/*
 * Enables the amplifier device.
 *
 * @return 0 on success, != 0 on error.
 */
int speaker_amp_enable() {
    int rc = 0;
    amplifier_mode_t new_audio_mode = Audio_Mode_Music_Normal;
    int i = 0;

    new_audio_mode = get_audio_mode(amp_dev);
    if (new_audio_mode == Audio_Mode_None) {
        ALOGD("%s: Mode none, not enabling amplifier", __func__);
        return rc;
    }

    if (amp_dev->ref_cnt[new_audio_mode] >= 1 && amp_dev->tfa_state == Amplifier_State_Online) {
        ALOGD("%s Mode %d already active!", __func__, new_audio_mode);
        amp_dev->ref_cnt[new_audio_mode]++;
        return rc;
    }

    rc = tfa_power(amp_dev, true);

    current_audio_mode = new_audio_mode;
    for (i = 0; i < Audio_Mode_Max; i++) {
        amp_dev->ref_cnt[i] = amp_dev->route_cnt[i];
    }

    return rc;
}

/*
 * Disables the amplifier device.
 *
 * @param snd_device The current sound device.
 *
 * @return 0 on success, != 0 on error.
 */
int speaker_amp_disable(snd_device_t snd_device) {
    int rc = 0;
    amplifier_mode_t new_audio_mode = Audio_Mode_None;
    int i = 0;

    if ((current_audio_mode == Audio_Mode_None) || (snd_device > SND_DEVICE_OUT_END)) {
        return rc;
    }

    switch(snd_device) {
        case SND_DEVICE_OUT_SPEAKER:
            new_audio_mode = Audio_Mode_Music_Normal;
            break;
        case SND_DEVICE_OUT_VOICE_SPEAKER:
        case SND_DEVICE_OUT_VOICE_SPEAKER_WB:
            new_audio_mode = Audio_Mode_Voice;
            break;
        default:
            break;
    }

    if ((new_audio_mode == Audio_Mode_None) || (amp_dev->ref_cnt[new_audio_mode] <= 0)) {
        ALOGE("%s: Device ref cnt is already 0", __func__);
        return rc;
    }

    amp_dev->ref_cnt[new_audio_mode]--;

    for (i = 0; i < Audio_Mode_Max; i++) {
        if (amp_dev->ref_cnt[i] > 0) {
            ALOGD("%s: Speaker still in use, not disabling the amplifier", __func__);
            return rc;
        }
    }

    rc = tfa_power(amp_dev, false);
    current_audio_mode = Audio_Mode_None;

    return rc;
}

/*
 * Restarts the amplifier IC if a new audio mode needs to be applied.
 */
void speaker_amp_update() {
    amplifier_mode_t new_audio_mode = Audio_Mode_Music_Normal;

    //FIXME: remove
    ALOGV("%s: Enter", __func__);

    new_audio_mode = get_audio_mode(amp_dev);
    if (new_audio_mode <= current_audio_mode) {
        ALOGE("%s: Same mode, bailing out", __func__);
        return;
    }

    if (current_audio_mode != Audio_Mode_None) {
        tfa_power(amp_dev, false);
    }

    speaker_amp_enable();
}

void speaker_amp_set_mode() {
    amplifier_mode_t new_audio_mode = Audio_Mode_None;

    ALOGV("%s: enter\n", __func__);

    new_audio_mode = get_audio_mode(amp_dev);
    if (new_audio_mode == Audio_Mode_None) {
        return;
    }

    ALOGV("%s: exit\n", __func__);
}

/*
 * Initializes the amplifier device and local class data.
 *
 * @param adev The audio HAL device handle which we link to the amplifier
 *             device instance.
 *
 * @return 0 on success, <0 on error.
 */
int speaker_amp_device_open(struct audio_device *adev) {
    int rc = 0;

    ALOGV("Opening amplifier device");

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
        ALOGE("%s: Failed to open amplifier device", __func__);
        return rc;
    }

    pthread_mutex_init(&amp_dev->tfa_lock, (const pthread_mutexattr_t *) NULL);

    rc = tfa_power(amp_dev, false);
    amp_dev->adev = adev;

    return rc;
}

/*
 * De-Initializes the amplifier device.
 */
void speaker_amp_device_close() {
    ALOGV("%s: Closing amplifier device", __func__);
    tfa_power(amp_dev, false);

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
}
