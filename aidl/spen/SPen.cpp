/*
 * SPDX-FileCopyrightText: 2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.samsung.hardware.spen-service"

#include <fstream>

#include "SPen.h"

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace spen {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value << std::endl;
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

SPen::SPen() {}

ndk::ScopedAStatus SPen::setCharging(bool charging, bool* _aidl_return) {
    set("/sys/class/sec/sec_epen/epen_ble_charging_mode", charging ? 1 : 0);

    return isCharging(_aidl_return);
}

ndk::ScopedAStatus SPen::isCharging(bool* _aidl_return) {
    *_aidl_return =
            get("/sys/class/sec/sec_epen/epen_ble_charging_mode", std::string("NG")) == "CHARGE";

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SPen::getMACAddress(std::string* _aidl_return) {
    *_aidl_return = get("/efs/spen/blespen_addr", std::string("00:00:00:00:00:00"));

    return ndk::ScopedAStatus::ok();
}

}  // namespace spen
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
