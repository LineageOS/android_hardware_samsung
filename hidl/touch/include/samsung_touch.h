/*
 * Copyright (C) 2021 The LineageOS Project
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

#ifndef SAMSUNG_TOUCH_H
#define SAMSUNG_TOUCH_H

/*
 * Board specific nodes
 *
 * If your kernel exposes these controls in another place, you can either
 * symlink to the locations given here, or override this header in your
 * device tree.
 */

// For GloveMode and StylusMode
#define TSP_CMD_LIST_NODE "/sys/class/sec/tsp/cmd_list"
#define TSP_CMD_RESULT_NODE "/sys/class/sec/tsp/cmd_result"
#define TSP_CMD_NODE "/sys/class/sec/tsp/cmd"

// For KeyDisabler
#define KEY_DISABLER_NODE "/sys/class/sec/sec_touchkey/input/enabled"

//For TouchscreenGesture
#define TOUCHSCREEN_GESTURE_NODE "/sys/class/sec/sec_epen/epen_gestures"

#endif  // SAMSUNG_TOUCH_H
