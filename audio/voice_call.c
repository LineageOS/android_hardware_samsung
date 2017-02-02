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

#define LOG_TAG "audio_voice_call"
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <stdlib.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include <samsung_audio.h>

#include "audio_hw.h"
#include "voice_call.h"

static struct pcm_config pcm_config_voice = {
    .channels = 2,
    .rate = 8000,
    .period_size = 320,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

static struct pcm_config pcm_config_voice_wide = {
    .channels = 2,
    .rate = 16000,
    .period_size = 320,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

/* Prototypes */
int start_voice_call(struct audio_device *adev);
int stop_voice_call(struct audio_device *adev);

static void voice_call_set_audio_path(struct voice_call *vc)
{
    enum _AudioPath device_type;

    switch(vc->out_device) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            device_type = SOUND_AUDIO_PATH_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_EARPIECE:
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            device_type = SOUND_AUDIO_PATH_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            device_type = SOUND_AUDIO_PATH_HEADPHONE;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            device_type = SOUND_AUDIO_PATH_BLUETOOTH;
            break;
        default:
            /* if output device isn't supported, use handset by default */
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
    }

    ALOGV("%s: ril_set_call_audio_path(%d)", __func__, device_type);

    ril_set_call_audio_path(&vc->ril, device_type);
}

void voice_call_set_volume(struct voice_call *vc, float volume)
{
    enum _SoundType sound_type;

    switch (vc->out_device) {
        case AUDIO_DEVICE_OUT_EARPIECE:
            sound_type = SOUND_TYPE_VOICE;
            break;
        case AUDIO_DEVICE_OUT_SPEAKER:
            sound_type = SOUND_TYPE_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            sound_type = SOUND_TYPE_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        case AUDIO_DEVICE_OUT_ALL_SCO:
            sound_type = SOUND_TYPE_BTVOICE;
            break;
        default:
            sound_type = SOUND_TYPE_VOICE;
    }

    ril_set_call_volume(&vc->ril, sound_type, volume);
}

static void wb_amr_callback(void *data, int enable)
{
    struct audio_device *adev = (struct audio_device *)data;
    struct voice_call *vc = adev->voice.call;

    pthread_mutex_lock(&adev->lock);

    if (vc->wb_amr != enable) {
        vc->wb_amr = enable;

        /* reopen the modem PCMs at the new rate */
        if (adev->voice.in_call) {
            ALOGV("%s: %s wide band voice call",
                  __func__,
                  enable ? "Enable" : "Disable");

            stop_voice_call(adev);
            start_voice_call(adev);
        }
    }

    pthread_mutex_unlock(&adev->lock);
}

struct voice_call *voice_call_init(struct audio_device *adev)
{
    char voice_config[PROPERTY_VALUE_MAX];
    struct voice_call *vc;
    int ret;

    vc = calloc(1, sizeof(struct voice_call));
    if (vc == NULL) {
        return NULL;
    }

    ret = property_get("audio_hal.force_voice_config", voice_config, "");
    if (ret > 0) {
        if ((strncmp(voice_config, "narrow", 6)) == 0)
            vc->wb_amr = false;
        else if ((strncmp(voice_config, "wide", 4)) == 0)
            vc->wb_amr = true;
        ALOGV("%s: Forcing voice config: %s", __func__, voice_config);
    } else {
        /* register callback for wideband AMR setting */
        ril_register_set_wb_amr_callback(wb_amr_callback, (void *)adev);
    }

    /* Two mic control */
    ret = property_get_bool("audio_hal.disable_two_mic", false);
    if (ret > 0) {
        vc->two_mic_disabled = true;
    }

    /* Do this as the last step so we do not have to close it on error */
    ret = ril_open(&vc->ril);
    if (ret != 0) {
        free(vc);
        return NULL;
    }

