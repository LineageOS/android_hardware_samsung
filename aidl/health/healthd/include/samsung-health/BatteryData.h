/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/health/HealthInfo.h>

namespace hardware {
namespace samsung {
namespace health {

class BatteryData {
  public:
    BatteryData();
    void UpdatePrevBattData();
    void UpdateAgingHistory(const ::aidl::android::hardware::health::HealthInfo& health_info);
    void FlushAgingHistory(int64_t now);
    void RestoreRtcStatus();
    void UpdateCapacityMax();

  private:
    const char* kCapacityMaxSysfs = "/sys/class/power_supply/battery/batt_capacity_max";
    const char* kCapacityMaxEfs = "/efs/Battery/batt_capacity_max";

    const char* kPrevBattSysfs = "/sys/class/power_supply/battery/prev_battery_data";
    const char* kPrevBattEfs = "/efs/FactoryApp/prev_batt_data";
    const char* kRtcStatusSysfs0 = "/sys/power/rtc_status";
    const char* kRtcStatusSysfs1 = "/sys/class/sec/rtc/rtc_status";
    const char* kRtcStatusEfs = "/efs/FactoryApp/rtc_status";
    const char* kBattTempChargeEfs = "/efs/FactoryApp/batt_temp_charge";
  
    int oldCapacityMax = -1;

    int64_t kAgingMaxElapsed = 7200;     // (2h): longer gaps (suspend) are discarded
    int kAgingFlushThreshold = 1800;     // 30min of accumulated time triggers a flush
    int kAgingChargeChangeBoost = 1800;  // a charge-state change forces the next flush

    static constexpr int kNumTempBuckets = 5;
    bool ah_have_prev_ = false;              // false until the first sample is recorded
    int64_t ah_last_time_ = 0;               // wall-clock seconds of the previous poll
    int ah_prev_temp_ = 0;                   // previous battery temperature (tenths degC)
    int ah_prev_level_ = 0;                  // previous battery level (%)
    bool ah_prev_charging_ = false;          // whether a charger was online at the previous poll
    int ah_accum_time_ = 0;                  // seconds accumulated since the last flush
    int32_t ah_cap_[kNumTempBuckets] = {};   // capacity (%) charged while in each temp bucket
    int64_t ah_time_[kNumTempBuckets] = {};  // seconds discharging in each temp bucket
    int64_t ah_ts_[kNumTempBuckets] = {};    // last time (s) each temp bucket was touched
};

}  // namespace health
}  // namespace samsung
}  // namespace hardware
