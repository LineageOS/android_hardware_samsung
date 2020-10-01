/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#include <fstream>

#include "DisplayColorCalibration.h"

using android::base::ReadFileToString;
using android::base::Split;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

bool DisplayColorCalibration::isSupported() {
    std::fstream rgb(FILE_RGB, rgb.in | rgb.out);

    return rgb.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IDisplayColorCalibration follow.
Return<int32_t> DisplayColorCalibration::getMaxValue() {
    return 32768;
}

Return<int32_t> DisplayColorCalibration::getMinValue() {
    return 255;
}

Return<void> DisplayColorCalibration::getCalibration(getCalibration_cb _hidl_cb) {
    std::vector<int32_t> rgb;
    std::string tmp;

    if (ReadFileToString(FILE_RGB, &tmp)) {
        std::vector<std::string> colors = Split(Trim(tmp), " ");
        for (const std::string& color : colors) {
            rgb.push_back(std::stoi(color));
        }
    }

    _hidl_cb(rgb);
    return Void();
}

Return<bool> DisplayColorCalibration::setCalibration(const hidl_vec<int32_t>& rgb) {
    std::string contents;

    for (const int32_t& color : rgb) {
        contents += std::to_string(color) + " ";
    }

    return WriteStringToFile(Trim(contents), FILE_RGB, true);
}

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
