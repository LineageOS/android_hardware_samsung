/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>

#include <aidl/android/hardware/health/HealthInfo.h>

namespace aidl::vendor::samsung::hardware::health {

class SecBatteryMonitor {
  public:
    void Restore();
    void Persist(const ::aidl::android::hardware::health::HealthInfo& health_info);

  private:
    void PersistCableCount(const ::aidl::android::hardware::health::HealthInfo& health_info);
    void PersistIntNode(const char* sysfs_path, const char* efs_path, int* old_value);
    void PersistPrevBattData();
    void UpdateAgingHistory(const ::aidl::android::hardware::health::HealthInfo& health_info);
    void FlushAgingHistory(int64_t now);

    // Previous charger-online state
    bool prev_charger_online_ = false;
    // Last values mirrored to /efs (-1 = unknown; seeded from /efs in Restore()).
    int old_capacity_max_ = -1;
    int old_err_wthm_ = -1;

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

}  // namespace aidl::vendor::samsung::hardware::health
