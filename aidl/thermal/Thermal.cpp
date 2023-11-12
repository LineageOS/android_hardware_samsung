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

#include "Thermal.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <utils/Trace.h>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

namespace {

ndk::ScopedAStatus initErrorStatus() {
    return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_STATE,
                                                            "ThermalHAL not initialized properly.");
}

ndk::ScopedAStatus readErrorStatus() {
    return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
            EX_ILLEGAL_STATE, "ThermalHal cannot read any sensor data");
}

bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface> left,
                     const std::shared_ptr<::ndk::ICInterface> right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

}  // namespace

Thermal::Thermal()
    : thermal_helper_(
              std::bind(&Thermal::sendThermalChangedCallback, this, std::placeholders::_1)) {}

ndk::ScopedAStatus Thermal::getTemperatures(std::vector<Temperature> *_aidl_return) {
    return getFilteredTemperatures(false, TemperatureType::UNKNOWN, _aidl_return);
}

ndk::ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType type,
                                                    std::vector<Temperature> *_aidl_return) {
    return getFilteredTemperatures(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredTemperatures(bool filterType, TemperatureType type,
                                                    std::vector<Temperature> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_helper_.fillCurrentTemperatures(filterType, false, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice> *_aidl_return) {
    return getFilteredCoolingDevices(false, CoolingType::BATTERY, _aidl_return);
}

ndk::ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType type,
                                                      std::vector<CoolingDevice> *_aidl_return) {
    return getFilteredCoolingDevices(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredCoolingDevices(bool filterType, CoolingType type,
                                                      std::vector<CoolingDevice> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_helper_.fillCurrentCoolingDevices(filterType, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold> *_aidl_return) {
    *_aidl_return = {};
    return getFilteredTemperatureThresholds(false, TemperatureType::UNKNOWN, _aidl_return);
}

ndk::ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) {
    return getFilteredTemperatureThresholds(true, type, _aidl_return);
}

ndk::ScopedAStatus Thermal::getFilteredTemperatureThresholds(
        bool filterType, TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_helper_.fillTemperatureThresholds(filterType, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback) {
    ATRACE_CALL();
    return registerThermalChangedCallback(callback, false, TemperatureType::UNKNOWN);
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
        const std::shared_ptr<IThermalChangedCallback> &callback, TemperatureType type) {
    ATRACE_CALL();
    return registerThermalChangedCallback(callback, true, type);
}

ndk::ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback) {
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    bool removed = false;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    callbacks_.erase(
            std::remove_if(
                    callbacks_.begin(), callbacks_.end(),
                    [&](const CallbackSetting &c) {
                        if (interfacesEqual(c.callback, callback)) {
                            LOG(INFO)
                                    << "a callback has been unregistered to ThermalHAL, isFilter: "
                                    << c.is_filter_type << " Type: " << toString(c.type);
                            removed = true;
                            return true;
                        }
                        return false;
                    }),
            callbacks_.end());
    if (!removed) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback wasn't registered");
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback, bool filterType,
        TemperatureType type) {
    ATRACE_CALL();
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting &c) {
            return interfacesEqual(c.callback, callback);
        })) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback already registered");
    }
    auto c = callbacks_.emplace_back(callback, filterType, type);
    LOG(INFO) << "a callback has been registered to ThermalHAL, isFilter: " << c.is_filter_type
              << " Type: " << toString(c.type);
    // Send notification right away after successful thermal callback registration
    std::function<void()> handler = [this, c, filterType, type]() {
        std::vector<Temperature> temperatures;
        if (thermal_helper_.fillCurrentTemperatures(filterType, true, type, &temperatures)) {
            for (const auto &t : temperatures) {
                if (!filterType || t.type == type) {
                    LOG(INFO) << "Sending notification: "
                              << " Type: " << toString(t.type) << " Name: " << t.name
                              << " CurrentValue: " << t.value
                              << " ThrottlingStatus: " << toString(t.throttlingStatus);
                    c.callback->notifyThrottling(t);
                }
            }
        }
    };
    looper_.addEvent(Looper::Event{handler});
    return ndk::ScopedAStatus::ok();
}

