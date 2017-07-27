/*
 * Copyright (C) 2016 The CyanogenMod Project
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

#ifndef SAMSUNG_LIGHTS_HELPER_H
#define SAMSUNG_LIGHTS_HELPER_H

#include <samsung_lights.h>

/*
 * Interfaces for other modules accessing lights HAL data.
 * For documentation, see lights_helper.c
 */
extern int set_cur_button_brightness(const int brightness);
extern int get_cur_panel_brightness();
extern int get_max_panel_brightness();
extern int set_cur_panel_brightness(const int brightness);
extern int set_max_panel_brightness(const int brightness);

#endif // SAMSUNG_LIGHTS_HELPER_H
