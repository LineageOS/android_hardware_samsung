/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/config/SimSlotStatus.h>
#include <aidl/android/hardware/radio/config/SlotPortMapping.h>
#include <samsung_ril/structs.h>

#include <mutex>
#include <optional>
#include <vector>

namespace aidl::android::hardware::radio::samsung {

// On tsds2 devices the second tray and the eUICC share one SIM path, and libsec-ril only reports
// whichever is connected, as physical slot 1. The framework is shown three slots instead, with
// the disconnected one inactive, so it can switch between them itself:
//   framework: 0 = tray, 1 = eUICC, 2 = hybrid tray
//   libsec-ril slot mapping: 0 = tray, 1 = hybrid tray, 2 = eUICC
class HybridSlots {
  public:
    static HybridSlots& get();

    std::vector<config::SimSlotStatus> present(std::vector<config::SimSlotStatus> slots);
    std::vector<SecSlotMapping> toRil(const std::vector<config::SlotPortMapping>& mappings);
    int toFramework(int rilSlot);

  private:
    HybridSlots();

    bool enabled() const;
    void load();
    void save();

    bool mEnabled = false;
    std::mutex mMutex;
    bool mEuiccActive = false;
    std::optional<config::SimSlotStatus> mEuicc;
    std::optional<config::SimSlotStatus> mTray;
};

}  // namespace aidl::android::hardware::radio::samsung
