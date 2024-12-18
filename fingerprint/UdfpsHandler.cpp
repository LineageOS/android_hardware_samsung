/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <fstream>
#include <string>
#include "UdfpsHandler.h"

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

template <typename T>
void UdfpsHandler::set(const std::string& path, const T& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open file at path: " << path;
        return;
    }

    file << value << std::endl;
    if (file.fail()) {
        LOG(ERROR) << "Failed to write value: " << value << " to path: " << path;
    }
}

void UdfpsHandler::setFodPress(bool enable) {
    if (enable) {
        LOG(INFO) << "Enabling FOD press";
        set(TSP_CMD, "fod_enable,1,1,0");
    } else {
        LOG(INFO) << "Disabling FOD press";
        set(TSP_CMD, "fod_enable,0,0,0");
    }
}

void UdfpsHandler::setFodRect(const std::string& fod_rect) {
    std::ifstream file(TSP_CMD_LIST);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open TSP_CMD_LIST file, skipping setFodRect...";
        return;
    }

    std::string line;
    bool cmd_support = false;

    while (getline(file, line)) {
        if (line == "set_fod_rect") {
            cmd_support = true;

            if (!fod_rect.empty()) {
                LOG(INFO) << "Writing set_fod_rect," << fod_rect << " to TSP Sponge";
                set(TSP_CMD, "set_fod_rect," + fod_rect);
            } else {
                LOG(INFO) << "Rectangle FOD location is not defined, skipping setFodRect...";
            }
            return;
        }
    }

    if (!cmd_support) {
        LOG(INFO) << "set_fod_rect command is not available to TSP, skipping setFodRect...";
    }
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
