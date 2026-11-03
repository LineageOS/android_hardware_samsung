/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungHybridSlots"

#include "config/HybridSlots.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace aidl::android::hardware::radio::samsung {

namespace {

constexpr int kEuicc = 1;
constexpr int kHybridTray = 2;
constexpr int kRilHybridSlot = 1;
constexpr int kRilHybridTray = 1;
constexpr int kRilEuicc = 2;

// The framework only recognises an inactive slot as an eUICC by its ATR, and only offers an
// inactive SIM whose ICCID a slot reports.
constexpr char kCache[] = "/data/vendor/secradio/hybrid_slots";

config::SimSlotStatus inactive(config::SimSlotStatus slot) {
    for (auto& port : slot.portInfo) {
        port.logicalSlotId = -1;
        port.portActive = false;
    }
    return slot;
}

config::SimSlotStatus absent() {
    return {
            .cardState = static_cast<int32_t>(RIL_CARDSTATE_ABSENT),
            .portInfo = {{.logicalSlotId = -1, .portActive = false}},
    };
}

bool isEuicc(const config::SimSlotStatus& slot) {
    return !slot.eid.empty() || slot.portInfo.size() > 1;
}

}  // namespace

HybridSlots& HybridSlots::get() {
    static HybridSlots instance;
    return instance;
}

HybridSlots::HybridSlots() {
    mEuiccActive = ::android::base::GetProperty("ril.simslottype2", "0") == "1";
    load();
}

bool HybridSlots::enabled() const {
    return ::android::base::GetProperty("persist.ril.esim.slotswitch", "") == "tsds2";
}

std::vector<config::SimSlotStatus> HybridSlots::present(std::vector<config::SimSlotStatus> slots) {
    if (!enabled() || slots.size() != 2) {
        return slots;
    }
    std::lock_guard lock(mMutex);
    const auto& hybrid = slots[kRilHybridSlot];
    LOG(DEBUG) << "Hybrid slot: card " << hybrid.cardState << ", " << hybrid.portInfo.size()
               << " ports, eid " << (hybrid.eid.empty() ? "none" : "set");
    mEuiccActive = isEuicc(hybrid) || (mEuicc && !hybrid.atr.empty() && hybrid.atr == mEuicc->atr);
    auto& cached = mEuiccActive ? mEuicc : mTray;
    if (const auto seen = inactive(hybrid); cached != seen) {
        cached = seen;
        save();
    }
    if (mEuiccActive) {
        slots.push_back(mTray.value_or(absent()));
    } else {
        slots.push_back(hybrid);
        // An eUICC not seen yet is reported once it has been switched in.
        slots[kEuicc] = mEuicc.value_or(absent());
    }
    return slots;
}

std::vector<SecSlotMapping> HybridSlots::toRil(
        const std::vector<config::SlotPortMapping>& mappings) {
    std::vector<SecSlotMapping> ril;
    for (const auto& mapping : mappings) {
        ril.push_back({mapping.physicalSlotId, mapping.portId});
    }
    if (!enabled()) {
        // Slot status reports the eUICC as slot 1, but mappings that use its second port
        // address it as slot 2.
        const bool usesPort1 = std::any_of(ril.begin(), ril.end(), [](const auto& mapping) {
            return mapping.physicalSlot == kRilHybridSlot && mapping.port == 1;
        });
        for (auto& mapping : ril) {
            if (usesPort1 && mapping.physicalSlot == kRilHybridSlot) {
                mapping.physicalSlot = kRilEuicc;
            }
        }
        return ril;
    }
    std::lock_guard lock(mMutex);
    for (auto& mapping : ril) {
        // Card status for the switched slot can arrive before the new slot status.
        if (mapping.physicalSlot == kEuicc) {
            mapping.physicalSlot = kRilEuicc;
            mEuiccActive = true;
        } else if (mapping.physicalSlot == kHybridTray) {
            mapping.physicalSlot = kRilHybridTray;
            mEuiccActive = false;
        }
    }
    return ril;
}

int HybridSlots::toFramework(int rilSlot) {
    if (rilSlot != kRilHybridSlot || !enabled()) {
        return rilSlot;
    }
    std::lock_guard lock(mMutex);
    return mEuiccActive ? kEuicc : kHybridTray;
}

// Lines: eUICC ATR, EID, port count and MEP mode, then hybrid tray card state and ICCID.
void HybridSlots::load() {
    std::string content;
    if (!::android::base::ReadFileToString(kCache, &content)) {
        return;
    }
    const auto lines = ::android::base::Split(content, "\n");
    if (lines.size() < 6) {
        return;
    }
    if (!lines[0].empty()) {
        mEuicc = {
                .cardState = static_cast<int32_t>(RIL_CARDSTATE_PRESENT),
                .atr = lines[0],
                .eid = lines[1],
                .supportedMepMode = static_cast<config::MultipleEnabledProfilesMode>(
                        std::atoi(lines[3].c_str())),
        };
        mEuicc->portInfo.resize(std::clamp(std::atoi(lines[2].c_str()), 1, kSecMaxPorts),
                                {.logicalSlotId = -1, .portActive = false});
    }
    if (!lines[4].empty()) {
        mTray = absent();
        mTray->cardState = std::atoi(lines[4].c_str());
        mTray->portInfo[0].iccId = lines[5];
    }
}

void HybridSlots::save() {
    std::vector<std::string> lines(6);
    if (mEuicc) {
        lines[0] = mEuicc->atr;
        lines[1] = mEuicc->eid;
        lines[2] = std::to_string(mEuicc->portInfo.size());
        lines[3] = std::to_string(static_cast<int>(mEuicc->supportedMepMode));
    }
    if (mTray) {
        lines[4] = std::to_string(mTray->cardState);
        lines[5] = mTray->portInfo.empty() ? "" : mTray->portInfo[0].iccId;
    }
    if (!::android::base::WriteStringToFile(::android::base::Join(lines, '\n'), kCache)) {
        PLOG(ERROR) << "Failed to write " << kCache;
    }
}

}  // namespace aidl::android::hardware::radio::samsung
