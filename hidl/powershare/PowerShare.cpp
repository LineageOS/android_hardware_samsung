/*
 * Copyright (C) 2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "powershare@1.0-service.samsung"

#include "PowerShare.h"
#include <android-base/logging.h>
#include <fstream>
#include <iostream>
#include "samsung_powershare.h"

namespace vendor {
namespace lineage {
namespace powershare {
namespace V1_0 {
namespace implementation {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);

    if (!file) {
        PLOG(ERROR) << "Failed to open: " << path;
        return;
    }

    LOG(DEBUG) << "write: " << path << " value: " << value;

    file << value << std::endl;

    if (!file) {
        PLOG(ERROR) << "Failed to write: " << path << " value: " << value;
    }
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);

    if (!file) {
        PLOG(ERROR) << "Failed to open: " << path;
        return def;
    }

    T result;

    file >> result;

    if (file.fail()) {
        PLOG(ERROR) << "Failed to read: " << path;
        return def;
    } else {
        LOG(DEBUG) << "read: " << path << " value: " << result;
        return result;
    }
}

Return<bool> PowerShare::isEnabled() {
    return get(POWERSHARE_PATH, 0) == 1;
}

Return<bool> PowerShare::setEnabled(bool enable) {
    set(POWERSHARE_PATH, enable ? 1 : 0);

    return isEnabled();
}

Return<uint32_t> PowerShare::getMinBattery() {
    return get(POWERSHARE_STOP_CAPACITY_PATH, 0);
}

Return<uint32_t> PowerShare::setMinBattery(uint32_t minBattery) {
    set(POWERSHARE_STOP_CAPACITY_PATH, minBattery);

    return getMinBattery();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace powershare
}  // namespace lineage
}  // namespace vendor
