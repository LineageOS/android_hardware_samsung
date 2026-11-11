/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/config/SimSlotStatus.h>
#include <aidl/android/hardware/radio/config/SlotPortMapping.h>
#include <aidl/android/hardware/radio/sim/CardStatus.h>
#include <samsung_ril/structs.h>

#include <array>
#include <functional>
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
    using SlotStatusListener = std::function<void(const std::vector<config::SimSlotStatus>&)>;

    static HybridSlots& get();

    std::vector<config::SimSlotStatus> present(std::vector<config::SimSlotStatus> slots);
    std::vector<SecSlotMapping> toRil(const std::vector<config::SlotPortMapping>& mappings);
    int toFramework(int rilSlot);
    // libsec-ril's SIM low level control actions that connect the side the last mapping asked
    // for, on modems that report no slot status and reject slot mappings. Empty elsewhere.
    std::vector<uint8_t> switchActions();

    // Once libsec-ril has refused to report slot status, it is built from the card status of
    // each SIM instead: returned here, and passed to the listener whenever it changes. The probe
    // asks libsec-ril for slot status once both SIMs have reported, as the radio is up by then.
    std::optional<std::vector<config::SimSlotStatus>> slotStatusUnsupported();
    void slotStatusProbeInconclusive();
    void onCardStatus(int socket, const sim::CardStatus& status);
    void setCallbacks(SlotStatusListener listener, std::function<void()> probe);

  private:
    HybridSlots();

    bool enabled() const;
    std::vector<config::SimSlotStatus> presentLocked(std::vector<config::SimSlotStatus> slots,
                                                     bool euiccActive);
    std::optional<std::vector<config::SimSlotStatus>> cardSlotStatusLocked();
    void load();
    void save();

    bool mEnabled = false;
    std::mutex mMutex;
    bool mEuiccActive = false;
    bool mSlotStatusSeen = false;
    bool mSlotStatusUnsupported = false;
    std::optional<config::SimSlotStatus> mEuicc;
    std::optional<config::SimSlotStatus> mTray;
    std::array<std::optional<config::SimSlotStatus>, 2> mCardSlots;
    std::optional<std::vector<config::SimSlotStatus>> mCardSlotStatus;
    SlotStatusListener mListener;
    std::function<void()> mProbe;
    bool mProbing = false;
};

}  // namespace aidl::android::hardware::radio::samsung
