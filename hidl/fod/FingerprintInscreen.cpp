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

#define LOG_TAG "FingerprintInscreenService"

#include "FingerprintInscreen.h"
#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <fstream>

namespace vendor {
namespace lineage {
namespace biometrics {
namespace fingerprint {
namespace inscreen {
namespace V1_0 {
namespace implementation {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

static hidl_vec<int8_t> stringToVec(const std::string& str) {
    auto vec = hidl_vec<int8_t>();
    vec.resize(str.size() + 1);
    for (size_t i = 0; i < str.size(); ++i) {
        vec[i] = (int8_t) str[i];
    }
    vec[str.size()] = '\0';
    return vec;
}

FingerprintInscreen::FingerprintInscreen() {
    mSecBiometricsFingerprintService = ISecBiometricsFingerprint::getService();
}

void FingerprintInscreen::requestResult(int, const hidl_vec<int8_t>&) {
    // Ignore all results
}

Return<void> FingerprintInscreen::onStartEnroll() {
    return Void();
}

Return<void> FingerprintInscreen::onFinishEnroll() {
    return Void();
}

Return<void> FingerprintInscreen::onPress() {
    mPreviousBrightness = get(BRIGHTNESS_PATH, -1);
    set(BRIGHTNESS_PATH, 319);
    set(TSP_CMD_PATH, FOD_ENABLE);
    mSecBiometricsFingerprintService->request(SEM_FINGER_STATE, 0,
        SEM_PARAM_PRESSED, stringToVec(SEM_AOSP_FQNAME), FingerprintInscreen::requestResult);
    return Void();
}

Return<void> FingerprintInscreen::onRelease() {
    mSecBiometricsFingerprintService->request(SEM_FINGER_STATE, 0,
        SEM_PARAM_RELEASED, stringToVec(SEM_AOSP_FQNAME), FingerprintInscreen::requestResult);
    set(TSP_CMD_PATH, FOD_DISBLE);
    if (mPreviousBrightness > -1) {
        set(BRIGHTNESS_PATH, mPreviousBrightness);
    }
    return Void();
}

Return<void> FingerprintInscreen::onShowFODView() {
    return Void();
}

Return<void> FingerprintInscreen::onHideFODView() {
    set(TSP_CMD_PATH, FOD_DISBLE);
    if (mPreviousBrightness > -1) {
        set(BRIGHTNESS_PATH, mPreviousBrightness);
    }
    return Void();
}

Return<bool> FingerprintInscreen::handleAcquired(int32_t acquiredInfo, int32_t vendorCode) {
    std::lock_guard<std::mutex> _lock(mCallbackLock);
    if (mCallback == nullptr) {
        return false;
    }

    if (acquiredInfo == FINGERPRINT_ACQUIRED_VENDOR) {
        if (vendorCode == 9002) {
            Return<void> ret = mCallback->onFingerDown();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerDown() error: " << ret.description();
            }
            return true;
        } else if (vendorCode == 9001) {
            Return<void> ret = mCallback->onFingerUp();
            if (!ret.isOk()) {
                LOG(ERROR) << "FingerUp() error: " << ret.description();
            } else {
                onRelease();
            }
            return true;
        }
    }
    return true;
}

Return<bool> FingerprintInscreen::handleError(int32_t, int32_t) {
    return false;
}

Return<void> FingerprintInscreen::setLongPressEnabled(bool) {
    return Void();
}

Return<int32_t> FingerprintInscreen::getDimAmount(int32_t cur_brightness) {
    if (cur_brightness <= 12) {
        return 2200 / std::max(cur_brightness, 10);
    } else if (cur_brightness <= 16) {
        return 3000 / cur_brightness;
    } else if (cur_brightness <= 20) {
        return 3700 / cur_brightness;
    } else {
        return 4400 / cur_brightness;
    }
}

Return<bool> FingerprintInscreen::shouldBoostBrightness() {
    return false;
}

Return<void> FingerprintInscreen::setCallback(const sp<IFingerprintInscreenCallback>& callback) {
    {
        std::lock_guard<std::mutex> _lock(mCallbackLock);
        mCallback = callback;
    }
    return Void();
}

Return<int32_t> FingerprintInscreen::getPositionX() {
    return FOD_SENSOR_X;
}

Return<int32_t> FingerprintInscreen::getPositionY() {
    return FOD_SENSOR_Y;
}

Return<int32_t> FingerprintInscreen::getSize() {
    return FOD_SENSOR_SIZE;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace inscreen
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace lineage
}  // namespace vendor
