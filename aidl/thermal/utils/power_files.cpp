
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

#include "power_files.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <dirent.h>
#include <utils/Trace.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

constexpr std::string_view kDeviceType("iio:device");
constexpr std::string_view kIioRootDir("/sys/bus/iio/devices");
constexpr std::string_view kEnergyValueNode("energy_value");

using ::android::base::ReadFileToString;
using ::android::base::StringPrintf;

bool PowerFiles::registerPowerRailsToWatch(const Json::Value &config) {
    if (!ParsePowerRailInfo(config, &power_rail_info_map_)) {
        LOG(ERROR) << "Failed to parse power rail info config";
        return false;
    }

    if (!power_rail_info_map_.size()) {
        LOG(INFO) << " No power rail info config found";
        return true;
    }

    if (!findEnergySourceToWatch()) {
        LOG(ERROR) << "Cannot find energy source";
        return false;
    }

    if (!energy_info_map_.size() && !updateEnergyValues()) {
        LOG(ERROR) << "Faield to update energy info";
        return false;
    }

    for (const auto &power_rail_info_pair : power_rail_info_map_) {
        std::vector<std::queue<PowerSample>> power_history;
        if (!power_rail_info_pair.second.power_sample_count ||
            power_rail_info_pair.second.power_sample_delay == std::chrono::milliseconds::max()) {
            continue;
        }

        PowerSample power_sample = {
                .energy_counter = 0,
                .duration = 0,
        };

        if (power_rail_info_pair.second.virtual_power_rail_info != nullptr &&
            power_rail_info_pair.second.virtual_power_rail_info->linked_power_rails.size()) {
            for (size_t i = 0;
                 i < power_rail_info_pair.second.virtual_power_rail_info->linked_power_rails.size();
                 ++i) {
                if (!energy_info_map_.count(power_rail_info_pair.second.virtual_power_rail_info
                                                    ->linked_power_rails[i])) {
                    LOG(ERROR) << " Could not find energy source "
                               << power_rail_info_pair.second.virtual_power_rail_info
                                          ->linked_power_rails[i];
                    return false;
                }
                power_history.emplace_back(std::queue<PowerSample>());
                for (int j = 0; j < power_rail_info_pair.second.power_sample_count; j++) {
                    power_history[i].emplace(power_sample);
                }
            }
        } else {
            if (energy_info_map_.count(power_rail_info_pair.first)) {
                power_history.emplace_back(std::queue<PowerSample>());
                for (int j = 0; j < power_rail_info_pair.second.power_sample_count; j++) {
                    power_history[0].emplace(power_sample);
                }
            } else {
                LOG(ERROR) << "Could not find energy source " << power_rail_info_pair.first;
                return false;
            }
        }

        if (power_history.size()) {
            power_status_map_[power_rail_info_pair.first] = {
                    .power_history = power_history,
                    .last_update_time = boot_clock::time_point::min(),
                    .last_updated_avg_power = NAN,
            };
        } else {
            LOG(ERROR) << "power history size is zero";
            return false;
        }
        LOG(INFO) << "Successfully to register power rail " << power_rail_info_pair.first;
    }
    return true;
}

bool PowerFiles::findEnergySourceToWatch(void) {
    std::string devicePath;

    if (energy_path_set_.size()) {
        return true;
    }

    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(kIioRootDir.data()), closedir);
    if (!dir) {
        PLOG(ERROR) << "Error opening directory" << kIioRootDir;
        return false;
    }

    // Find any iio:devices that support energy_value
    while (struct dirent *ent = readdir(dir.get())) {
        std::string devTypeDir = ent->d_name;
        if (devTypeDir.find(kDeviceType) != std::string::npos) {
            devicePath = StringPrintf("%s/%s", kIioRootDir.data(), devTypeDir.data());
            std::string deviceEnergyContent;

            if (!ReadFileToString(StringPrintf("%s/%s", devicePath.data(), kEnergyValueNode.data()),
                                  &deviceEnergyContent)) {
            } else if (deviceEnergyContent.size()) {
                energy_path_set_.emplace(
                        StringPrintf("%s/%s", devicePath.data(), kEnergyValueNode.data()));
            }
        }
    }

    if (!energy_path_set_.size()) {
        return false;
    }

    return true;
}

bool PowerFiles::updateEnergyValues(void) {
    std::string deviceEnergyContent;
    std::string deviceEnergyContents;
    std::string line;

    ATRACE_CALL();
    for (const auto &path : energy_path_set_) {
        if (!::android::base::ReadFileToString(path, &deviceEnergyContent)) {
            LOG(ERROR) << "Failed to read energy content from " << path;
            return false;
        } else {
            deviceEnergyContents.append(deviceEnergyContent);
        }
    }

    std::istringstream energyData(deviceEnergyContents);

    while (std::getline(energyData, line)) {
        /* Read rail energy */
        uint64_t energy_counter = 0;
        uint64_t duration = 0;

        /* Format example: CH3(T=358356)[S2M_VDD_CPUCL2], 761330 */
        auto start_pos = line.find("T=");
        auto end_pos = line.find(')');
        if (start_pos != std::string::npos) {
            duration =
                    strtoul(line.substr(start_pos + 2, end_pos - start_pos - 2).c_str(), NULL, 10);
        } else {
            continue;
        }

        start_pos = line.find(")[");
        end_pos = line.find(']');
        std::string railName;
        if (start_pos != std::string::npos) {
            railName = line.substr(start_pos + 2, end_pos - start_pos - 2);
        } else {
            continue;
        }

        start_pos = line.find("],");
        if (start_pos != std::string::npos) {
            energy_counter = strtoul(line.substr(start_pos + 2).c_str(), NULL, 10);
        } else {
            continue;
        }

        energy_info_map_[railName] = {
                .energy_counter = energy_counter,
                .duration = duration,
        };
    }

    return true;
}

