/* tools/mkbootimg/samsung_dtbh.h
**
** Copyright 2016, The LineageOS Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef _SAMSUNG_DTBH_H_
#define _SAMSUNG_DTBH_H_

/*
 * This is an example of how this header should look like.
 * You need to extract the values for your target manually.
 */

#if 0 // DUMMY
#define DTBH_MAGIC         "DTBH"
#define DTBH_VERSION       2
#define DTBH_PLATFORM      "k3g"
#define DTBH_SUBTYPE       "k3g_eur_open"
/* Hardcoded entry */
#define DTBH_PLATFORM_CODE 0x1e92
#define DTBH_SUBTYPE_CODE  0x7d64f612

/* DTBH_MAGIC + DTBH_VERSION + DTB counts */
#define DT_HEADER_PHYS_SIZE 12

/*
 * keep the eight uint32_t entries first in this struct so we can memcpy them to the file
 */
#define DT_ENTRY_PHYS_SIZE (sizeof(uint32_t) * 8)
#endif // DUMMY

#endif // _SAMSUNG_DTBH_H_