void Thermal::sendThermalChangedCallback(const Temperature &t) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    LOG(VERBOSE) << "Sending notification: "
                 << " Type: " << toString(t.type) << " Name: " << t.name
                 << " CurrentValue: " << t.value
                 << " ThrottlingStatus: " << toString(t.throttlingStatus);

    callbacks_.erase(std::remove_if(callbacks_.begin(), callbacks_.end(),
                                    [&](const CallbackSetting &c) {
                                        if (!c.is_filter_type || t.type == c.type) {
                                            ::ndk::ScopedAStatus ret =
                                                    c.callback->notifyThrottling(t);
                                            if (!ret.isOk()) {
                                                LOG(ERROR) << "a Thermal callback is dead, removed "
                                                              "from callback list.";
                                                return true;
                                            }
                                            return false;
                                        }
                                        return false;
                                    }),
                     callbacks_.end());
}

void Thermal::dumpVirtualSensorInfo(std::ostringstream *dump_buf) {
    *dump_buf << "getVirtualSensorInfo:" << std::endl;
    const auto &map = thermal_helper_.GetSensorInfoMap();
    for (const auto &sensor_info_pair : map) {
        if (sensor_info_pair.second.virtual_sensor_info != nullptr) {
            *dump_buf << " Name: " << sensor_info_pair.first << std::endl;
            *dump_buf << "  LinkedSensorName: [";
            for (size_t i = 0;
                 i < sensor_info_pair.second.virtual_sensor_info->linked_sensors.size(); i++) {
                *dump_buf << sensor_info_pair.second.virtual_sensor_info->linked_sensors[i] << " ";
            }
            *dump_buf << "]" << std::endl;
            *dump_buf << "  LinkedSensorCoefficient: [";
            for (size_t i = 0; i < sensor_info_pair.second.virtual_sensor_info->coefficients.size();
                 i++) {
                *dump_buf << sensor_info_pair.second.virtual_sensor_info->coefficients[i] << " ";
            }
            *dump_buf << "]" << std::endl;
            *dump_buf << "  Offset: " << sensor_info_pair.second.virtual_sensor_info->offset
                      << std::endl;
            *dump_buf << "  Trigger Sensor: ";
            if (sensor_info_pair.second.virtual_sensor_info->trigger_sensors.empty()) {
                *dump_buf << "N/A" << std::endl;
            } else {
                for (size_t i = 0;
                     i < sensor_info_pair.second.virtual_sensor_info->trigger_sensors.size(); i++) {
                    *dump_buf << sensor_info_pair.second.virtual_sensor_info->trigger_sensors[i]
                              << " ";
                }
                *dump_buf << std::endl;
            }
            *dump_buf << "  Formula: ";
            switch (sensor_info_pair.second.virtual_sensor_info->formula) {
                case FormulaOption::COUNT_THRESHOLD:
                    *dump_buf << "COUNT_THRESHOLD";
                    break;
                case FormulaOption::WEIGHTED_AVG:
                    *dump_buf << "WEIGHTED_AVG";
                    break;
                case FormulaOption::MAXIMUM:
                    *dump_buf << "MAXIMUM";
                    break;
                case FormulaOption::MINIMUM:
                    *dump_buf << "MINIMUM";
                    break;
                default:
                    *dump_buf << "NONE";
                    break;
            }

            *dump_buf << std::endl;
        }
    }
}

