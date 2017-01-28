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

#ifndef RIL_INTERFACE_H
#define RIL_INTERFACE_H

#include <telephony/ril.h>
#include "secril-client.h"

struct ril_handle
{
    void *client;
    int volume_steps_max;
};


/* Function prototypes */
int ril_open(struct ril_handle *ril);

int ril_close(struct ril_handle *ril);

int ril_set_call_volume(struct ril_handle *ril,
                        enum _SoundType sound_type,
                        float volume);

int ril_set_call_audio_path(struct ril_handle *ril,
                            enum _AudioPath path);

int ril_set_call_clock_sync(struct ril_handle *ril,
                            enum _SoundClockCondition condition);

int ril_set_mute(struct ril_handle *ril, enum _MuteCondition condition);

int ril_set_two_mic_control(struct ril_handle *ril,
                            enum __TwoMicSolDevice device,
                            enum __TwoMicSolReport report);

void ril_register_set_wb_amr_callback(void *function, void *data);

#endif
