/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#include <fstream>

#include "DisplayColorCalibrationExynos.h"

using android::base::ReadFileToString;
using android::base::Split;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

static constexpr const char* kRGBPath = "/sys/class/mdnie/mdnie/sensorRGB";

bool DisplayColorCalibrationExynos::isSupported() {
    std::fstream file(kRGBPath, file.in | file.out);
    return file.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnDisplayColorCalibration follow.
ndk::ScopedAStatus DisplayColorCalibrationExynos::getMaxValue(int32_t* _aidl_return) {
    *_aidl_return = 255;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibrationExynos::getMinValue(int32_t* _aidl_return) {
    *_aidl_return = 1;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibrationExynos::getCalibration(std::vector<int32_t>* _aidl_return) {
    std::vector<int32_t> rgb;
    std::string tmp;

    if (ReadFileToString(kRGBPath, &tmp)) {
        std::vector<std::string> colors = Split(Trim(tmp), " ");
        for (const std::string& color : colors) {
            rgb.push_back(std::stoi(color));
        }
    } else {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = rgb;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibrationExynos::setCalibration(const std::vector<int32_t>& rgb) {
    std::string contents;

    for (const int32_t& color : rgb) {
        contents += std::to_string(color) + " ";
    }

    if (!WriteStringToFile(Trim(contents), kRGBPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
