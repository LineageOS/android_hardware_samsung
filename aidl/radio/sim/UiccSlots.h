/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/structs.h>

#include <mutex>
#include <string>

namespace aidl::android::hardware::radio::samsung {

class UiccSlots {
  public:
    struct LogicalSlot {
        int physicalSlot = -1;
        int port = 0;
        std::string eid;
    };

    static UiccSlots& get();

    void update(const SecSimSlotStatusList& list);
    LogicalSlot logicalSlot(RIL_SOCKET_ID socket);

  private:
    std::mutex mMutex;
    LogicalSlot mLogicalSlots[RIL_SOCKET_NUM];
};

}  // namespace aidl::android::hardware::radio::samsung
