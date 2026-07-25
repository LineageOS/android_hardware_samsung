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

class ShortDetection {
  public:
    ShortDetection();
    void RestoreCisdData();
    void UpdateCisdData();
    void UpdateCableCount(const ::aidl::android::hardware::health::HealthInfo& health_info);

  private:
    size_t kMaxCisdLen = 1024;  // larger is treated as broken data

    const char* kCableCountEfs = "/efs/FactoryApp/batt_cable_count";
    const char* kSysfsBase = "/sys/class/power_supply/battery";

    struct CisdDataEntry {
        const char* node;
        const char* efs;
        bool clearEfsData;
    };

    static constexpr CisdDataEntry kCisdDataEntries[] = {
            {"cisd_cable_data", "/efs/FactoryApp/cisd_cable_data", false},
            {"cisd_data", "/efs/FactoryApp/cisd_data", true},
            {"cisd_dcerr_data", "/efs/FactoryApp/cisd_dcerr_data", false},
            {"cisd_event_data", "/efs/FactoryApp/cisd_event_data", false},
            {"cisd_wc_data", "/efs/FactoryApp/cisd_wc_data", false},
            {"cisd_pd_data", "/efs/FactoryApp/cisd_pd_data", false},
            {"cisd_power_data", "/efs/FactoryApp/cisd_power_data", false},
            {"cisd_tx_data", "/efs/FactoryApp/cisd_tx_data", false},
    };

    bool prev_charger_online_ = false;
};

}  // namespace health
}  // namespace samsung
}  // namespace hardware