void Thermal::dumpThrottlingInfo(std::ostringstream *dump_buf) {
    *dump_buf << "getThrottlingInfo:" << std::endl;
    const auto &map = thermal_helper_.GetSensorInfoMap();
    const auto &thermal_throttling_status_map = thermal_helper_.GetThermalThrottlingStatusMap();
    for (const auto &name_info_pair : map) {
        if (name_info_pair.second.throttling_info == nullptr) {
            continue;
        }
        if (name_info_pair.second.throttling_info->binded_cdev_info_map.size()) {
            if (thermal_throttling_status_map.find(name_info_pair.first) ==
                thermal_throttling_status_map.end()) {
                continue;
            }
            *dump_buf << " Name: " << name_info_pair.first << std::endl;
            if (thermal_throttling_status_map.at(name_info_pair.first)
                        .pid_power_budget_map.size()) {
                *dump_buf << "  PID Info:" << std::endl;
                *dump_buf << "   K_po: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_po[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_pu: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_pu[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_i: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_i[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   K_d: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->k_d[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   i_max: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->i_max[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   max_alloc_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->max_alloc_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   min_alloc_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->min_alloc_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   s_power: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->s_power[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "   i_cutoff: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << name_info_pair.second.throttling_info->i_cutoff[i] << " ";
                }
                *dump_buf << "]" << std::endl;
            }
            *dump_buf << "  Binded CDEV Info:" << std::endl;
            for (const auto &binded_cdev_info_pair :
                 name_info_pair.second.throttling_info->binded_cdev_info_map) {
                *dump_buf << "   Cooling device name: " << binded_cdev_info_pair.first << std::endl;
                if (thermal_throttling_status_map.at(name_info_pair.first)
                            .pid_power_budget_map.size()) {
                    *dump_buf << "    WeightForPID: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.cdev_weight_for_pid[i] << " ";
                    }
                    *dump_buf << "]" << std::endl;
                }
                *dump_buf << "    Ceiling: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << binded_cdev_info_pair.second.cdev_ceiling[i] << " ";
                }
                *dump_buf << "]" << std::endl;
                *dump_buf << "    Hard limit: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    *dump_buf << binded_cdev_info_pair.second.limit_info[i] << " ";
                }
                *dump_buf << "]" << std::endl;

                if (!binded_cdev_info_pair.second.power_rail.empty()) {
                    *dump_buf << "    Binded power rail: "
                              << binded_cdev_info_pair.second.power_rail << std::endl;
                    *dump_buf << "    Power threshold: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.power_thresholds[i] << " ";
                    }
                    *dump_buf << "]" << std::endl;
                    *dump_buf << "    Floor with PowerLink: [";
                    for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                        *dump_buf << binded_cdev_info_pair.second.cdev_floor_with_power_link[i]
                                  << " ";
                    }
                    *dump_buf << "]" << std::endl;
                    *dump_buf << "    Release logic: ";
                    switch (binded_cdev_info_pair.second.release_logic) {
                        case ReleaseLogic::INCREASE:
                            *dump_buf << "INCREASE";
                            break;
                        case ReleaseLogic::DECREASE:
                            *dump_buf << "DECREASE";
                            break;
                        case ReleaseLogic::STEPWISE:
                            *dump_buf << "STEPWISE";
                            break;
                        case ReleaseLogic::RELEASE_TO_FLOOR:
                            *dump_buf << "RELEASE_TO_FLOOR";
                            break;
                        default:
                            *dump_buf << "NONE";
                            break;
                    }
                    *dump_buf << std::endl;
                    *dump_buf << "    high_power_check: " << std::boolalpha
                              << binded_cdev_info_pair.second.high_power_check << std::endl;
                    *dump_buf << "    throttling_with_power_link: " << std::boolalpha
                              << binded_cdev_info_pair.second.throttling_with_power_link
                              << std::endl;
                }
            }
        }
    }
}

