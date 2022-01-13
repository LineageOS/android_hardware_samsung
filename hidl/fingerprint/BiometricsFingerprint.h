/*
 * Copyright (C) 2019-2022 The LineageOS Project
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

#pragma once

#include <chrono>
#include <thread>

#ifdef HAS_FINGERPRINT_GESTURES
#include <linux/uinput.h>
#endif

#include <hardware/fingerprint.h>
#include <hardware/hardware.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <android/hardware/biometrics/fingerprint/2.3/IBiometricsFingerprint.h>
#include <android/hardware/biometrics/fingerprint/2.1/types.h>

#include "VendorConstants.h"

namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace V2_3 {
namespace implementation {

using namespace std::chrono_literals;

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::biometrics::fingerprint::V2_3::IBiometricsFingerprint;
using ::android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback;
using ::android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo;
using ::android::hardware::biometrics::fingerprint::V2_1::FingerprintError;
using ::android::hardware::biometrics::fingerprint::V2_1::RequestStatus;

struct BiometricsFingerprint : public IBiometricsFingerprint {
    BiometricsFingerprint();
    ~BiometricsFingerprint();

    // Method to wrap legacy HAL with BiometricsFingerprint class
    static IBiometricsFingerprint* getInstance();

    // Methods from ::android::hardware::biometrics::fingerprint::V2_3::IBiometricsFingerprint follow.
    Return<uint64_t> setNotify(
        const sp<IBiometricsFingerprintClientCallback>& clientCallback) override;
    Return<uint64_t> preEnroll() override;
    Return<RequestStatus> enroll(const hidl_array<uint8_t, 69>& hat, uint32_t gid,
                                 uint32_t timeoutSec) override;
    Return<RequestStatus> postEnroll() override;
    Return<uint64_t> getAuthenticatorId() override;
    Return<RequestStatus> cancel() override;
    Return<RequestStatus> enumerate() override;
    Return<RequestStatus> remove(uint32_t gid, uint32_t fid) override;
    Return<RequestStatus> setActiveGroup(uint32_t gid, const hidl_string& storePath) override;
    Return<RequestStatus> authenticate(uint64_t operationId, uint32_t gid) override;
    Return<bool> isUdfps(uint32_t sensorID) override;
    Return<void> onFingerDown(uint32_t x, uint32_t y, float minor, float major) override;
    Return<void> onFingerUp() override;

  private:
    bool openHal();
    int request(int cmd, int param);
    int waitForSensor(std::chrono::milliseconds pollWait, std::chrono::milliseconds timeOut);
    static void notify(
        const fingerprint_msg_t* msg); /* Static callback for legacy HAL implementation */
    void handleEvent(int eventCode);
    static Return<RequestStatus> ErrorFilter(int32_t error);
    static FingerprintError VendorErrorFilter(int32_t error, int32_t* vendorCode);
    static FingerprintAcquiredInfo VendorAcquiredFilter(int32_t error, int32_t* vendorCode);
    static BiometricsFingerprint* sInstance;

    std::mutex mClientCallbackMutex;
    sp<IBiometricsFingerprintClientCallback> mClientCallback;
    bool mIsUdfps;
#ifdef HAS_FINGERPRINT_GESTURES
    int uinputFd;
#endif

    int (*ss_fingerprint_close)();
    int (*ss_fingerprint_open)(const char* id);

    int (*ss_set_notify_callback)(fingerprint_notify_t notify);
    uint64_t (*ss_fingerprint_pre_enroll)();
    int (*ss_fingerprint_enroll)(const hw_auth_token_t* hat, uint32_t gid, uint32_t timeout_sec);
    int (*ss_fingerprint_post_enroll)();
    uint64_t (*ss_fingerprint_get_auth_id)();
    int (*ss_fingerprint_cancel)();
    int (*ss_fingerprint_enumerate)();
    int (*ss_fingerprint_remove)(uint32_t gid, uint32_t fid);
    int (*ss_fingerprint_set_active_group)(uint32_t gid, const char* store_path);
    int (*ss_fingerprint_authenticate)(uint64_t operation_id, uint32_t gid);
    int (*ss_fingerprint_request)(uint32_t cmd, char *inBuf, uint32_t inBuf_length, char *outBuf, uint32_t outBuf_length, uint32_t param);
};

}  // namespace implementation
}  // namespace V2_3
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace hardware
}  // namespace android
