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

#include <fstream>

#include "AdaptiveBacklight.h"

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

bool AdaptiveBacklight::isSupported() {
    std::ofstream file("/sys/class/lcd/panel/power_reduce");
    return file.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IAdaptiveBacklight follow.
Return<bool> AdaptiveBacklight::isEnabled() {
    std::ifstream file("/sys/class/lcd/panel/power_reduce");
    int value;

    file >> value;

    return !file.fail() && value == 1;
}

Return<bool> AdaptiveBacklight::setEnabled(bool enabled) {
    std::ofstream file("/sys/class/lcd/panel/power_reduce");
    file << (enabled ? "1" : "0");
    return true;
}

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
