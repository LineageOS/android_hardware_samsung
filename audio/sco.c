/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2017 Christopher N. Hesse <raymanfx@gmail.com>
 * Copyright (C) 2017 Andreas Schneider <asn@cryptomilk.org>
 * Copyright (C) 2018 The LineageOS Project
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

#define LOG_TAG "audio_hw_sco"
/*#define LOG_NDEBUG 0*/
/*#define VERY_VERY_VERBOSE_LOGGING*/
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>

#include <samsung_audio.h>

#include "audio_hw.h"

#define AUDIO_PARAMETER_BT_SCO      "BT_SCO"
#define AUDIO_PARAMETER_HFP_ENABLE      "hfp_enable"

struct pcm_config pcm_config_sco = {
    .channels = 1,
    .rate = SCO_DEFAULT_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_sco_wb = {
    .channels = 1,
    .rate = SCO_WB_SAMPLING_RATE,
    .period_size = SCO_PERIOD_SIZE,
    .period_count = SCO_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};


/*
 * This must be called with the hw device mutex locked, OK to hold other
 * mutexes.
 */
void stop_bt_sco(struct audio_device *adev) {
    ALOGV("%s: Closing SCO PCMs", __func__);

    if (adev->pcm_sco_rx != NULL) {
        pcm_stop(adev->pcm_sco_rx);
        pcm_close(adev->pcm_sco_rx);
        adev->pcm_sco_rx = NULL;
    }

    if (adev->pcm_sco_tx != NULL) {
        pcm_stop(adev->pcm_sco_tx);
        pcm_close(adev->pcm_sco_tx);
        adev->pcm_sco_tx = NULL;
    }
}

/* must be called with the hw device mutex locked, OK to hold other mutexes */
void start_bt_sco(struct audio_device *adev)
{
    struct pcm_config *sco_config;

    if (adev->pcm_sco_rx != NULL || adev->pcm_sco_tx != NULL) {
        ALOGW("%s: SCO PCMs already open!\n", __func__);
        return;
    }

    ALOGV("%s: Opening SCO PCMs", __func__);

    if (adev->voice.bluetooth_wb) {
        ALOGV("%s: pcm_config wideband", __func__);
        sco_config = &pcm_config_sco_wb;
    } else {
        ALOGV("%s: pcm_config narrowband", __func__);
        sco_config = &pcm_config_sco;
    }

    adev->pcm_sco_rx = pcm_open(SOUND_CARD,
                                   SOUND_PLAYBACK_SCO_DEVICE,
                                   PCM_OUT|PCM_MONOTONIC,
                                   sco_config);
    if (adev->pcm_sco_rx != NULL && !pcm_is_ready(adev->pcm_sco_rx)) {
        ALOGE("%s: cannot open PCM SCO RX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_rx));
        goto err_sco_rx;
    }

    adev->pcm_sco_tx = pcm_open(SOUND_CARD,
                                   SOUND_CAPTURE_SCO_DEVICE,
                                   PCM_IN|PCM_MONOTONIC,
                                   sco_config);
    if (adev->pcm_sco_tx && !pcm_is_ready(adev->pcm_sco_tx)) {
        ALOGE("%s: cannot open PCM SCO TX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_tx));
        goto err_sco_tx;
    }

    pcm_start(adev->pcm_sco_rx);
    pcm_start(adev->pcm_sco_tx);

    return;

err_sco_tx:
    pcm_close(adev->pcm_sco_tx);
    adev->pcm_sco_tx = NULL;
err_sco_rx:
    pcm_close(adev->pcm_sco_rx);
    adev->pcm_sco_rx = NULL;

}

void audio_bt_sco_set_parameters(struct audio_device *adev, struct str_parms *parms)
{
    int ret;
    char value[32]={0};

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_BT_SCO, value,
                            sizeof(value));
    if (ret >= 0) {
           if(!strncmp(value,"on",sizeof(value)))
               start_bt_sco(adev);
           else
               stop_bt_sco(adev);
    }

    memset(value, 0, sizeof(value));
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_HFP_ENABLE, value,
                            sizeof(value));
    if (ret >= 0) {
           ALOGV("%s: found hfp_enable parameter.", __func__);
    }


}
