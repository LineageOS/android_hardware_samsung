/*
 * Copyright (C) 2013 The CyanogenMod Project
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

#define LOG_TAG "audio_hw_primary"
/*#define LOG_NDEBUG 0*/

#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include <utils/Log.h>
#include <cutils/properties.h>

#include "ril_interface.h"

#define VOLUME_STEPS_DEFAULT  "5"
#define VOLUME_STEPS_PROPERTY "ro.config.vc_call_vol_steps"

/* Audio WB AMR callback */
void (*_audio_set_wb_amr_callback)(void *, int);
void *callback_data = NULL;

void ril_register_set_wb_amr_callback(void *function, void *data)
{
    _audio_set_wb_amr_callback = function;
    callback_data = data;
}

/* This is the callback function that the RIL uses to
set the wideband AMR state */
static int ril_set_wb_amr_callback(void *ril_client __unused,
                                   const void *data,
                                   size_t datalen)
{
    int enable = ((int *)data)[0];

    if (callback_data == NULL || _audio_set_wb_amr_callback == NULL) {
        return -1;
    }

    _audio_set_wb_amr_callback(callback_data, enable);

    return 0;
}

static int ril_connect_if_required(struct ril_handle *ril)
{
    int ok;
    int rc;

    if (ril->client == NULL) {
        ALOGE("ril->client is NULL");
        return -1;
    }

    ok = isConnected_RILD(ril->client);
    if (ok) {
        return 0;
    }

    rc = Connect_RILD(ril->client);
    if (rc != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("Connect_RILD() failed");
        return -1;
    }

    return 0;
}

int ril_open(struct ril_handle *ril)
{
    char property[PROPERTY_VALUE_MAX];

    if (ril == NULL) {
        return -1;
    }

    ril->client = OpenClient_RILD();
    if (ril->client == NULL) {
        ALOGE("OpenClient_RILD() failed");
        return -1;
    }

    /* register the wideband AMR callback */
    RegisterUnsolicitedHandler(ril->client,
                               RIL_UNSOL_SNDMGR_WB_AMR_REPORT,
                               (RilOnUnsolicited)ril_set_wb_amr_callback);

    property_get(VOLUME_STEPS_PROPERTY, property, VOLUME_STEPS_DEFAULT);
    ril->volume_steps_max = atoi(property);

    /*
     * This catches the case where VOLUME_STEPS_PROPERTY does not contain
     * an integer
     */
    if (ril->volume_steps_max == 0) {
        ril->volume_steps_max = atoi(VOLUME_STEPS_DEFAULT);
    }

    return 0;
}

int ril_close(struct ril_handle *ril)
{
    int rc;

    if (ril == NULL || ril->client == NULL) {
        return -1;
    }

    rc = Disconnect_RILD(ril->client);
    if (rc != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("Disconnect_RILD failed");
        return -1;
    }

    rc = CloseClient_RILD(ril->client);
    if (rc != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("CloseClient_RILD() failed");
        return -1;
    }
    ril->client = NULL;

    return 0;
}

int ril_set_call_volume(struct ril_handle *ril,
                        enum _SoundType sound_type,
                        float volume)
{
    int rc;

    rc = ril_connect_if_required(ril);
    if (rc != 0) {
        return 0;
    }

    rc = SetCallVolume(ril->client,
                       sound_type,
                       (int)(volume * ril->volume_steps_max));

    return rc;
}

int ril_set_call_audio_path(struct ril_handle *ril, enum _AudioPath path)
{
    int rc;

    rc = ril_connect_if_required(ril);
    if (rc != 0) {
        return 0;
    }

    rc = SetCallAudioPath(ril->client, path);

    return rc;
}

int ril_set_call_clock_sync(struct ril_handle *ril,
                            enum _SoundClockCondition condition)
{
    int rc;

    rc = ril_connect_if_required(ril);
    if (rc != 0) {
        return 0;
    }

    rc = SetCallClockSync(ril->client, condition);

    return rc;
}

int ril_set_mute(struct ril_handle *ril, enum _MuteCondition condition)
{
    int rc;

    rc = ril_connect_if_required(ril);
    if (rc != 0) {
        return 0;
    }

    rc = SetMute(ril->client, condition);

    return rc;
}

int ril_set_two_mic_control(struct ril_handle *ril,
                            enum __TwoMicSolDevice device,
                            enum __TwoMicSolReport report)
{
    int rc;

    rc = ril_connect_if_required(ril);
    if (rc != 0) {
        return 0;
    }

    rc = SetTwoMicControl(ril->client, device, report);

    return rc;
}
