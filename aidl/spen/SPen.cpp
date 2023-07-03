/*
 * SPDX-FileCopyrightText: 2022-2023 The LineageOS Project
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

ndk::ScopedAStatus SPen::setCharging(bool in_charging, bool* _aidl_return) {
    set(SYSFS_CHARGING_NODE, in_charging ? 1 : 0);

    return isCharging(_aidl_return);
}

ndk::ScopedAStatus SPen::isCharging(bool* _aidl_return) {
    *_aidl_return = get(SYSFS_CHARGING_NODE, std::string(SPEN_STATE_NG)) == SPEN_STATE_CHARGE;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SPen::getMACAddress(std::string* _aidl_return) {
    std::ifstream file(SPEN_ADDR_PATH_VENDOR);
    if (file.good()) {
        *_aidl_return = get(SPEN_ADDR_PATH_VENDOR, std::string(SPEN_ADDR_DEFAULT));
    } else {
        *_aidl_return = get(SPEN_ADDR_PATH, std::string(SPEN_ADDR_DEFAULT));
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SPen::setMACAddress(const std::string& in_mac) {
    if (get(SPEN_ADDR_PATH_VENDOR, std::string(SPEN_ADDR_DEFAULT)) != in_mac) {
        set(SPEN_ADDR_PATH_VENDOR, in_mac);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace spen
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
