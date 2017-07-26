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

#define LOG_TAG "audio_hw_voice"
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
#include "voice.h"

#ifdef AUDIENCE_EARSMART_IC
#include "audience.h"
#endif

static struct pcm_config pcm_config_voicecall = {
    .channels = 2,
    .rate = 8000,
    .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
    .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
    .format = PCM_FORMAT_S16_LE,
};

static struct pcm_config pcm_config_voicecall_wideband = {
    .channels = 2,
    .rate = 16000,
    .period_size = CAPTURE_PERIOD_SIZE_LOW_LATENCY,
    .period_count = CAPTURE_PERIOD_COUNT_LOW_LATENCY,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco = {
    .channels = 1,
    .rate = SCO_DEFAULT_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_voice_sco_wb = {
    .channels = 1,
    .rate = SCO_WB_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

/* Prototypes */
int start_voice_call(struct audio_device *adev);
int stop_voice_call(struct audio_device *adev);

void set_voice_session_audio_path(struct voice_session *session)
{
    enum _AudioPath device_type;
    int rc;

    switch(session->out_device) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            device_type = SOUND_AUDIO_PATH_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_EARPIECE:
            device_type = SOUND_AUDIO_PATH_EARPIECE;
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
            /* if output device isn't supported, use earpiece by default */
            device_type = SOUND_AUDIO_PATH_EARPIECE;
            break;
    }

    ALOGV("%s: ril_set_call_audio_path(%d)", __func__, device_type);

    rc = ril_set_call_audio_path(&session->ril, device_type);
    ALOGE_IF(rc != 0, "Failed to set audio path: (%d)", rc);
}

/*
 * This decides based on the output device, if we enable
 * two mic control
 */
void prepare_voice_session(struct voice_session *session,
                           audio_devices_t active_out_devices)
{
    ALOGV("%s: active_out_devices: 0x%x", __func__, active_out_devices);

    session->out_device = active_out_devices;

    switch (session->out_device) {
    case AUDIO_DEVICE_OUT_EARPIECE:
    case AUDIO_DEVICE_OUT_SPEAKER:
        session->two_mic_control = true;
        break;
    default:
        session->two_mic_control = false;
        break;
    }

    if (session->two_mic_disabled) {
        session->two_mic_control = false;
    }
}

/*
 * This must be called with the hw device mutex locked, OK to hold other
 * mutexes.
 */
static void stop_voice_session_bt_sco(struct voice_session *session) {
    ALOGV("%s: Closing SCO PCMs", __func__);

    if (session->pcm_sco_rx != NULL) {
        pcm_stop(session->pcm_sco_rx);
        pcm_close(session->pcm_sco_rx);
        session->pcm_sco_rx = NULL;
    }

    if (session->pcm_sco_tx != NULL) {
        pcm_stop(session->pcm_sco_tx);
        pcm_close(session->pcm_sco_tx);
        session->pcm_sco_tx = NULL;
    }
}

/* must be called with the hw device mutex locked, OK to hold other mutexes */
void start_voice_session_bt_sco(struct voice_session *session)
{
    struct pcm_config *voice_sco_config;

    if (session->pcm_sco_rx != NULL || session->pcm_sco_tx != NULL) {
        ALOGW("%s: SCO PCMs already open!\n", __func__);
        return;
    }

    ALOGV("%s: Opening SCO PCMs", __func__);

    if (session->wb_amr_type >= 1) {
        ALOGV("%s: pcm_config wideband", __func__);
        voice_sco_config = &pcm_config_voice_sco_wb;
    } else {
        ALOGV("%s: pcm_config narrowband", __func__);
        voice_sco_config = &pcm_config_voice_sco;
    }

    session->pcm_sco_rx = pcm_open(SOUND_CARD,
                                   SOUND_PLAYBACK_SCO_DEVICE,
                                   PCM_OUT|PCM_MONOTONIC,
                                   voice_sco_config);
    if (session->pcm_sco_rx != NULL && !pcm_is_ready(session->pcm_sco_rx)) {
        ALOGE("%s: cannot open PCM SCO RX stream: %s",
              __func__, pcm_get_error(session->pcm_sco_rx));
        goto err_sco_rx;
    }

    session->pcm_sco_tx = pcm_open(SOUND_CARD,
                                   SOUND_CAPTURE_SCO_DEVICE,
                                   PCM_IN|PCM_MONOTONIC,
                                   voice_sco_config);
    if (session->pcm_sco_tx && !pcm_is_ready(session->pcm_sco_tx)) {
        ALOGE("%s: cannot open PCM SCO TX stream: %s",
              __func__, pcm_get_error(session->pcm_sco_tx));
        goto err_sco_tx;
    }

    pcm_start(session->pcm_sco_rx);
    pcm_start(session->pcm_sco_tx);

    return;

err_sco_tx:
    pcm_close(session->pcm_sco_tx);
    session->pcm_sco_tx = NULL;
err_sco_rx:
    pcm_close(session->pcm_sco_rx);
    session->pcm_sco_rx = NULL;
}
/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
int start_voice_session(struct voice_session *session)
{
    struct pcm_config *voice_config;

    if (session->pcm_voice_rx != NULL || session->pcm_voice_tx != NULL) {
        ALOGW("%s: Voice PCMs already open!\n", __func__);
        return 0;
    }

    ALOGV("%s: Opening voice PCMs", __func__);

    /* TODO: Handle wb_amr=2 */
    if (session->wb_amr_type >= 1) {
        ALOGV("%s: pcm_config wideband", __func__);
        voice_config = &pcm_config_voicecall_wideband;
    } else {
        ALOGV("%s: pcm_config narrowband", __func__);
        voice_config = &pcm_config_voicecall;
    }

    /* Open modem PCM channels */
    session->pcm_voice_rx = pcm_open(SOUND_CARD,
                                     SOUND_PLAYBACK_VOICE_DEVICE,
                                     PCM_OUT|PCM_MONOTONIC,
                                     voice_config);
    if (session->pcm_voice_rx != NULL && !pcm_is_ready(session->pcm_voice_rx)) {
        ALOGE("%s: cannot open PCM voice RX stream: %s",
              __func__,
              pcm_get_error(session->pcm_voice_rx));

        pcm_close(session->pcm_voice_tx);
        session->pcm_voice_tx = NULL;

        return -ENOMEM;
    }

    session->pcm_voice_tx = pcm_open(SOUND_CARD,
                                     SOUND_CAPTURE_VOICE_DEVICE,
                                     PCM_IN|PCM_MONOTONIC,
                                     voice_config);
    if (session->pcm_voice_tx != NULL && !pcm_is_ready(session->pcm_voice_tx)) {
        ALOGE("%s: cannot open PCM voice TX stream: %s",
              __func__,
              pcm_get_error(session->pcm_voice_tx));

        pcm_close(session->pcm_voice_rx);
        session->pcm_voice_rx = NULL;

        return -ENOMEM;
    }

    pcm_start(session->pcm_voice_rx);
    pcm_start(session->pcm_voice_tx);

    if (session->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        start_voice_session_bt_sco(session);
    }

#ifdef AUDIENCE_EARSMART_IC
    ALOGV("%s: Enabling Audience IC", __func__);
    es_start_voice_session(session);
#endif

    if (session->two_mic_control) {
        ALOGV("%s: enabling two mic control", __func__);
        ril_set_two_mic_control(&session->ril, AUDIENCE, TWO_MIC_SOLUTION_ON);
    } else {
        ALOGV("%s: disabling two mic control", __func__);
        ril_set_two_mic_control(&session->ril, AUDIENCE, TWO_MIC_SOLUTION_OFF);
    }

    ril_set_call_clock_sync(&session->ril, SOUND_CLOCK_START);

    return 0;
}

/*
 * This function must be called with hw device mutex locked, OK to hold other
 * mutexes
 */
void stop_voice_session(struct voice_session *session)
{
    int status = 0;

    ALOGV("%s: Closing active PCMs", __func__);

    if (session->pcm_voice_rx != NULL) {
        pcm_stop(session->pcm_voice_rx);
        pcm_close(session->pcm_voice_rx);
        session->pcm_voice_rx = NULL;
        status++;
    }

    if (session->pcm_voice_tx != NULL) {
        pcm_stop(session->pcm_voice_tx);
        pcm_close(session->pcm_voice_tx);
        session->pcm_voice_tx = NULL;
        status++;
    }

    if (session->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        stop_voice_session_bt_sco(session);
    }

#ifdef AUDIENCE_EARSMART_IC
    ALOGV("%s: Disabling Audience IC", __func__);
    es_stop_voice_session();
#endif

    session->out_device = AUDIO_DEVICE_NONE;

    ALOGV("%s: Successfully closed %d active PCMs", __func__, status);
}

void set_voice_session_volume(struct voice_session *session, float volume)
{
    enum _SoundType sound_type;

    switch (session->out_device) {
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

    ril_set_call_volume(&session->ril, sound_type, volume);
}

void set_voice_session_mic_mute(struct voice_session *session, bool state)
{
    enum _MuteCondition mute_condition = state ? TX_MUTE : TX_UNMUTE;

    ril_set_mute(&session->ril, mute_condition);
}

bool voice_session_uses_twomic(struct voice_session *session)
{
    if (session->two_mic_disabled) {
        return false;
    }

    return session->two_mic_control;
}

bool voice_session_uses_wideband(struct voice_session *session)
{
    return session->wb_amr_type >= 1;
}

static void voice_session_wb_amr_callback(void *data, int wb_amr_type)
{
    struct audio_device *adev = (struct audio_device *)data;
    struct voice_session *session =
        (struct voice_session *)adev->voice.session;

    pthread_mutex_lock(&adev->lock);

    if (session->wb_amr_type != wb_amr_type) {
        session->wb_amr_type = wb_amr_type;

        /* reopen the modem PCMs at the new rate */
        if (adev->voice.in_call) {
            ALOGV("%s: %s wide band voice call (WB_AMR=%d)",
                  __func__,
                  wb_amr_type > 0 ? "Enable" : "Disable",
                  wb_amr_type);

            /* TODO Handle wb_amr_type=2 */

            /*
             * We need stop the PCM and start with the
             * wide band pcm_config.
             */
            stop_voice_call(adev);
            start_voice_call(adev);
        }
    }

    pthread_mutex_unlock(&adev->lock);
}

struct voice_session *voice_session_init(struct audio_device *adev)
{
    char voice_config[PROPERTY_VALUE_MAX];
    struct voice_session *session;
    int ret;

    session = calloc(1, sizeof(struct voice_session));
    if (session == NULL) {
        return NULL;
    }

    /* Two mic control */
    ret = property_get_bool("audio_hal.disable_two_mic", false);
    if (ret > 0) {
        session->two_mic_disabled = true;
    }

    /* Do this as the last step so we do not have to close it on error */
    ret = ril_open(&session->ril);
    if (ret != 0) {
        free(session);
        return NULL;
    }

    ret = property_get("audio_hal.force_voice_config", voice_config, "");
    if (ret > 0) {
        if ((strncmp(voice_config, "narrow", 6)) == 0)
            session->wb_amr_type = 0;
        else if ((strncmp(voice_config, "wide", 4)) == 0)
            session->wb_amr_type = 1;
        ALOGV("%s: Forcing voice config: %s", __func__, voice_config);
    } else {
        if (RIL_UNSOL_SNDMGR_WB_AMR_REPORT > 0) {
            /* register callback for wideband AMR setting */
            ret = ril_set_wb_amr_callback(&session->ril,
                                          voice_session_wb_amr_callback,
                                          (void *)adev);
            if (ret != 0) {
                ALOGE("%s: Failed to register WB_AMR callback", __func__);
                free(session);
                return NULL;
            }

            ALOGV("%s: Registered WB_AMR callback", __func__);
        } else {
            ALOGV("%s: WB_AMR callback not supported", __func__);
        }
    }

    return session;
}

void voice_session_deinit(struct voice_session *session)
{
    ril_close(&session->ril);
    free(session);
}