float PowerFiles::updateAveragePower(std::string_view power_rail,
                                     std::queue<PowerSample> *power_history) {
    float avg_power = NAN;

    if (!energy_info_map_.count(power_rail.data())) {
        LOG(ERROR) << " Could not find power rail " << power_rail.data();
        return avg_power;
    }
    const auto last_sample = power_history->front();
    const auto curr_sample = energy_info_map_.at(power_rail.data());
    const auto duration = curr_sample.duration - last_sample.duration;
    const auto deltaEnergy = curr_sample.energy_counter - last_sample.energy_counter;

    if (!last_sample.duration) {
        LOG(VERBOSE) << "Power rail " << power_rail.data()
                     << ": all power samples have not been collected yet";
    } else if (duration <= 0 || deltaEnergy < 0) {
        LOG(ERROR) << "Power rail " << power_rail.data() << " is invalid: duration = " << duration
                   << ", deltaEnergy = " << deltaEnergy;

        return avg_power;
    } else {
        avg_power = static_cast<float>(deltaEnergy) / static_cast<float>(duration);
        LOG(VERBOSE) << "Power rail " << power_rail.data() << ", avg power = " << avg_power
                     << ", duration = " << duration << ", deltaEnergy = " << deltaEnergy;
    }

    power_history->pop();
    power_history->push(curr_sample);

    return avg_power;
}

float PowerFiles::updatePowerRail(std::string_view power_rail) {
    float avg_power = NAN;

    if (!power_rail_info_map_.count(power_rail.data())) {
        return avg_power;
    }

    if (!power_status_map_.count(power_rail.data())) {
        return avg_power;
    }

    const auto &power_rail_info = power_rail_info_map_.at(power_rail.data());
    auto &power_status = power_status_map_.at(power_rail.data());

    boot_clock::time_point now = boot_clock::now();
    auto time_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - power_status.last_update_time);

    if (power_status.last_update_time != boot_clock::time_point::min() &&
        time_elapsed_ms < power_rail_info.power_sample_delay) {
        return power_status.last_updated_avg_power;
    }

    if (!energy_info_map_.size() && !updateEnergyValues()) {
        LOG(ERROR) << "Failed to update energy values";
        return avg_power;
    }

    if (power_rail_info.virtual_power_rail_info == nullptr) {
        avg_power = updateAveragePower(power_rail, &power_status.power_history[0]);
    } else {
        const auto offset = power_rail_info.virtual_power_rail_info->offset;
        float avg_power_val = 0.0;
        for (size_t i = 0; i < power_rail_info.virtual_power_rail_info->linked_power_rails.size();
             i++) {
            float coefficient = power_rail_info.virtual_power_rail_info->coefficients[i];
            float avg_power_number = updateAveragePower(
                    power_rail_info.virtual_power_rail_info->linked_power_rails[i],
                    &power_status.power_history[i]);

            switch (power_rail_info.virtual_power_rail_info->formula) {
                case FormulaOption::COUNT_THRESHOLD:
                    if ((coefficient < 0 && avg_power_number < -coefficient) ||
                        (coefficient >= 0 && avg_power_number >= coefficient))
                        avg_power_val += 1;
                    break;
                case FormulaOption::WEIGHTED_AVG:
                    avg_power_val += avg_power_number * coefficient;
                    break;
                case FormulaOption::MAXIMUM:
                    if (i == 0)
                        avg_power_val = std::numeric_limits<float>::lowest();
                    if (avg_power_number * coefficient > avg_power_val)
                        avg_power_val = avg_power_number * coefficient;
                    break;
                case FormulaOption::MINIMUM:
                    if (i == 0)
                        avg_power_val = std::numeric_limits<float>::max();
                    if (avg_power_number * coefficient < avg_power_val)
                        avg_power_val = avg_power_number * coefficient;
                    break;
                default:
                    break;
            }
        }
        if (avg_power_val >= 0) {
            avg_power_val = avg_power_val + offset;
        }

        avg_power = avg_power_val;
    }

    if (avg_power < 0) {
        avg_power = NAN;
    }

    power_status.last_updated_avg_power = avg_power;
    power_status.last_update_time = now;
    return avg_power;
}

bool PowerFiles::refreshPowerStatus(void) {
    if (!updateEnergyValues()) {
        LOG(ERROR) << "Failed to update energy values";
        return false;
    }

    for (const auto &power_status_pair : power_status_map_) {
        updatePowerRail(power_status_pair.first);
    }
    return true;
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
