/*
 * SPDX-FileCopyrightText: 2019-2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>
#include <livedisplay/samsung/SunlightEnhancementExynos.h>

#include <climits>
#include <fstream>

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

static constexpr const char* kLUXPath = "/sys/class/mdnie/mdnie/lux";

bool SunlightEnhancementExynos::isSupported() {
    std::fstream file(kLUXPath, file.in | file.out);
    return file.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnSunlightEnhancement follow.
ndk::ScopedAStatus SunlightEnhancementExynos::getEnabled(bool* _aidl_return) {
    std::string tmp;
    int32_t contents = 0;

    if (!ReadFileToString(kLUXPath, &tmp)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    contents = std::stoi(Trim(tmp));

    *_aidl_return = contents > 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SunlightEnhancementExynos::setEnabled(bool enabled) {
    if (!WriteStringToFile(enabled ? INT_MAX : "0", kLUXPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
