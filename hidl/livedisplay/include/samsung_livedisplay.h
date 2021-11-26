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

#ifndef SAMSUNG_LIVEDISPLAY_H
#define SAMSUNG_LIVEDISPLAY_H

/*
 * Board specific nodes
 *
 * If your kernel exposes these controls in another place, you can either
 * symlink to the locations given here, or override this header in your
 * device tree.
 */

// For AdaptiveBacklight
#define LCD_PANEL_POWER_REDUCE_NODE "/sys/class/lcd/panel/power_reduce"

// For DisplayColorCalibration
#define RGB_NODE "/sys/class/graphics/fb0/rgb"

// For DisplayColorCalibration Exynos
#define RGB_EXYNOS_NODE "/sys/class/mdnie/mdnie/sensorRGB"

// For DisplayModes
#define MDNIE_MODE_NODE "/sys/class/mdnie/mdnie/mode";
#define MDNIE_MODE_MAX_NODE "/sys/class/mdnie/mdnie/mode_max";

// For ReadingEnhancement
#define MDNIE_ACCESSIBILITY_NODE "/sys/class/mdnie/mdnie/accessibility"

// For SunlightEnhancement
#define PANEL_AUTO_BRIGHTNESS_NODE "/sys/class/lcd/panel/panel/auto_brightness"
#define MDNIE_OUTDOOR_NODE "/sys/class/mdnie/mdnie/outdoor"

// For SunlightEnhancement Exynos
#define MDNIE_LUX_NODE "/sys/class/mdnie/mdnie/lux"

#endif  // SAMSUNG_LIVEDISPLAY_H