    return vc;
}

void voice_call_deinit(struct voice_call *vc)
{
    ril_close(&vc->ril);
    free(vc);
}

/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
int voice_call_start(struct voice_call *vc, struct audio_usecase *uc_info)
{
#if defined(SOUND_CAPTURE_VOICE_DEVICE) && defined(SOUND_CAPTURE_VOICE_DEVICE)
    struct pcm_config *voice_config;

    if (vc->pcm_voice_rx != NULL || vc->pcm_voice_tx != NULL) {
        ALOGW("%s: Voice PCMs already open!\n", __func__);
        return 0;
    }

    ALOGV("%s: Opening voice PCMs", __func__);

    if (vc->wb_amr) {
        voice_config = &pcm_config_voice_wide;
    } else {
        voice_config = &pcm_config_voice;
    }

    /* Open modem PCM channels */
    vc->pcm_voice_rx = pcm_open(SOUND_CARD,
                                SOUND_PLAYBACK_VOICE_DEVICE,
                                PCM_OUT|PCM_MONOTONIC,
                                voice_config);
    if (vc->pcm_voice_rx != NULL && !pcm_is_ready(vc->pcm_voice_rx)) {
        ALOGE("%s: cannot open PCM voice RX stream: %s",
              __func__,
              pcm_get_error(vc->pcm_voice_rx));

        pcm_close(vc->pcm_voice_tx);
        vc->pcm_voice_tx = NULL;

        return -ENOMEM;
    }

    vc->pcm_voice_tx = pcm_open(SOUND_CARD,
                                SOUND_CAPTURE_VOICE_DEVICE,
                                PCM_IN|PCM_MONOTONIC,
                                voice_config);
    if (vc->pcm_voice_tx != NULL && !pcm_is_ready(vc->pcm_voice_tx)) {
        ALOGE("%s: cannot open PCM voice TX stream: %s",
              __func__,
              pcm_get_error(vc->pcm_voice_tx));

        pcm_close(vc->pcm_voice_rx);
        vc->pcm_voice_rx = NULL;
        return -ENOMEM;
    }

    pcm_start(vc->pcm_voice_rx);
    pcm_start(vc->pcm_voice_tx);

    vc->out_device = uc_info->out_snd_device;

    /* TODO: handle SCO */

    switch (vc->out_device) {
        case AUDIO_DEVICE_OUT_EARPIECE:
        case AUDIO_DEVICE_OUT_SPEAKER:
            vc->two_mic_control = true;
            break;
        default:
            vc->two_mic_control = false;
            break;
    }

    if (vc->two_mic_disabled) {
        vc->two_mic_control = false;
    }

    if (vc->two_mic_control) {
        ALOGV("%s: enabling two mic control", __func__);
        ril_set_two_mic_control(&vc->ril, AUDIENCE, TWO_MIC_SOLUTION_ON);
    } else {
        ALOGV("%s: disabling two mic control", __func__);
        ril_set_two_mic_control(&vc->ril, AUDIENCE, TWO_MIC_SOLUTION_OFF);
    }

    voice_call_set_audio_path(vc);
    ril_set_call_clock_sync(&vc->ril, SOUND_CLOCK_START);
#endif /* SOUND_CAPTURE_VOICE_DEVICE && SOUND_CAPTURE_VOICE_DEVICE */

    return 0;
}

/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
void voice_call_stop(struct voice_call *vc)
{
#if defined(SOUND_CAPTURE_VOICE_DEVICE) && defined(SOUND_CAPTURE_VOICE_DEVICE)
    int status = 0;

    ALOGV("%s: Closing active PCMs", __func__);

    if (vc->pcm_voice_rx != NULL) {
        pcm_stop(vc->pcm_voice_rx);
        pcm_close(vc->pcm_voice_rx);
        vc->pcm_voice_rx = NULL;
        status++;
    }

    if (vc->pcm_voice_tx != NULL) {
        pcm_stop(vc->pcm_voice_tx);
        pcm_close(vc->pcm_voice_tx);
        vc->pcm_voice_tx = NULL;
        status++;
    }

    /* TODO: handle SCO */

    vc->out_device = AUDIO_DEVICE_NONE;

    ALOGV("%s: Successfully closed %d active PCMs", __func__, status);
#endif /* SOUND_CAPTURE_VOICE_DEVICE && SOUND_CAPTURE_VOICE_DEVICE */
}
