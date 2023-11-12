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

#include <aidl/android/hardware/thermal/BnThermal.h>

#include <mutex>
#include <thread>

#include "thermal-helper.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

struct CallbackSetting {
    CallbackSetting(std::shared_ptr<IThermalChangedCallback> callback, bool is_filter_type,
                    TemperatureType type)
        : callback(std::move(callback)), is_filter_type(is_filter_type), type(type) {}
    std::shared_ptr<IThermalChangedCallback> callback;
    bool is_filter_type;
    TemperatureType type;
};

class Thermal : public BnThermal {
  public:
    Thermal();
    ~Thermal() = default;
    ndk::ScopedAStatus getTemperatures(std::vector<Temperature> *_aidl_return) override;
    ndk::ScopedAStatus getTemperaturesWithType(TemperatureType type,
                                               std::vector<Temperature> *_aidl_return) override;

    ndk::ScopedAStatus getCoolingDevices(std::vector<CoolingDevice> *_aidl_return) override;
    ndk::ScopedAStatus getCoolingDevicesWithType(CoolingType type,
                                                 std::vector<CoolingDevice> *_aidl_return) override;

    ndk::ScopedAStatus getTemperatureThresholds(
            std::vector<TemperatureThreshold> *_aidl_return) override;
    ndk::ScopedAStatus getTemperatureThresholdsWithType(
            TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) override;

    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback> &callback) override;
    ndk::ScopedAStatus registerThermalChangedCallbackWithType(
            const std::shared_ptr<IThermalChangedCallback> &callback,
            TemperatureType type) override;
    ndk::ScopedAStatus unregisterThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback> &callback) override;
    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

    // Helper function for calling callbacks
    void sendThermalChangedCallback(const Temperature &t);

  private:
    class Looper {
      public:
        struct Event {
            std::function<void()> handler;
        };

        Looper() {
            thread_ = std::thread([&] { loop(); });
        }
        void addEvent(const Event &e);

      private:
        std::condition_variable cv_;
        std::queue<Event> events_;
        std::mutex mutex_;
        std::thread thread_;

        void loop();
    };

    ThermalHelper thermal_helper_;
    std::mutex thermal_callback_mutex_;
    std::vector<CallbackSetting> callbacks_;
    Looper looper_;

    ndk::ScopedAStatus getFilteredTemperatures(bool filterType, TemperatureType type,
                                               std::vector<Temperature> *_aidl_return);
    ndk::ScopedAStatus getFilteredCoolingDevices(bool filterType, CoolingType type,
                                                 std::vector<CoolingDevice> *_aidl_return);
    ndk::ScopedAStatus getFilteredTemperatureThresholds(
            bool filterType, TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return);
    ndk::ScopedAStatus registerThermalChangedCallback(
            const std::shared_ptr<IThermalChangedCallback> &callback, bool filterType,
            TemperatureType type);

    void dumpVirtualSensorInfo(std::ostringstream *dump_buf);
    void dumpThrottlingInfo(std::ostringstream *dump_buf);
    void dumpThrottlingRequestStatus(std::ostringstream *dump_buf);
    void dumpPowerRailInfo(std::ostringstream *dump_buf);
    void dumpStatsRecord(std::ostringstream *dump_buf, const StatsRecord &stats_record,
                         std::string_view line_prefix);
    void dumpThermalStats(std::ostringstream *dump_buf);
    void dumpThermalData(int fd);
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
