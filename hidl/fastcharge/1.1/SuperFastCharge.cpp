/*
 * Copyright (C) 2020-2021 The LineageOS Project
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

#define LOG_TAG "fastcharge@1.1-service.samsung"

#include "SuperFastCharge.h"
#include <android-base/logging.h>
#include <cutils/properties.h>

#include <fstream>
#include <iostream>
#include "samsung_fastcharge.h"

namespace vendor {
namespace lineage {
namespace fastcharge {
namespace V1_1 {
namespace implementation {

static constexpr const char* kSuperFastChargingProp = "persist.vendor.sec.superfastchg_enabled";

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

SuperFastCharge::SuperFastCharge() {
    setEnabled(property_get_bool(kSuperFastChargingProp, FASTCHARGE_DEFAULT_SETTING));
}

bool SuperFastCharge::isSupported() {
    std::fstream file(SUPER_FASTCHARGE_PATH, file.in | file.out);
    return file.good();
}

Return<bool> SuperFastCharge::isEnabled() {
    return get(SUPER_FASTCHARGE_PATH, 0) < 1;
}

Return<bool> SuperFastCharge::setEnabled(bool enable) {
    set(SUPER_FASTCHARGE_PATH, enable ? 0 : 1);

    bool enabled = isEnabled();
    property_set(kSuperFastChargingProp, enabled ? "true" : "false");

    return enabled;
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace fastcharge
}  // namespace lineage
}  // namespace vendor
