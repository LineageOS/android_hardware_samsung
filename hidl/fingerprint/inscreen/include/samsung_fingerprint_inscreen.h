/*
 * Copyright (C) 2020 The LineageOS Project
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

#ifndef SAMSUNG_FINGERPRINT_INSCREEN_H
#define SAMSUNG_FINGERPRINT_INSCREEN_H

#define FOD_SENSOR_X 435
#define FOD_SENSOR_Y 2025
#define FOD_SENSOR_SIZE 210

#define FOD_ENABLE "fod_enable,1,1"
#define FOD_DISABLE "fod_enable,0"

// #define FOD_SET_RECT "set_fod_rect,554,2263,886,2595"

#define FINGERPRINT_ACQUIRED_VENDOR 6
#define VENDORCODE_FINGER_DOWN 9002
#define VENDORCODE_FINGER_UP 9001

#define TSP_CMD_PATH "/sys/class/sec/tsp/cmd"

#endif  // SAMSUNG_FINGERPRINT_INSCREEN_H