void Thermal::dumpThrottlingRequestStatus(std::ostringstream *dump_buf) {
    const auto &thermal_throttling_status_map = thermal_helper_.GetThermalThrottlingStatusMap();
    if (!thermal_throttling_status_map.size()) {
        return;
    }
    *dump_buf << "getThrottlingRequestStatus:" << std::endl;
    for (const auto &thermal_throttling_status_pair : thermal_throttling_status_map) {
        *dump_buf << " Name: " << thermal_throttling_status_pair.first << std::endl;
        if (thermal_throttling_status_pair.second.pid_power_budget_map.size()) {
            *dump_buf << "  power budget request state" << std::endl;
            for (const auto &request_pair :
                 thermal_throttling_status_pair.second.pid_power_budget_map) {
                *dump_buf << "   " << request_pair.first << ": " << request_pair.second
                          << std::endl;
            }
        }
        if (thermal_throttling_status_pair.second.pid_cdev_request_map.size()) {
            *dump_buf << "  pid cdev request state" << std::endl;
            for (const auto &request_pair :
                 thermal_throttling_status_pair.second.pid_cdev_request_map) {
                *dump_buf << "   " << request_pair.first << ": " << request_pair.second
                          << std::endl;
            }
        }
        if (thermal_throttling_status_pair.second.hardlimit_cdev_request_map.size()) {
            *dump_buf << "  hard limit cdev request state" << std::endl;
            for (const auto &request_pair :
                 thermal_throttling_status_pair.second.hardlimit_cdev_request_map) {
                *dump_buf << "   " << request_pair.first << ": " << request_pair.second
                          << std::endl;
            }
        }
        if (thermal_throttling_status_pair.second.throttling_release_map.size()) {
            *dump_buf << "  cdev release state" << std::endl;
            for (const auto &request_pair :
                 thermal_throttling_status_pair.second.throttling_release_map) {
                *dump_buf << "   " << request_pair.first << ": " << request_pair.second
                          << std::endl;
            }
        }
        if (thermal_throttling_status_pair.second.cdev_status_map.size()) {
            *dump_buf << "  cdev request state" << std::endl;
            for (const auto &request_pair : thermal_throttling_status_pair.second.cdev_status_map) {
                *dump_buf << "   " << request_pair.first << ": " << request_pair.second
                          << std::endl;
            }
        }
    }
}

void Thermal::dumpPowerRailInfo(std::ostringstream *dump_buf) {
    const auto &power_rail_info_map = thermal_helper_.GetPowerRailInfoMap();
    const auto &power_status_map = thermal_helper_.GetPowerStatusMap();

    *dump_buf << "getPowerRailInfo:" << std::endl;
    for (const auto &power_rail_pair : power_rail_info_map) {
        *dump_buf << " Power Rail: " << power_rail_pair.first << std::endl;
        *dump_buf << "  Power Sample Count: " << power_rail_pair.second.power_sample_count
                  << std::endl;
        *dump_buf << "  Power Sample Delay: " << power_rail_pair.second.power_sample_delay.count()
                  << std::endl;
        if (power_status_map.count(power_rail_pair.first)) {
            auto power_history = power_status_map.at(power_rail_pair.first).power_history;
            *dump_buf << "  Last Updated AVG Power: "
                      << power_status_map.at(power_rail_pair.first).last_updated_avg_power << " mW"
                      << std::endl;
            if (power_rail_pair.second.virtual_power_rail_info != nullptr) {
                *dump_buf << "  Formula=";
                switch (power_rail_pair.second.virtual_power_rail_info->formula) {
                    case FormulaOption::COUNT_THRESHOLD:
                        *dump_buf << "COUNT_THRESHOLD";
                        break;
                    case FormulaOption::WEIGHTED_AVG:
                        *dump_buf << "WEIGHTED_AVG";
                        break;
                    case FormulaOption::MAXIMUM:
                        *dump_buf << "MAXIMUM";
                        break;
                    case FormulaOption::MINIMUM:
                        *dump_buf << "MINIMUM";
                        break;
                    default:
                        *dump_buf << "NONE";
                        break;
                }
                *dump_buf << std::endl;
            }
            for (size_t i = 0; i < power_history.size(); ++i) {
                if (power_rail_pair.second.virtual_power_rail_info != nullptr) {
                    *dump_buf
                            << "  Linked power rail "
                            << power_rail_pair.second.virtual_power_rail_info->linked_power_rails[i]
                            << std::endl;
                    *dump_buf << "   Coefficient="
                              << power_rail_pair.second.virtual_power_rail_info->coefficients[i]
                              << std::endl;
                    *dump_buf << "   Power Samples: ";
                } else {
                    *dump_buf << "  Power Samples: ";
                }
                while (power_history[i].size() > 0) {
                    const auto power_sample = power_history[i].front();
                    power_history[i].pop();
                    *dump_buf << "(T=" << power_sample.duration
                              << ", uWs=" << power_sample.energy_counter << ") ";
                }
                *dump_buf << std::endl;
            }
        }
    }
}

