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

/*
 * NOTICE
 *
 * This must be kept in sync with the kernel API, for exmaple
 * "es705-routes.h" for the Galaxy Note 5 (N920T) with its ES804 IC.
 */

#define SYSFS_PATH_POWERCTRL		"/sys/class/earsmart/control/power_control_set"
#define SYSFS_PATH_PRESET		"/sys/class/earsmart/control/route_value"
#define SYSFS_PATH_VEQ			"/sys/class/earsmart/control/veq_control_set"
#define SYSFS_PATH_EXTRAVOLUME		"/sys/class/earsmart/control/extra_volume"
#define Call_HS_NB			0  /* Call, Headset, Narrow Band */
#define Call_FT_NB			1  /* Call, Far Talk, Narrow Band */
#define Call_CT_NB			2  /* Call, Close Talk, Narrow Band */
#define Call_FT_NB_NR_OFF		3  /* Call, Far Talk, NB, NR off */
#define Call_CT_NB_NR_OFF		4  /* Call, Close Talk, NB, NR off */
#define Call_BT_NB			10 /* Call, BT, NB */
#define Call_TTY_VCO			11 /* Call, TTY HCO NB */
#define Call_TTY_HCO			12 /* Call, TTY VCO NB */
#define Call_TTY_FULL			13 /* Call, TTY FULL NB */
#define Call_FT_EVS			14 /* Call, Far Talk, EVS */
#define Call_CT_EVS			15 /* Call, Close Talk, EVS */
#define LOOPBACK_CT			17 /* Loopback, Close Talk */
#define LOOPBACK_FT			18 /* Loopback, Far Talk */
#define LOOPBACK_HS			19 /* Loopback, Headset */
#define Call_BT_WB			20 /* Call, BT, WB */
#define Call_HS_WB			21 /* Call, Headset, Wide Band */
#define Call_FT_WB			22 /* Call, Far Talk, Wide Band */
#define Call_CT_WB			23 /* Call, Close Talk, Wide Band */
#define Call_FT_WB_NR_OFF		24 /* Call, Far Talk, WB, NR off */
#define Call_CT_WB_NR_OFF		25 /* Call, Close Talk, WB, NR off */
#define AUDIENCE_SLEEP			40 /* Route none, Audience Sleep State */
