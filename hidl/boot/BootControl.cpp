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

#include "BootControl.h"

#include <fstream>

namespace android {
namespace hardware {
namespace boot {
namespace V1_0 {
namespace implementation {

bool BootControl::readMetadata(bctl_metadata_t& data) {
    std::fstream in(mBlkDevice, std::ios::binary | std::ios::in);

    if (in.fail()) {
        return false;
    }

    in.seekg(BCTL_METADATA_OFFSET);

    if (in.fail()) {
        return false;
    }

    in.read(reinterpret_cast<char*>(&data), sizeof(bctl_metadata_t));

    if (!validateMetadata(data)) {
#if 0
        /* this is what samsung does, probably not what we desire */
        data = resetMetadata();
        if (validateMetadata(data)) {
            return true;
        }
#endif
        return false;
    }

    return !in.eof() && !in.fail();
}

bool BootControl::writeMetadata(bctl_metadata_t& data) {
    if (!validateMetadata(data)) return false;

    // We use std::ios::in | std::ios::out even though we only write so that
    // we don't truncate or append to the file, but rather overwrite the file
    // in the exact place that we want to write the struct to.
    std::fstream out(mBlkDevice, std::ios::binary | std::ios::in | std::ios::out);

    if (out.fail()) {
        return false;
    }

    out.seekp(BCTL_METADATA_OFFSET);

    if (out.fail()) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(&data), sizeof(bctl_metadata_t));

    return !out.eof() && !out.fail();
}

bool BootControl::validateMetadata(bctl_metadata_t& data) {
    if (data.slot_info[0].magic != BCTL_METADATA_MAGIC ||
        data.slot_info[1].magic != BCTL_METADATA_MAGIC) {
        return false;
    }

    return true;
}

bctl_metadata_t BootControl::resetMetadata() {
    bctl_metadata_t data{};

    // reset to defaults
    data.slot_info[0].magic = BCTL_METADATA_MAGIC;
    data.slot_info[0].bootable = 1;
    data.slot_info[0].is_active = 1;
    data.slot_info[0].boot_successful = 0;
    data.slot_info[0].tries_remaining = 7;

    data.slot_info[1].magic = BCTL_METADATA_MAGIC;
    data.slot_info[1].bootable = 1;
    data.slot_info[1].is_active = 0;
    data.slot_info[1].boot_successful = 0;
    data.slot_info[1].tries_remaining = 7;

    return data;
}

// Methods from ::android::hardware::boot::V1_0::IBootControl follow.
Return<uint32_t> BootControl::getNumberSlots() {
    return 2;
}

Return<uint32_t> BootControl::getCurrentSlot() {
    bctl_metadata_t data;
    std::string slot_suffix = GetProperty("ro.boot.slot_suffix", "");

    if (!slot_suffix.empty()) {
        if (slot_suffix.compare(SLOT_SUFFIX_A)) {
            return 0;
        } else if (slot_suffix.compare(SLOT_SUFFIX_B)) {
            return 1;
        }
    } else {
        // read current slot from metadata incase "ro.boot.slot_suffix" is empty
        if (readMetadata(data)) {
            // This is a clever hack because if slot b is active,
            // is_active will be 0 and if slot a is active, is_active
            // will be 1. In other words, the "not" value of slot A's
            // is_active var lines up to the current active slot index.
            return !data.slot_info[0].is_active;
        }
    }

    // fallback to slot A
    return 0;
}

Return<void> BootControl::markBootSuccessful(markBootSuccessful_cb _hidl_cb) {
    bctl_metadata_t data;
    bool success = false;
    int active_slot = getCurrentSlot();

    if (readMetadata(data)) {
        data.slot_info[active_slot].boot_successful = 1;
        data.slot_info[active_slot].tries_remaining = 0;

        if (success)
            if (writeMetadata(data)) {
                _hidl_cb(CommandResult{true, ""});
            } else {
                _hidl_cb(CommandResult{false, "Failed to write metadata"});
            }
        else {
            _hidl_cb(CommandResult{false, "No active slot"});
        }
    } else {
        _hidl_cb(CommandResult{false, "Failed to read metadata"});
    }

    return Void();
}

Return<void> BootControl::setActiveBootSlot(uint32_t slot, setActiveBootSlot_cb _hidl_cb) {
    bctl_metadata_t data;

    if (slot < 2) {
        if (readMetadata(data)) {
            data.slot_info[slot].bootable = 1;
            data.slot_info[slot].is_active = 1;
            data.slot_info[slot].boot_successful = 0;
            data.slot_info[slot].tries_remaining = 7;

            data.slot_info[!slot].bootable = 1;
            data.slot_info[!slot].is_active = 0;
            data.slot_info[!slot].boot_successful = 0;
            data.slot_info[!slot].tries_remaining = 7;

            if (writeMetadata(data)) {
                _hidl_cb(CommandResult{true, ""});
            } else {
                _hidl_cb(CommandResult{false, "Failed to write metadata"});
            }
        } else {
            _hidl_cb(CommandResult{false, "Failed to read metadata"});
        }
    } else {
        _hidl_cb(CommandResult{false, "Invalid slot"});
    }

    return Void();
}

Return<void> BootControl::setSlotAsUnbootable(uint32_t slot, setSlotAsUnbootable_cb _hidl_cb) {
    bctl_metadata_t data;

    if (slot < 2) {
        if (readMetadata(data)) {
            data.slot_info[slot].bootable = 0;

            if (writeMetadata(data)) {
                _hidl_cb(CommandResult{true, ""});
            } else {
                _hidl_cb(CommandResult{false, "Failed to write metadata"});
            }
        } else {
            _hidl_cb(CommandResult{false, "Failed to read metadata"});
        }
    } else {
        _hidl_cb(CommandResult{false, "Invalid slot"});
    }

    return Void();
}

Return<BoolResult> BootControl::isSlotBootable(uint32_t slot) {
    bctl_metadata_t data;
    BoolResult ret = BoolResult::FALSE;

    if (slot < 2) {
        if (readMetadata(data)) {
            ret = static_cast<BoolResult>(data.slot_info[slot].bootable);
        } else {
            ret = BoolResult::FALSE;
        }
    } else {
        ret = BoolResult::INVALID_SLOT;
    }

    return ret;
}

Return<BoolResult> BootControl::isSlotMarkedSuccessful(uint32_t slot) {
    bctl_metadata_t data;
    BoolResult ret = BoolResult::FALSE;

    if (slot < 2) {
        if (readMetadata(data)) {
            ret = static_cast<BoolResult>(data.slot_info[slot].boot_successful);
        } else {
            ret = BoolResult::FALSE;
        }
    } else {
        ret = BoolResult::INVALID_SLOT;
    }

    return ret;
}

Return<void> BootControl::getSuffix(uint32_t slot, getSuffix_cb _hidl_cb) {
    if (slot < 2) {
        if (slot == 0) {
            _hidl_cb(SLOT_SUFFIX_A);
        } else {
            _hidl_cb(SLOT_SUFFIX_B);
        }
    } else {
        // default to slot A
        _hidl_cb(SLOT_SUFFIX_A);
    }

    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace boot
}  // namespace hardware
}  // namespace android
