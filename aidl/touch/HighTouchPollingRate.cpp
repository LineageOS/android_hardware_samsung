/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include "HighTouchPollingRate.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

bool HighTouchPollingRate::isSupported() {
    return !mHtprCmd.empty();
}

ndk::ScopedAStatus HighTouchPollingRate::getEnabled(bool* _aidl_return) {
    std::ifstream file(TSP_CMD_RESULT_NODE);
    if (file.is_open()) {
        std::string line;
        getline(file, line);
        *_aidl_return = !line.compare(mHtprCmd + ",1:OK");
        file.close();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus HighTouchPollingRate::setEnabled(bool enabled) {
    std::ofstream file(TSP_CMD_NODE);
    file << (mHtprCmd + ",") << (enabled ? "1" : "0");

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
