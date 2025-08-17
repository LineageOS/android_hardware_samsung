/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "DisplayModesService"

#include <android-base/logging.h>

#include <fstream>

#include "DisplayModes.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
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
ndk::ScopedAStatus DisplayModes::getDisplayModes(std::vector<DisplayMode>* _aidl_return) {
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

    *_aidl_return = modes;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayModes::getCurrentDisplayMode(DisplayMode* _aidl_return) {
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

    *_aidl_return = DisplayMode{currentModeId, kModeMap.at(currentModeId)};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayModes::getDefaultDisplayMode(DisplayMode* _aidl_return) {
    *_aidl_return = DisplayMode{mDefaultModeId, kModeMap.at(mDefaultModeId)};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayModes::setDisplayMode(int32_t modeID, bool makeDefault) {
    const auto iter = kModeMap.find(modeID);
    if (iter == kModeMap.end()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    std::ofstream modeFile(kModePath);
    modeFile << iter->first;
    if (modeFile.fail()) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (makeDefault) {
        std::ofstream defaultFile(kDefaultPath);
        defaultFile << iter->first;
        if (defaultFile.fail()) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
        mDefaultModeId = iter->first;
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
