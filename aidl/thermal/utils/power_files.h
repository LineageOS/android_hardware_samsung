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

#pragma once

#include <android-base/chrono_utils.h>

#include <chrono>
#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "thermal_info.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::base::boot_clock;

struct PowerSample {
    uint64_t energy_counter;
    uint64_t duration;
};

struct PowerStatus {
    boot_clock::time_point last_update_time;
    // A vector to record the queues of power sample history.
    std::vector<std::queue<PowerSample>> power_history;
    float last_updated_avg_power;
};

// A helper class for monitoring power rails.
class PowerFiles {
  public:
    PowerFiles() = default;
    ~PowerFiles() = default;
    // Disallow copy and assign.
    PowerFiles(const PowerFiles &) = delete;
    void operator=(const PowerFiles &) = delete;
    bool registerPowerRailsToWatch(const Json::Value &config);
    // Update the power data from ODPM sysfs
    bool refreshPowerStatus(void);
    // Get power status map
    const std::unordered_map<std::string, PowerStatus> &GetPowerStatusMap() const {
        std::shared_lock<std::shared_mutex> _lock(power_status_map_mutex_);
        return power_status_map_;
    }
    // Get power rail info map
    const std::unordered_map<std::string, PowerRailInfo> &GetPowerRailInfoMap() const {
        return power_rail_info_map_;
    }

  private:
    // Update energy value to energy_info_map_, return false if the value is failed to update.
    bool updateEnergyValues(void);
    // Compute the average power for physical power rail.
    float updateAveragePower(std::string_view power_rail, std::queue<PowerSample> *power_history);
    // Update the power data for the target power rail.
    float updatePowerRail(std::string_view power_rail);
    // Find the energy source path, return false if no energy source found.
    bool findEnergySourceToWatch(void);
    // The map to record the energy counter for each power rail.
    std::unordered_map<std::string, PowerSample> energy_info_map_;
    // The map to record the power data for each thermal sensor.
    std::unordered_map<std::string, PowerStatus> power_status_map_;
    mutable std::shared_mutex power_status_map_mutex_;
    // The map to record the power rail information from thermal config
    std::unordered_map<std::string, PowerRailInfo> power_rail_info_map_;
    // The set to store the energy source paths
    std::unordered_set<std::string> energy_path_set_;
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
