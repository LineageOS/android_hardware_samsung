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
// libsec-ril's SIM low level control actions.
constexpr uint8_t kSwitchStart = 0x21;
constexpr uint8_t kSwitchToEuicc = 0x10;
constexpr uint8_t kSwitchToTray = 0x11;
constexpr uint8_t kSwitchFinish = 0x20;

// The framework only recognises an inactive slot as an eUICC by its ATR, and only offers an
// inactive SIM whose ICCID a slot reports.
constexpr char kCache[] = "/data/vendor/secradio/hybrid_slots";

// libsec-ril copies this into persist.ril.esim.slotswitch, but on a fresh /data only after it has
// reported the first slot status.
constexpr char kFloatingFeature[] = "/vendor/etc/floating_feature.xml";
constexpr char kTsds2Feature[] =
        "<SEC_FLOATING_FEATURE_COMMON_CONFIG_EMBEDDED_SIM_SLOTSWITCH>tsds2</";

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

// libsec-ril records which side of the hybrid path is connected.
bool euiccConnected() {
    return ::android::base::GetProperty("ril.simslottype2", "0") == "1";
}

}  // namespace

HybridSlots& HybridSlots::get() {
    static HybridSlots instance;
    return instance;
}

HybridSlots::HybridSlots() {
    std::string features;
    mEnabled = ::android::base::ReadFileToString(kFloatingFeature, &features)
                       ? features.find(kTsds2Feature) != std::string::npos
                       : ::android::base::GetProperty("persist.ril.esim.slotswitch", "") == "tsds2";
    mEuiccActive = euiccConnected();
    load();
}

bool HybridSlots::enabled() const {
    return mEnabled;
}

std::vector<config::SimSlotStatus> HybridSlots::present(std::vector<config::SimSlotStatus> slots) {
    if (!enabled() || slots.size() != 2) {
        return slots;
    }
    std::lock_guard lock(mMutex);
    mSlotStatusSeen = true;
    const auto& hybrid = slots[kRilHybridSlot];
    LOG(DEBUG) << "Hybrid slot: card " << hybrid.cardState << ", " << hybrid.portInfo.size()
               << " ports, eid " << (hybrid.eid.empty() ? "none" : "set");
    const bool euiccActive =
            isEuicc(hybrid) || (mEuicc && !hybrid.atr.empty() && hybrid.atr == mEuicc->atr);
    return presentLocked(std::move(slots), euiccActive);
}

std::vector<config::SimSlotStatus> HybridSlots::presentLocked(
        std::vector<config::SimSlotStatus> slots, bool euiccActive) {
    const auto& hybrid = slots[kRilHybridSlot];
    mEuiccActive = euiccActive;
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
    // For MEP-A the framework shifts ports in slot mappings by one while no card status has
    // arrived for the eUICC, but libsec-ril counts them from 0 as for MEP-B.
    if (slots[kEuicc].supportedMepMode != config::MultipleEnabledProfilesMode::NONE) {
        slots[kEuicc].supportedMepMode = config::MultipleEnabledProfilesMode::MEP_B;
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
    // Some modems never report slot status, but libsec-ril records which side is connected.
    if (!mSlotStatusSeen) {
        return euiccConnected() ? kEuicc : kHybridTray;
    }
    return mEuiccActive ? kEuicc : kHybridTray;
}

std::vector<uint8_t> HybridSlots::switchActions() {
    if (!enabled()) {
        return {};
    }
    std::lock_guard lock(mMutex);
    if (mSlotStatusSeen) {
        return {};
    }
    return {kSwitchStart, mEuiccActive ? kSwitchToEuicc : kSwitchToTray, kSwitchFinish};
}

std::optional<std::vector<config::SimSlotStatus>> HybridSlots::slotStatusUnsupported() {
    if (!enabled()) {
        return std::nullopt;
    }
    std::lock_guard lock(mMutex);
    if (mSlotStatusSeen) {
        return std::nullopt;
    }
    mSlotStatusUnsupported = true;
    mProbing = false;
    if (auto slots = cardSlotStatusLocked()) {
        mCardSlotStatus = slots;
    }
    return mCardSlotStatus;
}

void HybridSlots::slotStatusProbeInconclusive() {
    std::lock_guard lock(mMutex);
    mProbing = false;
}

void HybridSlots::onCardStatus(int socket, const sim::CardStatus& status) {
    if (!enabled() || socket < 0 || socket >= static_cast<int>(mCardSlots.size())) {
        return;
    }
    std::optional<std::vector<config::SimSlotStatus>> slots;
    SlotStatusListener listener;
    std::function<void()> probe;
    {
        std::lock_guard lock(mMutex);
        if (mSlotStatusSeen) {
            return;
        }
        mCardSlots[socket] = config::SimSlotStatus{
                .cardState = status.cardState,
                .atr = status.atr,
                .eid = status.eid,
                .portInfo = {{.iccId = status.iccid, .logicalSlotId = socket, .portActive = true}},
        };
        if (!mSlotStatusUnsupported) {
            if (!mProbing && mCardSlots[0] && mCardSlots[1]) {
                mProbing = true;
                probe = mProbe;
            }
        } else if (slots = cardSlotStatusLocked(); slots && slots != mCardSlotStatus) {
            mCardSlotStatus = slots;
            listener = mListener;
        }
    }
    if (probe) {
        probe();
    }
    if (listener && slots) {
        listener(*slots);
    }
}

std::optional<std::vector<config::SimSlotStatus>> HybridSlots::cardSlotStatusLocked() {
    // A slot reported absent would take the other SIM away from the framework.
    if (!mSlotStatusUnsupported || !mCardSlots[0] || !mCardSlots[1]) {
        return std::nullopt;
    }
    return presentLocked({*mCardSlots[0], *mCardSlots[1]}, euiccConnected());
}

void HybridSlots::setCallbacks(SlotStatusListener listener, std::function<void()> probe) {
    std::lock_guard lock(mMutex);
    mListener = std::move(listener);
    mProbe = std::move(probe);
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
