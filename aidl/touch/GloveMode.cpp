/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include "GloveMode.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

bool GloveMode::isSupported() {
    std::ifstream file(TSP_CMD_LIST_NODE);
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (!line.compare("glove_mode")) return true;
        }
        file.close();
    }
    return false;
}

ndk::ScopedAStatus GloveMode::getEnabled(bool* _aidl_return) {
    std::ifstream file(TSP_CMD_RESULT_NODE);
    if (file.is_open()) {
        std::string line;
        getline(file, line);
        *_aidl_return = !line.compare("glove_mode,1:OK");
        file.close();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GloveMode::setEnabled(bool enabled) {
    std::ofstream file(TSP_CMD_NODE);
    file << "glove_mode," << (enabled ? "1" : "0");

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