void Thermal::dumpStatsRecord(std::ostringstream *dump_buf, const StatsRecord &stats_record,
                              std::string_view line_prefix) {
    const auto now = boot_clock::now();
    *dump_buf << line_prefix << "Time Since Last Stats Report: "
              << std::chrono::duration_cast<std::chrono::minutes>(
                         now - stats_record.last_stats_report_time)
                         .count()
              << " mins" << std::endl;
    *dump_buf << line_prefix << "Time in State ms: [";
    for (const auto &time_in_state : stats_record.time_in_state_ms) {
        *dump_buf << time_in_state.count() << " ";
    }
    *dump_buf << "]" << std::endl;
}

void Thermal::dumpThermalStats(std::ostringstream *dump_buf) {
    *dump_buf << "getThermalStatsInfo:" << std::endl;
    *dump_buf << " Sensor Temp Stats Info:" << std::endl;
    const auto &sensor_temp_stats_map_ = thermal_helper_.GetSensorTempStatsSnapshot();
    const std::string sensor_temp_stats_line_prefix("    ");
    for (const auto &sensor_temp_stats_pair : sensor_temp_stats_map_) {
        *dump_buf << "  Sensor Name: " << sensor_temp_stats_pair.first << std::endl;
        const auto &sensor_temp_stats = sensor_temp_stats_pair.second;
        *dump_buf << "   Max Temp: " << sensor_temp_stats.max_temp << ", TimeStamp: "
                  << system_clock::to_time_t(sensor_temp_stats.max_temp_timestamp) << std::endl;
        *dump_buf << "   Min Temp: " << sensor_temp_stats.min_temp << ", TimeStamp: "
                  << system_clock::to_time_t(sensor_temp_stats.min_temp_timestamp) << std::endl;
        for (const auto &stats_by_threshold : sensor_temp_stats.stats_by_custom_threshold) {
            *dump_buf << "   Record by Threshold: [";
            for (const auto &threshold : stats_by_threshold.thresholds) {
                *dump_buf << threshold << " ";
            }
            *dump_buf << "]" << std::endl;
            if (stats_by_threshold.logging_name.has_value()) {
                *dump_buf << "    Logging Name: " << stats_by_threshold.logging_name.value()
                          << std::endl;
            }
            dumpStatsRecord(dump_buf, stats_by_threshold.stats_record,
                            sensor_temp_stats_line_prefix);
        }

        if (sensor_temp_stats.stats_by_default_threshold.has_value()) {
            *dump_buf << "   Record by Severity:" << std::endl;
            dumpStatsRecord(dump_buf, sensor_temp_stats.stats_by_default_threshold.value(),
                            sensor_temp_stats_line_prefix);
        }
    }
    *dump_buf << " Sensor Cdev Request Stats Info:" << std::endl;
    const auto &sensor_cdev_request_stats_map_ =
            thermal_helper_.GetSensorCoolingDeviceRequestStatsSnapshot();
    const std::string sensor_cdev_request_stats_line_prefix("     ");
    for (const auto &sensor_cdev_request_stats_pair : sensor_cdev_request_stats_map_) {
        *dump_buf << "  Sensor Name: " << sensor_cdev_request_stats_pair.first << std::endl;
        for (const auto &cdev_request_stats_pair : sensor_cdev_request_stats_pair.second) {
            *dump_buf << "   Cooling Device Name: " << cdev_request_stats_pair.first << std::endl;
            const auto &request_stats = cdev_request_stats_pair.second;
            for (const auto &stats_by_threshold : request_stats.stats_by_custom_threshold) {
                *dump_buf << "    Record by Threshold: [";
                for (const auto &threshold : stats_by_threshold.thresholds) {
                    *dump_buf << threshold << " ";
                }
                *dump_buf << "]" << std::endl;
                if (stats_by_threshold.logging_name.has_value()) {
                    *dump_buf << "     Logging Name: " << stats_by_threshold.logging_name.value()
                              << std::endl;
                }
                dumpStatsRecord(dump_buf, stats_by_threshold.stats_record,
                                sensor_cdev_request_stats_line_prefix);
            }
            if (request_stats.stats_by_default_threshold.has_value()) {
                *dump_buf << "    Record by All State" << std::endl;
                dumpStatsRecord(dump_buf, request_stats.stats_by_default_threshold.value(),
                                sensor_cdev_request_stats_line_prefix);
            }
        }
    }
}

