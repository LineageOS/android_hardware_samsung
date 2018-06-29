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

#ifndef BT_SCO_H
#define BT_SCO_H

#include <samsung_audio.h>
#include "audio_hw.h"

void start_bt_sco(struct audio_device *adev);
void stop_bt_sco(struct audio_device *adev);
void audio_bt_sco_set_parameters(struct audio_device *adev, struct str_parms *parms);


#endif /* BT_SCO_H */

