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

#ifndef SAMSUNG_BCTL_METADATA_H
#define SAMSUNG_BCTL_METADATA_H

#define BCTL_METADATA_PARTITION "/dev/block/bootdevice/by-name/slotinfo"
#define BCTL_METADATA_OFFSET 0x800
#define BCTL_METADATA_MAGIC 0x43425845

#define SLOT_SUFFIX_A "_a"
#define SLOT_SUFFIX_B "_b"

// Our internal data structures
struct slot_metadata_t {
    uint32_t magic;
    uint8_t bootable;
    uint8_t is_active;
    uint8_t boot_successful;
    uint8_t tries_remaining;
    uint8_t reserved[8];
};

struct bctl_metadata_t {
    slot_metadata_t slot_info[2];
};

#endif  // SAMSUNG_BCTL_METADATA_H
