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

#include <aidl/android/hardware/thermal/CoolingType.h>
#include <aidl/android/hardware/thermal/TemperatureType.h>
#include <aidl/android/hardware/thermal/ThrottlingSeverity.h>
#include <json/value.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

constexpr size_t kThrottlingSeverityCount =
        std::distance(::ndk::enum_range<ThrottlingSeverity>().begin(),
                      ::ndk::enum_range<ThrottlingSeverity>().end());
using ThrottlingArray = std::array<float, static_cast<size_t>(kThrottlingSeverityCount)>;
using CdevArray = std::array<int, static_cast<size_t>(kThrottlingSeverityCount)>;
constexpr std::chrono::milliseconds kMinPollIntervalMs = std::chrono::milliseconds(2000);
constexpr std::chrono::milliseconds kUeventPollTimeoutMs = std::chrono::milliseconds(300000);
// Max number of time_in_state buckets is 20 in atoms
// VendorSensorCoolingDeviceStats, VendorTempResidencyStats
constexpr int kMaxStatsResidencyCount = 20;
constexpr int kMaxStatsThresholdCount = kMaxStatsResidencyCount - 1;

enum FormulaOption : uint32_t {
    COUNT_THRESHOLD = 0,
    WEIGHTED_AVG,
    MAXIMUM,
    MINIMUM,
};

template <typename T>
struct ThresholdList {
    std::optional<std::string> logging_name;
    std::vector<T> thresholds;
    explicit ThresholdList(std::optional<std::string> logging_name, std::vector<T> thresholds)
        : logging_name(logging_name), thresholds(thresholds) {}

    ThresholdList() = default;
    ThresholdList(const ThresholdList &) = default;
    ThresholdList &operator=(const ThresholdList &) = default;
    ThresholdList(ThresholdList &&) = default;
    ThresholdList &operator=(ThresholdList &&) = default;
    ~ThresholdList() = default;
};

template <typename T>
struct StatsInfo {
    // if bool, record all or none depending on flag
    // if set, check name present in set
    std::variant<bool, std::unordered_set<std::string> >
            record_by_default_threshold_all_or_name_set_;
    // map name to list of thresholds
    std::unordered_map<std::string, std::vector<ThresholdList<T> > > record_by_threshold;
    void clear() {
        record_by_default_threshold_all_or_name_set_ = false;
        record_by_threshold.clear();
    }
};

struct StatsConfig {
    StatsInfo<float> sensor_stats_info;
    StatsInfo<int> cooling_device_request_info;
    void clear() {
        sensor_stats_info.clear();
        cooling_device_request_info.clear();
    }
};

enum SensorFusionType : uint32_t {
    SENSOR = 0,
    ODPM,
};

struct VirtualSensorInfo {
    std::vector<std::string> linked_sensors;
    std::vector<SensorFusionType> linked_sensors_type;
    std::vector<float> coefficients;
    float offset;
    std::vector<std::string> trigger_sensors;
    FormulaOption formula;
};

struct VirtualPowerRailInfo {
    std::vector<std::string> linked_power_rails;
    std::vector<float> coefficients;
    float offset;
    FormulaOption formula;
};

// The method when the ODPM power is lower than threshold
enum ReleaseLogic : uint32_t {
    INCREASE = 0,      // Increase throttling by step
    DECREASE,          // Decrease throttling by step
    STEPWISE,          // Support both increase and decrease logix
    RELEASE_TO_FLOOR,  // Release throttling to floor directly
    NONE,
};

struct BindedCdevInfo {
    CdevArray limit_info;
    ThrottlingArray power_thresholds;
    ReleaseLogic release_logic;
    ThrottlingArray cdev_weight_for_pid;
    CdevArray cdev_ceiling;
    int max_release_step;
    int max_throttle_step;
    CdevArray cdev_floor_with_power_link;
    std::string power_rail;
    // The flag for activate release logic when power is higher than power threshold
    bool high_power_check;
    // The flag for only triggering throttling until all power samples are collected
    bool throttling_with_power_link;
};

struct ThrottlingInfo {
    ThrottlingArray k_po;
    ThrottlingArray k_pu;
    ThrottlingArray k_i;
    ThrottlingArray k_d;
    ThrottlingArray i_max;
    ThrottlingArray max_alloc_power;
    ThrottlingArray min_alloc_power;
    ThrottlingArray s_power;
    ThrottlingArray i_cutoff;
    float i_default;
    int tran_cycle;
    std::unordered_map<std::string, ThrottlingArray> excluded_power_info_map;
    std::unordered_map<std::string, BindedCdevInfo> binded_cdev_info_map;
};

struct SensorInfo {
    TemperatureType type;
    ThrottlingArray hot_thresholds;
    ThrottlingArray cold_thresholds;
    ThrottlingArray hot_hysteresis;
    ThrottlingArray cold_hysteresis;
    std::string temp_path;
    float vr_threshold;
    float multiplier;
    std::chrono::milliseconds polling_delay;
    std::chrono::milliseconds passive_delay;
    std::chrono::milliseconds time_resolution;
    bool send_cb;
    bool send_powerhint;
    bool is_watch;
    bool is_hidden;
    std::unique_ptr<VirtualSensorInfo> virtual_sensor_info;
    std::shared_ptr<ThrottlingInfo> throttling_info;
};

struct CdevInfo {
    CoolingType type;
    std::string read_path;
    std::string write_path;
    std::vector<float> state2power;
    int max_state;
};

struct PowerRailInfo {
    std::string rail;
    int power_sample_count;
    std::chrono::milliseconds power_sample_delay;
    std::unique_ptr<VirtualPowerRailInfo> virtual_power_rail_info;
};

bool ParseThermalConfig(std::string_view config_path, Json::Value *config);
bool ParseSensorInfo(const Json::Value &config,
                     std::unordered_map<std::string, SensorInfo> *sensors_parsed);
bool ParseCoolingDevice(const Json::Value &config,
                        std::unordered_map<std::string, CdevInfo> *cooling_device_parsed);
bool ParsePowerRailInfo(const Json::Value &config,
                        std::unordered_map<std::string, PowerRailInfo> *power_rail_parsed);
bool ParseStatsConfig(const Json::Value &config,
                      const std::unordered_map<std::string, SensorInfo> &sensor_info_map_,
                      const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_,
                      StatsConfig *stats_config);
}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
