/*
 * Copyright (C) 2020 The LineageOS Project
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

#define LOG_TAG "android.hardware.power@1.0-service.exynos"

#include "Power.h"
#include <android-base/logging.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "samsung_lights.h"
#include "samsung_power.h"

namespace android {
namespace hardware {
namespace power {
namespace V1_0 {
namespace implementation {

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

Return<void> Power::setInteractive(bool interactive) {
    if (!initialized) {
        initialize();
    }

    if (!interactive) {
        int32_t panel_brightness = get(PANEL_BRIGHTNESS_NODE, -1);

        if (panel_brightness > 0) {
            LOG(VERBOSE) << "Moving to non-interactive state, but screen is still on,"
                         << "not disabling input devices";
            goto out;
        }
    }

    if (!sec_touchscreen.empty()) {
        set(sec_touchscreen, interactive ? "1" : "0");
    }

    if (!sec_touchkey.empty()) {
        if (!interactive) {
            int button_state = get(sec_touchkey, -1);

            if (button_state < 0) {
                LOG(ERROR) << "Failed to read touchkey state";
                goto out;
            }

            /*
             * If button_state is 0, the keys have been disabled by another component
             * (for example lineagehw), which means we don't want them to be enabled when resuming
             * from suspend.
             */
            if (button_state == 0) {
                touchkeys_blocked = true;
            }
        }

        if (!touchkeys_blocked) {
            set(sec_touchkey, interactive ? "1" : "0");
        }
    }

out:
    for (const std::string& interactivePath : cpuInteractivePaths) {
        set(interactivePath + "/io_is_busy", interactive ? "1" : "0");
    }

    return Void();
}

Return<void> Power::powerHint(PowerHint hint, int32_t data) {
    if (!initialized) {
        initialize();
    }

    /* Bail out if low-power mode is active */
    if (current_profile == PowerProfile::POWER_SAVE && hint != PowerHint::LOW_POWER &&
        hint != static_cast<PowerHint>(LineagePowerHint::SET_PROFILE)) {
        LOG(VERBOSE) << "PROFILE_POWER_SAVE active, ignoring hint " << static_cast<int32_t>(hint);
        return Void();
    }

    switch (hint) {
        case PowerHint::INTERACTION:
        case PowerHint::LAUNCH:
            sendBoostpulse();
            break;
        case PowerHint::LOW_POWER:
            setProfile(data ? PowerProfile::POWER_SAVE : PowerProfile::BALANCED);
            break;
        default:
            if (hint == static_cast<PowerHint>(LineagePowerHint::SET_PROFILE)) {
                setProfile(static_cast<PowerProfile>(data));
            } else if (hint == static_cast<PowerHint>(LineagePowerHint::CPU_BOOST)) {
                sendBoost(data);
            } else {
                LOG(INFO) << "Unknown power hint: " << static_cast<int32_t>(hint);
            }
            break;
    }
    return Void();
}

Return<void> Power::setFeature(Feature feature __unused, bool activate __unused) {
    if (!initialized) {
        initialize();
    }

#ifdef TAP_TO_WAKE_NODE
    if (feature == Feature::POWER_FEATURE_DOUBLE_TAP_TO_WAKE) {
        set(TAP_TO_WAKE_NODE, activate ? "1" : "0");
    }
#endif

    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) {
    _hidl_cb({}, Status::SUCCESS);
    return Void();
}

Return<int32_t> Power::getFeature(LineageFeature feature) {
    switch (feature) {
        case LineageFeature::SUPPORTED_PROFILES:
            return static_cast<int32_t>(PowerProfile::MAX);
        default:
            return -1;
    }
}

void Power::initialize() {
    findInputNodes();

    setProfile(PowerProfile::BALANCED);

    for (const std::string& interactivePath : cpuInteractivePaths) {
        hispeed_freqs.emplace_back(get<std::string>(interactivePath + "/hispeed_freq", ""));
    }

    for (const std::string& sysfsPath : cpuSysfsPaths) {
        max_freqs.emplace_back(get<std::string>(sysfsPath + "/cpufreq/scaling_max_freq", ""));
    }

    initialized = true;
}

void Power::findInputNodes() {
    std::error_code ec;
    for (auto& de : std::filesystem::directory_iterator("/sys/class/input/", ec)) {
        /* we are only interested in the input devices that we can access */
        if (ec || de.path().string().find("/sys/class/input/input") == std::string::npos) {
            continue;
        }

        for (auto& de2 : std::filesystem::directory_iterator(de.path(), ec)) {
            if (!ec && de2.path().string().find("/name") != std::string::npos) {
                std::string content = get<std::string>(de2.path(), "");
                if (content == "sec_touchkey") {
                    sec_touchkey = de.path().string().append("/enabled");
                    LOG(INFO) << "found sec_touchkey: " << sec_touchkey;
                } else if (content == "sec_touchscreen") {
                    sec_touchscreen = de.path().string().append("/enabled");
                    LOG(INFO) << "found sec_touchscreen: " << sec_touchscreen;
                }
            }
        }
    }
}

void Power::setProfile(PowerProfile profile) {
    if (current_profile == profile) {
        return;
    }

    switch (profile) {
        case PowerProfile::POWER_SAVE:
            // Limit to hispeed freq
            for (int i = 0; i < cpuSysfsPaths.size(); i++) {
                if (hispeed_freqs.size() > i && !hispeed_freqs.at(i).empty()) {
                    set(cpuSysfsPaths.at(i) + "/cpufreq/scaling_max_freq", hispeed_freqs.at(i));
                }
            }
            break;
        case PowerProfile::BALANCED:
        case PowerProfile::HIGH_PERFORMANCE:
            // Restore normal max freq
            for (int i = 0; i < cpuSysfsPaths.size(); i++) {
                if (max_freqs.size() > i && !max_freqs.at(i).empty()) {
                    set(cpuSysfsPaths.at(i) + "/cpufreq/scaling_max_freq", max_freqs.at(i));
                }
            }
            break;
        default:
            break;
    }

    current_profile = profile;
}

void Power::sendBoostpulse() {
    // the boostpulse node is only valid for the LITTLE cluster
    set(cpuInteractivePaths.front() + "/boostpulse", "1");
}

void Power::sendBoost(int duration_us) {
    set(cpuInteractivePaths.front() + "/boost", "1");

    usleep(duration_us);

    set(cpuInteractivePaths.front() + "/boost", "0");
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace power
}  // namespace hardware
}  // namespace android
