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

#define LOG_TAG "DisplayModesService"

#include "DisplayModes.h"
#include <android-base/logging.h>
#include <fstream>

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

static constexpr const char* kModePath = "/sys/class/mdnie/mdnie/mode";
static constexpr const char* kModeMaxPath = "/sys/class/mdnie/mdnie/mode_max";
static constexpr const char* kDefaultPath = "/data/vendor/display/.displaymodedefault";

const std::map<int32_t, std::string> DisplayModes::kModeMap = {
    // clang-format off
    {0, "Dynamic"},
    {1, "Standard"},
    {2, "Natural"},
    {3, "Cinema"},
    {4, "Adaptive"},
    {5, "Reading"},
    // clang-format on
};

DisplayModes::DisplayModes() : mDefaultModeId(0) {
    std::ifstream defaultFile(kDefaultPath);
    int value;

    defaultFile >> value;
    LOG(DEBUG) << "Default file read result " << value << " fail " << defaultFile.fail();
    if (defaultFile.fail()) {
        return;
    }

    for (const auto& entry : kModeMap) {
        if (value == entry.first) {
            mDefaultModeId = entry.first;
            break;
        }
    }

    setDisplayMode(mDefaultModeId, false);
}

bool DisplayModes::isSupported() {
    std::ofstream modeFile(kModePath);
    return modeFile.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IDisplayModes follow.
Return<void> DisplayModes::getDisplayModes(getDisplayModes_cb resultCb) {
    std::ifstream maxModeFile(kModeMaxPath);
    int value;
    std::vector<DisplayMode> modes;
    if (!maxModeFile.fail()) {
        maxModeFile >> value;
    } else {
        value = kModeMap.size();
    }
    for (const auto& entry : kModeMap) {
        if (entry.first < value) modes.push_back({entry.first, entry.second});
    }
    resultCb(modes);
    return Void();
}

Return<void> DisplayModes::getCurrentDisplayMode(getCurrentDisplayMode_cb resultCb) {
    int32_t currentModeId = mDefaultModeId;
    std::ifstream modeFile(kModePath);
    int value;
    modeFile >> value;
    if (!modeFile.fail()) {
        for (const auto& entry : kModeMap) {
            if (value == entry.first) {
                currentModeId = entry.first;
                break;
            }
        }
    }
    resultCb({currentModeId, kModeMap.at(currentModeId)});
    return Void();
}

Return<void> DisplayModes::getDefaultDisplayMode(getDefaultDisplayMode_cb resultCb) {
    resultCb({mDefaultModeId, kModeMap.at(mDefaultModeId)});
    return Void();
}

Return<bool> DisplayModes::setDisplayMode(int32_t modeID, bool makeDefault) {
    const auto iter = kModeMap.find(modeID);
    if (iter == kModeMap.end()) {
        return false;
    }
    std::ofstream modeFile(kModePath);
    modeFile << iter->first;
    if (modeFile.fail()) {
        return false;
    }

    if (makeDefault) {
        std::ofstream defaultFile(kDefaultPath);
        defaultFile << iter->first;
        if (defaultFile.fail()) {
            return false;
        }
        mDefaultModeId = iter->first;
    }
    return true;
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
