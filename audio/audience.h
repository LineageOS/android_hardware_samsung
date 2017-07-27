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

#include "audio_hw.h"
#include "voice.h"

enum es_power_state {
    ES_POWER_FW_LOAD,
    ES_POWER_SLEEP,
    ES_POWER_SLEEP_PENDING,
    ES_POWER_AWAKE,
    ES_MAX = ES_POWER_AWAKE
};

int es_start_voice_session(struct voice_session *session);
void es_stop_voice_session();