void Thermal::dumpThermalData(int fd) {
    std::ostringstream dump_buf;

    if (!thermal_helper_.isInitializedOk()) {
        dump_buf << "ThermalHAL not initialized properly." << std::endl;
    } else {
        const auto &sensor_status_map = thermal_helper_.GetSensorStatusMap();
        {
            dump_buf << "getCachedTemperatures:" << std::endl;
            boot_clock::time_point now = boot_clock::now();
            for (const auto &sensor_status_pair : sensor_status_map) {
                if ((sensor_status_pair.second.thermal_cached.timestamp) ==
                    boot_clock::time_point::min()) {
                    continue;
                }
                dump_buf << " Name: " << sensor_status_pair.first
                         << " CachedValue: " << sensor_status_pair.second.thermal_cached.temp
                         << " TimeToCache: "
                         << std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now - sensor_status_pair.second.thermal_cached.timestamp)
                                    .count()
                         << "ms" << std::endl;
            }
        }
        {
            dump_buf << "getEmulTemperatures:" << std::endl;
            for (const auto &sensor_status_pair : sensor_status_map) {
                if (sensor_status_pair.second.emul_setting == nullptr) {
                    continue;
                }
                dump_buf << " Name: " << sensor_status_pair.first
                         << " EmulTemp: " << sensor_status_pair.second.emul_setting->emul_temp
                         << " EmulSeverity: "
                         << sensor_status_pair.second.emul_setting->emul_severity << std::endl;
            }
        }
        {
            const auto &map = thermal_helper_.GetSensorInfoMap();
            dump_buf << "getCurrentTemperatures:" << std::endl;
            Temperature temp_2_0;
            for (const auto &name_info_pair : map) {
                thermal_helper_.readTemperature(name_info_pair.first, &temp_2_0, nullptr, true);
                dump_buf << " Type: " << toString(temp_2_0.type)
                         << " Name: " << name_info_pair.first << " CurrentValue: " << temp_2_0.value
                         << " ThrottlingStatus: " << toString(temp_2_0.throttlingStatus)
                         << std::endl;
            }
            dump_buf << "getTemperatureThresholds:" << std::endl;
            for (const auto &name_info_pair : map) {
                if (!name_info_pair.second.is_watch) {
                    continue;
                }
                dump_buf << " Type: " << toString(name_info_pair.second.type)
                         << " Name: " << name_info_pair.first;
                dump_buf << " hotThrottlingThreshold: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.hot_thresholds[i] << " ";
                }
                dump_buf << "] coldThrottlingThreshold: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.cold_thresholds[i] << " ";
                }
                dump_buf << "] vrThrottlingThreshold: " << name_info_pair.second.vr_threshold;
                dump_buf << std::endl;
            }
            dump_buf << "getHysteresis:" << std::endl;
            for (const auto &name_info_pair : map) {
                if (!name_info_pair.second.is_watch) {
                    continue;
                }
                dump_buf << " Name: " << name_info_pair.first;
                dump_buf << " hotHysteresis: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.hot_hysteresis[i] << " ";
                }
                dump_buf << "] coldHysteresis: [";
                for (size_t i = 0; i < kThrottlingSeverityCount; ++i) {
                    dump_buf << name_info_pair.second.cold_hysteresis[i] << " ";
                }
                dump_buf << "]" << std::endl;
            }
        }
        {
            dump_buf << "getCurrentCoolingDevices:" << std::endl;
            std::vector<CoolingDevice> cooling_devices;
            if (!thermal_helper_.fillCurrentCoolingDevices(false, CoolingType::CPU,
                                                           &cooling_devices)) {
                dump_buf << " Failed to getCurrentCoolingDevices." << std::endl;
            }

            for (const auto &c : cooling_devices) {
                dump_buf << " Type: " << toString(c.type) << " Name: " << c.name
                         << " CurrentValue: " << c.value << std::endl;
            }
        }
        {
            dump_buf << "getCallbacks:" << std::endl;
            dump_buf << " Total: " << callbacks_.size() << std::endl;
            for (const auto &c : callbacks_) {
                dump_buf << " IsFilter: " << c.is_filter_type << " Type: " << toString(c.type)
                         << std::endl;
            }
        }
        {
            dump_buf << "sendCallback:" << std::endl;
            dump_buf << "  Enabled List: ";
            const auto &map = thermal_helper_.GetSensorInfoMap();
            for (const auto &name_info_pair : map) {
                if (name_info_pair.second.send_cb) {
                    dump_buf << name_info_pair.first << " ";
                }
            }
            dump_buf << std::endl;
        }
        {
            dump_buf << "sendPowerHint:" << std::endl;
            dump_buf << "  Enabled List: ";
            const auto &map = thermal_helper_.GetSensorInfoMap();
            for (const auto &name_info_pair : map) {
                if (name_info_pair.second.send_powerhint) {
                    dump_buf << name_info_pair.first << " ";
                }
            }
            dump_buf << std::endl;
        }
        dumpVirtualSensorInfo(&dump_buf);
        dumpThrottlingInfo(&dump_buf);
        dumpThrottlingRequestStatus(&dump_buf);
        dumpPowerRailInfo(&dump_buf);
        dumpThermalStats(&dump_buf);
        {
            dump_buf << "getAIDLPowerHalInfo:" << std::endl;
            dump_buf << " Exist: " << std::boolalpha << thermal_helper_.isAidlPowerHalExist()
                     << std::endl;
            dump_buf << " Connected: " << std::boolalpha << thermal_helper_.isPowerHalConnected()
                     << std::endl;
            dump_buf << " Ext connected: " << std::boolalpha
                     << thermal_helper_.isPowerHalExtConnected() << std::endl;
        }
    }
    std::string buf = dump_buf.str();
    if (!::android::base::WriteStringToFd(buf, fd)) {
        PLOG(ERROR) << "Failed to dump state to fd";
    }
    fsync(fd);
}

