/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

class UdfpsHandler {
public:
    void setFodPress(bool enable);
    void setFodRect(const std::string& fod_rect);
    template <typename T>
    void set(const std::string& path, const T& value);

private:
    static constexpr const char* TSP_CMD = "/sys/class/sec/tsp/cmd";
    static constexpr const char* TSP_CMD_LIST = "/sys/class/sec/tsp/cmd_list";
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
