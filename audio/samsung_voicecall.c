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

#define LOG_TAG "samsung_voicecall"
#define LOG_NDEBUG 0
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include <system/thread_defs.h>

#include <samsung_audio.h>

#include "audio_hw.h"
#include "samsung_voicecall.h"

static struct samsung_call *g_call;

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

/* TODO: Move to header! */
#define PCM_CARD_VOICE 0
#define PCM_DEVICE_VOICE 1

/* Prototypes */
int start_voice_call(struct audio_device *adev);
int stop_voice_call(struct audio_device *adev);

static void set_call_audio_path(struct audio_device *adev)
{
    enum _AudioPath device_type;

    switch(g_call->out_device) {
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

    ril_set_call_audio_path(&g_call->ril, device_type);
}

/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
int start_voice_call_(struct audio_device *adev, struct audio_usecase *uc_info)
{
    struct pcm_config *voice_config;

    if (g_call->pcm_voice_rx != NULL || g_call->pcm_voice_tx != NULL) {
        ALOGW("%s: Voice PCMs already open!\n", __func__);
        return 0;
    }

    ALOGV("%s: Opening voice PCMs", __func__);

    if (g_call->wb_amr) {
        voice_config = &pcm_config_voice_wide;
    } else {
        voice_config = &pcm_config_voice;
    }

    /* Open modem PCM channels */
    g_call->pcm_voice_rx = pcm_open(PCM_CARD_VOICE,
                                  PCM_DEVICE_VOICE,
                                  PCM_OUT | PCM_MONOTONIC,
                                  voice_config);
    if (g_call->pcm_voice_rx != NULL && !pcm_is_ready(g_call->pcm_voice_rx)) {
        ALOGE("%s: cannot open PCM voice RX stream: %s",
              __func__, pcm_get_error(g_call->pcm_voice_rx));
        pcm_close(g_call->pcm_voice_tx);
        g_call->pcm_voice_tx = NULL;
        return -ENOMEM;
    }

    g_call->pcm_voice_tx = pcm_open(PCM_CARD_VOICE,
                                  PCM_DEVICE_VOICE,
                                  PCM_IN,
                                  voice_config);
    if (g_call->pcm_voice_tx != NULL && !pcm_is_ready(g_call->pcm_voice_tx)) {
        ALOGE("%s: cannot open PCM voice TX stream: %s",
              __func__, pcm_get_error(g_call->pcm_voice_tx));
        pcm_close(g_call->pcm_voice_rx);
        g_call->pcm_voice_rx = NULL;
        return -ENOMEM;
    }

    pcm_start(g_call->pcm_voice_rx);
    pcm_start(g_call->pcm_voice_tx);

    g_call->out_device = uc_info->out_snd_device;

    /* start SCO stream if needed */
    if (g_call->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        // TODO
        //start_bt_sco(adev);
    }

    switch (g_call->out_device) {
        case AUDIO_DEVICE_OUT_EARPIECE:
        case AUDIO_DEVICE_OUT_SPEAKER:
            g_call->two_mic_control = true;
            break;
        default:
            g_call->two_mic_control = false;
            break;
    }

    if (g_call->two_mic_disabled) {
        g_call->two_mic_control = false;
    }

    if (g_call->two_mic_control) {
        ALOGV("%s: enabling two mic control", __func__);
        ril_set_two_mic_control(&g_call->ril, AUDIENCE, TWO_MIC_SOLUTION_ON);
    } else {
        ALOGV("%s: disabling two mic control", __func__);
        ril_set_two_mic_control(&g_call->ril, AUDIENCE, TWO_MIC_SOLUTION_OFF);
    }

    set_call_audio_path(adev);
    ril_set_call_clock_sync(&g_call->ril, SOUND_CLOCK_START);

    return 0;
}

/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
void stop_voice_call_(struct audio_device *adev)
{
    int status = 0;

    ALOGV("%s: Closing active PCMs", __func__);

    if (g_call->pcm_voice_rx) {
        pcm_stop(g_call->pcm_voice_rx);
        pcm_close(g_call->pcm_voice_rx);
        g_call->pcm_voice_rx = NULL;
        status++;
    }

    if (g_call->pcm_voice_tx) {
        pcm_stop(g_call->pcm_voice_tx);
        pcm_close(g_call->pcm_voice_tx);
        g_call->pcm_voice_tx = NULL;
        status++;
    }

    /* End SCO stream if needed */
    if (g_call->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        // TODO
        //stop_bt_sco(adev);
        status++;
    }

    g_call->out_device = AUDIO_DEVICE_NONE;

    ALOGV("%s: Successfully closed %d active PCMs", __func__, status);
}

/* always called with adev lock held */
void set_voice_volume_l_(struct audio_device *adev, float volume)
{
    enum _SoundType sound_type;

    switch (g_call->out_device) {
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

    ril_set_call_volume(&g_call->ril, sound_type, volume);
}

static void wb_amr_callback(void *data, int enable)
{
    struct audio_device *adev = (struct audio_device *)data;

    pthread_mutex_lock(&adev->lock);

    if (g_call->wb_amr != enable) {
        g_call->wb_amr = enable;

        /* reopen the modem PCMs at the new rate */
        if (adev->in_call) {
            ALOGV("%s: %s Incall Wide Band support",
                  __func__,
                  enable ? "Turn on" : "Turn off");

            stop_voice_call(adev);
            start_voice_call(adev);
        }
    }

    pthread_mutex_unlock(&adev->lock);
}

int samsung_voicecall_init(struct audio_device *adev)
{
    g_call = calloc(1, sizeof(struct samsung_call));
    if (g_call == NULL) {
        return -ENOMEM;
    }

    /* RIL */
    ril_open(&g_call->ril);

    char voice_config[PROPERTY_VALUE_MAX];

    if (property_get("audio_hal.force_voice_config", voice_config, "") > 0) {
        if ((strncmp(voice_config, "narrow", 6)) == 0)
            g_call->wb_amr = false;
        else if ((strncmp(voice_config, "wide", 4)) == 0)
            g_call->wb_amr = true;
        ALOGV("%s: Forcing voice config: %s", __func__, voice_config);
    } else {
        /* register callback for wideband AMR setting */
        ril_register_set_wb_amr_callback(wb_amr_callback, (void *)adev);
    }

    /* Two mic control */
    if (property_get_bool("audio_hal.disable_two_mic", false))
        g_call->two_mic_disabled = true;

    return 0;
}
