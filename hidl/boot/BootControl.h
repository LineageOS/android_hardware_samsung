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

#ifndef ANDROID_HARDWARE_BOOT_V1_0_BOOTCONTROL_H
#define ANDROID_HARDWARE_BOOT_V1_0_BOOTCONTROL_H

#include <android-base/properties.h>
#include <android/hardware/boot/1.0/IBootControl.h>

#include "bctl_metadata.h"

namespace android {
namespace hardware {
namespace boot {
namespace V1_0 {
namespace implementation {

using ::android::base::GetProperty;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::boot::V1_0::BoolResult;
using ::android::hardware::boot::V1_0::CommandResult;

class BootControl : public IBootControl {
  public:
    // Methods from ::android::hardware::boot::V1_0::IBootControl follow.
    Return<uint32_t> getNumberSlots() override;
    Return<uint32_t> getCurrentSlot() override;
    Return<void> markBootSuccessful(markBootSuccessful_cb _hidl_cb) override;
    Return<void> setActiveBootSlot(uint32_t slot, setActiveBootSlot_cb _hidl_cb) override;
    Return<void> setSlotAsUnbootable(uint32_t slot, setSlotAsUnbootable_cb _hidl_cb) override;
    Return<BoolResult> isSlotBootable(uint32_t slot) override;
    Return<BoolResult> isSlotMarkedSuccessful(uint32_t slot) override;
    Return<void> getSuffix(uint32_t slot, getSuffix_cb _hidl_cb) override;

  private:
    std::string mBlkDevice = BCTL_METADATA_PARTITION;

    bool readMetadata(bctl_metadata_t& data);
    bool writeMetadata(bctl_metadata_t& data);
    bool validateMetadata(bctl_metadata_t& data);
    bctl_metadata_t resetMetadata();
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace boot
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_BOOT_V1_0_BOOTCONTROL_H
