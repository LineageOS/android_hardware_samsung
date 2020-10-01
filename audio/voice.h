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

#ifndef VOICE_CALL_H
#define VOICE_CALL_H

#include "ril_interface.h"

struct voice_session {
    struct ril_handle ril;

    struct pcm *pcm_voice_rx;
    struct pcm *pcm_voice_tx;

    int wb_amr_type;
    bool two_mic_control;
    bool two_mic_disabled;

    /* from uc_info */
    audio_devices_t out_device;

    /* parent container */
    struct voice_data *vdata;
};

void prepare_voice_session(struct voice_session *session,
                           audio_devices_t active_out_devices);
int start_voice_session(struct voice_session *session);
void stop_voice_session(struct voice_session *session);
void set_voice_session_volume(struct voice_session *session, float volume);
void set_voice_session_audio_path(struct voice_session *session);
void set_voice_session_mic_mute(struct voice_session *session, bool state);

void start_voice_session_bt_sco(struct audio_device *adev);
void stop_voice_session_bt_sco(struct audio_device *adev);

bool voice_session_uses_twomic(struct voice_session *session);
bool voice_session_uses_wideband(struct voice_session *session);

struct voice_session *voice_session_init(struct audio_device *adev);
void voice_session_deinit(struct voice_session *s);

#endif /* VOICE_CALL_H */
