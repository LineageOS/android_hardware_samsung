/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define ATRACE_TAG (ATRACE_TAG_THERMAL | ATRACE_TAG_HAL)

#include "thermal_files.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <utils/Trace.h>

#include <algorithm>
#include <string_view>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::base::StringPrintf;

std::string ThermalFiles::getThermalFilePath(std::string_view thermal_name) const {
    auto sensor_itr = thermal_name_to_path_map_.find(thermal_name.data());
    if (sensor_itr == thermal_name_to_path_map_.end()) {
        return "";
    }
    return sensor_itr->second;
}

bool ThermalFiles::addThermalFile(std::string_view thermal_name, std::string_view path) {
    return thermal_name_to_path_map_.emplace(thermal_name, path).second;
}

bool ThermalFiles::readThermalFile(std::string_view thermal_name, std::string *data) const {
    std::string sensor_reading;
    std::string file_path = getThermalFilePath(std::string_view(thermal_name));
    *data = "";

    ATRACE_NAME(StringPrintf("ThermalFiles::readThermalFile - %s", thermal_name.data()).c_str());
    if (file_path.empty()) {
        PLOG(WARNING) << "Failed to find " << thermal_name << "'s path";
        return false;
    }

    if (!::android::base::ReadFileToString(file_path, &sensor_reading)) {
        PLOG(WARNING) << "Failed to read sensor: " << thermal_name;
        return false;
    }

    // Strip the newline.
    *data = ::android::base::Trim(sensor_reading);
    return true;
}

bool ThermalFiles::writeCdevFile(std::string_view cdev_name, std::string_view data) {
    std::string file_path =
            getThermalFilePath(::android::base::StringPrintf("%s_%s", cdev_name.data(), "w"));

    ATRACE_NAME(StringPrintf("ThermalFiles::writeCdevFile - %s", cdev_name.data()).c_str());
    if (!::android::base::WriteStringToFile(data.data(), file_path)) {
        PLOG(WARNING) << "Failed to write cdev: " << cdev_name << " to " << data.data();
        return false;
    }

    return true;
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
