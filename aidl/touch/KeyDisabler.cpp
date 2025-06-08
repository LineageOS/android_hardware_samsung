/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include "KeyDisabler.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

bool KeyDisabler::isSupported() {
    std::ofstream file(KEY_DISABLER_NODE);
    return file.good();
}

ndk::ScopedAStatus KeyDisabler::getEnabled(bool* _aidl_return) {
    std::ifstream file(KEY_DISABLER_NODE);
    int status = -1;

    if (file.is_open()) {
        file >> status;
    }

    *_aidl_return = status == 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus KeyDisabler::setEnabled(bool enabled) {
    std::ofstream file(KEY_DISABLER_NODE);
    file << (enabled ? "0" : "1");

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
