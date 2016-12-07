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

#ifndef SAMSUNG_POWER_H
#define SAMSUNG_POWER_H

/*
 * Board specific nodes
 *
 * If your kernel exposes these controls in another place, you can either
 * symlink to the locations given here, or override this header in your
 * device tree.
 */
#define BOOSTPULSE_PATH "/sys/devices/system/cpu/cpu0/cpufreq/interactive/boostpulse"

#define IO_IS_BUSY_PATH "/sys/devices/system/cpu/cpu0/cpufreq/interactive/io_is_busy"

#define CPU0_HISPEED_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/interactive/hispeed_freq"
#define CPU0_MAX_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define CPU4_HISPEED_FREQ_PATH "/sys/devices/system/cpu/cpu4/cpufreq/interactive/hispeed_freq"
#define CPU4_MAX_FREQ_PATH "/sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq"

#endif // SAMSUNG_POWER_H