binder_status_t Thermal::dump(int fd, const char **args, uint32_t numArgs) {
    if (numArgs == 0 || std::string(args[0]) == "-a") {
        dumpThermalData(fd);
        return STATUS_OK;
    }

    if (std::string(args[0]) == "emul_temp") {
        return (numArgs != 3 || !thermal_helper_.emulTemp(std::string(args[1]), std::atof(args[2])))
                       ? STATUS_BAD_VALUE
                       : STATUS_OK;
    } else if (std::string(args[0]) == "emul_severity") {
        return (numArgs != 3 ||
                !thermal_helper_.emulSeverity(std::string(args[1]), std::atoi(args[2])))
                       ? STATUS_BAD_VALUE
                       : STATUS_OK;
    } else if (std::string(args[0]) == "emul_clear") {
        return (numArgs != 2 || !thermal_helper_.emulClear(std::string(args[1]))) ? STATUS_BAD_VALUE
                                                                                  : STATUS_OK;
    }
    return STATUS_BAD_VALUE;
}

void Thermal::Looper::addEvent(const Thermal::Looper::Event &e) {
    std::unique_lock<std::mutex> lock(mutex_);
    events_.push(e);
    cv_.notify_all();
}

void Thermal::Looper::loop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return !events_.empty(); });
        Event event = events_.front();
        events_.pop();
        lock.unlock();
        event.handler();
    }
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
