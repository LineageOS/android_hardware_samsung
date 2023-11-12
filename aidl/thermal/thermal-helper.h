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

#include <aidl/android/hardware/thermal/IThermal.h>

#include <array>
#include <chrono>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "utils/power_files.h"
#include "utils/powerhal_helper.h"
#include "utils/thermal_files.h"
#include "utils/thermal_info.h"
#include "utils/thermal_stats_helper.h"
#include "utils/thermal_throttling.h"
#include "utils/thermal_watcher.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::sp;

using NotificationCallback = std::function<void(const Temperature &t)>;

// Get thermal_zone type
bool getThermalZoneTypeById(int tz_id, std::string *);

struct ThermalSample {
    float temp;
    boot_clock::time_point timestamp;
};

struct EmulSetting {
    float emul_temp;
    int emul_severity;
    bool pending_update;
};

struct SensorStatus {
    ThrottlingSeverity severity;
    ThrottlingSeverity prev_hot_severity;
    ThrottlingSeverity prev_cold_severity;
    ThrottlingSeverity prev_hint_severity;
    boot_clock::time_point last_update_time;
    ThermalSample thermal_cached;
    std::unique_ptr<EmulSetting> emul_setting;
};

class ThermalHelper {
  public:
    explicit ThermalHelper(const NotificationCallback &cb);
    ~ThermalHelper() = default;

    bool fillCurrentTemperatures(bool filterType, bool filterCallback, TemperatureType type,
                                 std::vector<Temperature> *temperatures);
    bool fillTemperatureThresholds(bool filterType, TemperatureType type,
                                   std::vector<TemperatureThreshold> *thresholds) const;
    bool fillCurrentCoolingDevices(bool filterType, CoolingType type,
                                   std::vector<CoolingDevice> *coolingdevices) const;
    bool emulTemp(std::string_view target_sensor, const float temp);
    bool emulSeverity(std::string_view target_sensor, const int severity);
    bool emulClear(std::string_view target_sensor);

    // Disallow copy and assign.
    ThermalHelper(const ThermalHelper &) = delete;
    void operator=(const ThermalHelper &) = delete;

    bool isInitializedOk() const { return is_initialized_; }

    // Read the temperature of a single sensor.
    bool readTemperature(std::string_view sensor_name, Temperature *out);
    bool readTemperature(
            std::string_view sensor_name, Temperature *out,
            std::pair<ThrottlingSeverity, ThrottlingSeverity> *throtting_status = nullptr,
            const bool force_sysfs = false);

    bool readTemperatureThreshold(std::string_view sensor_name, TemperatureThreshold *out) const;
    // Read the value of a single cooling device.
    bool readCoolingDevice(std::string_view cooling_device, CoolingDevice *out) const;
    // Get SensorInfo Map
    const std::unordered_map<std::string, SensorInfo> &GetSensorInfoMap() const {
        return sensor_info_map_;
    }
    // Get CdevInfo Map
    const std::unordered_map<std::string, CdevInfo> &GetCdevInfoMap() const {
        return cooling_device_info_map_;
    }
    // Get SensorStatus Map
    const std::unordered_map<std::string, SensorStatus> &GetSensorStatusMap() const {
        std::shared_lock<std::shared_mutex> _lock(sensor_status_map_mutex_);
        return sensor_status_map_;
    }
    // Get ThermalThrottling Map
    const std::unordered_map<std::string, ThermalThrottlingStatus> &GetThermalThrottlingStatusMap()
            const {
        return thermal_throttling_.GetThermalThrottlingStatusMap();
    }
    // Get PowerRailInfo Map
    const std::unordered_map<std::string, PowerRailInfo> &GetPowerRailInfoMap() const {
        return power_files_.GetPowerRailInfoMap();
    }

    // Get PowerStatus Map
    const std::unordered_map<std::string, PowerStatus> &GetPowerStatusMap() const {
        return power_files_.GetPowerStatusMap();
    }

    // Get Thermal Stats Sensor Map
    const std::unordered_map<std::string, SensorTempStats> GetSensorTempStatsSnapshot() {
        return thermal_stats_helper_.GetSensorTempStatsSnapshot();
    }
    // Get Thermal Stats Sensor, Binded Cdev State Request Map
    const std::unordered_map<std::string, std::unordered_map<std::string, ThermalStats<int>>>
    GetSensorCoolingDeviceRequestStatsSnapshot() {
        return thermal_stats_helper_.GetSensorCoolingDeviceRequestStatsSnapshot();
    }

    void sendPowerExtHint(const Temperature &t);
    bool isAidlPowerHalExist() { return power_hal_service_.isAidlPowerHalExist(); }
    bool isPowerHalConnected() { return power_hal_service_.isPowerHalConnected(); }
    bool isPowerHalExtConnected() { return power_hal_service_.isPowerHalExtConnected(); }

  private:
    bool initializeSensorMap(const std::unordered_map<std::string, std::string> &path_map);
    bool initializeCoolingDevices(const std::unordered_map<std::string, std::string> &path_map);
    bool isSubSensorValid(std::string_view sensor_data, const SensorFusionType sensor_fusion_type);
    void setMinTimeout(SensorInfo *sensor_info);
    void initializeTrip(const std::unordered_map<std::string, std::string> &path_map,
                        std::set<std::string> *monitored_sensors, bool thermal_genl_enabled);
    void clearAllThrottling();
    // For thermal_watcher_'s polling thread, return the sleep interval
    std::chrono::milliseconds thermalWatcherCallbackFunc(
            const std::set<std::string> &uevent_sensors);
    // Return hot and cold severity status as std::pair
    std::pair<ThrottlingSeverity, ThrottlingSeverity> getSeverityFromThresholds(
            const ThrottlingArray &hot_thresholds, const ThrottlingArray &cold_thresholds,
            const ThrottlingArray &hot_hysteresis, const ThrottlingArray &cold_hysteresis,
            ThrottlingSeverity prev_hot_severity, ThrottlingSeverity prev_cold_severity,
            float value) const;
    // Read sensor data according to the type
    bool readDataByType(std::string_view sensor_data, float *reading_value,
                        const SensorFusionType type, const bool force_no_cache,
                        std::map<std::string, float> *sensor_log_map);
    // Read temperature data according to thermal sensor's info
    bool readThermalSensor(std::string_view sensor_name, float *temp, const bool force_sysfs,
                           std::map<std::string, float> *sensor_log_map);
    bool connectToPowerHal();
    void updateSupportedPowerHints();
    void updateCoolingDevices(const std::vector<std::string> &cooling_devices_to_update);
    sp<ThermalWatcher> thermal_watcher_;
    PowerFiles power_files_;
    ThermalFiles thermal_sensors_;
    ThermalFiles cooling_devices_;
    ThermalThrottling thermal_throttling_;
    bool is_initialized_;
    const NotificationCallback cb_;
    std::unordered_map<std::string, CdevInfo> cooling_device_info_map_;
    std::unordered_map<std::string, SensorInfo> sensor_info_map_;
    std::unordered_map<std::string, std::unordered_map<ThrottlingSeverity, ThrottlingSeverity>>
            supported_powerhint_map_;
    PowerHalService power_hal_service_;
    ThermalStatsHelper thermal_stats_helper_;
    mutable std::shared_mutex sensor_status_map_mutex_;
    std::unordered_map<std::string, SensorStatus> sensor_status_map_;
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
