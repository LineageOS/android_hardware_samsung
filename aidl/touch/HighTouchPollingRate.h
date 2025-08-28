/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/touch/BnHighTouchPollingRate.h>
#include <samsung_touch.h>
#include <fstream>

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

class HighTouchPollingRate : public BnHighTouchPollingRate {
  public:
    HighTouchPollingRate() {
        std::ifstream file(TSP_CMD_LIST_NODE);
        if (file.is_open()) {
            mHtprCmd = "";
            std::string line;
            while (getline(file, line)) {
                if (!line.compare("set_game_mode") || !line.compare("set_scan_rate")) {
                    mHtprCmd = line;
                    break;
                }
            }
            file.close();
        }
    }

    bool isSupported();

    ndk::ScopedAStatus getEnabled(bool* _aidl_return) override;
    ndk::ScopedAStatus setEnabled(bool enabled) override;

  private:
    std::string mHtprCmd;
};

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl