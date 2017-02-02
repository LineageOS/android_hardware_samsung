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

struct voice_call {
    struct ril_handle ril;

    struct pcm *pcm_voice_rx;
    struct pcm *pcm_voice_tx;

    bool wb_amr;
    bool two_mic_control;
    bool two_mic_disabled;

    /* from uc_info */
    audio_devices_t out_device;
};

struct voice_call *voice_call_init(struct audio_device *adev);
void voice_call_deinit(struct voice_call *s);

void voice_call_set_volume(struct voice_call *vc, float volume);
int voice_call_start(struct voice_call *vc, struct audio_usecase *uc_info);
void voice_call_stop(struct voice_call *vc);

#endif /* VOICE_CALL_H */
