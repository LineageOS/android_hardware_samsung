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

bool WriteSysfs(const std::string& path, const std::string& data);
bool WriteEfs(const std::string& path, const std::string& data);

inline bool isChargerOnline(const ::aidl::android::hardware::health::HealthInfo& health_info) {
    return health_info.chargerAcOnline || health_info.chargerUsbOnline ||
           health_info.chargerWirelessOnline || health_info.chargerDockOnline;
}

}  // namespace health
}  // namespace samsung
}  // namespace hardware
