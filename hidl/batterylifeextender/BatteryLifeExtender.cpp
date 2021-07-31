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

#define LOG_TAG "batterylifeextender@1.0-service.samsung"

#include "BatteryLifeExtender.h"
#include <android-base/logging.h>
#include <cutils/properties.h>

#include <fstream>
#include <iostream>
#include "samsung_batterylifeextender.h"

namespace vendor {
namespace lineage {
namespace batterylifeextender {
namespace V1_0 {
namespace implementation {

static constexpr const char* kBatteryLifeExtenderProp = "persist.vendor.sec.battlifeext_enabled";

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

BatteryLifeExtender::BatteryLifeExtender() {
    setEnabled(property_get_bool(kBatteryLifeExtenderProp, BATTERYLIFEEXTENDER_DEFAULT_SETTING));
}

Return<bool> BatteryLifeExtender::isEnabled() {
    return get(BATTERYLIFEEXTENDER_PATH, 0) == 1;
}

Return<bool> BatteryLifeExtender::setEnabled(bool enable) {
    set(BATTERYLIFEEXTENDER_PATH, enable ? 1 : 0);

    bool enabled = isEnabled();
    property_set(kBatteryLifeExtenderProp, enabled ? "true" : "false");

    return enabled;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace batterylifeextender
}  // namespace lineage
}  // namespace vendor
