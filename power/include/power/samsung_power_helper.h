/*
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

#ifndef SAMSUNG_POWER_HELPER_H
#define SAMSUNG_POWER_HELPER_H 1

#include <samsung_power.h>

#define BOOST_PATH        "/boost"
#define BOOSTPULSE_PATH   "/boostpulse"

#define IO_IS_BUSY_PATH   "/io_is_busy"
#define HISPEED_FREQ_PATH "/hispeed_freq"

#define MAX_FREQ_PATH     "/cpufreq/scaling_max_freq"

#define ARRAY_SIZE(a)     sizeof(a) / sizeof(a[0])

#define CLUSTER_COUNT     ARRAY_SIZE(CPU_SYSFS_PATHS)
#define PARAM_MAXLEN      10

/*
 * Interfaces for other modules accessing power HAL data.
 * For documentation, see lights_helper.c
 */
extern int sysfs_read(char *path, char *s, int num_bytes);
extern void sysfs_write(const char *path, char *s);
extern void cpu_sysfs_read(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN]);
extern void cpu_sysfs_write(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN]);
extern void cpu_interactive_read(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN]);
extern void cpu_interactive_write(const char *param, char s[CLUSTER_COUNT][PARAM_MAXLEN]);
extern void send_boost(int boost_fd, int32_t duration_us);
extern void send_boostpulse(int boostpulse_fd);

#endif // SAMSUNG_POWER_HELPER_H
