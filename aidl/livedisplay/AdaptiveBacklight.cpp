/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#include <fstream>

#include "AdaptiveBacklight.h"

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

static constexpr const char* kBacklightPath = "/sys/class/lcd/panel/power_reduce";

bool AdaptiveBacklight::isSupported() {
    std::fstream backlight(kBacklightPath, backlight.in | backlight.out);
    return backlight.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnAdaptiveBacklight follow.
ndk::ScopedAStatus AdaptiveBacklight::getEnabled(bool* _aidl_return) {
    std::string tmp;
    int32_t contents = 0;

    if (!ReadFileToString(kBacklightPath, &tmp)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    contents = std::stoi(Trim(tmp));

    *_aidl_return = contents > 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AdaptiveBacklight::setEnabled(bool enabled) {
    if (!WriteStringToFile(enabled ? "1" : "0", kBacklightPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
