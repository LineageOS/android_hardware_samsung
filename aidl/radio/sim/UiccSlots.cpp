/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungUiccSlots"

#include "sim/UiccSlots.h"

#include <android-base/logging.h>

#include <algorithm>

namespace aidl::android::hardware::radio::samsung {

UiccSlots& UiccSlots::get() {
    static UiccSlots instance;
    return instance;
}

void UiccSlots::update(const SecSimSlotStatusList& list) {
    std::lock_guard lock(mMutex);
    for (auto& slot : mLogicalSlots) {
        slot = {};
    }
    for (int s = 0; s < std::clamp(list.numSlots, 0, kSecMaxSlots); s++) {
        const auto& slot = list.slots[s];
        for (int p = 0; p < std::clamp(slot.numPorts, 0, kSecMaxPorts); p++) {
            const int logical = slot.ports[p].logicalSlot;
            if (slot.ports[p].portState != 0 && logical >= 0 && logical < RIL_SOCKET_NUM) {
                mLogicalSlots[logical] = {s, p, slot.eid != nullptr ? slot.eid : ""};
                LOG(INFO) << "Logical slot " << logical << " on physical " << s << " port " << p;
            }
        }
    }
}

UiccSlots::LogicalSlot UiccSlots::logicalSlot(RIL_SOCKET_ID socket) {
    std::lock_guard lock(mMutex);
    return socket >= 0 && socket < RIL_SOCKET_NUM ? mLogicalSlots[socket] : LogicalSlot{};
}

}  // namespace aidl::android::hardware::radio::samsung
