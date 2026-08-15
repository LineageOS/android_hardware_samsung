/*
 * Copyright (C) 2016 The Android Open Source Project
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
#define LOG_TAG "android.hardware.gatekeeper-service.samsung"

#include <endian.h>

#include <android-base/logging.h>
#include <dlfcn.h>

#include "GateKeeper.h"

using ::gatekeeper::EnrollRequest;
using ::gatekeeper::EnrollResponse;
using ::gatekeeper::ERROR_NONE;
using ::gatekeeper::ERROR_RETRY;
using ::gatekeeper::SizedBuffer;
using ::gatekeeper::VerifyRequest;
using ::gatekeeper::VerifyResponse;

namespace aidl::android::hardware::gatekeeper {

SGatekeeper::SGatekeeper() {
    int ret = hw_get_module_by_class(GATEKEEPER_HARDWARE_MODULE_ID, NULL, &module);
    device = NULL;

    if (!ret) {
        ret = gatekeeper_open(module, &device);
    }
    if (ret < 0) {
        LOG(ERROR) << "Unable to open GateKeeper HAL: " << ret;
        abort();
    }
}

SGatekeeper::~SGatekeeper() {
    if (device != nullptr) {
        int ret = gatekeeper_close(device);
        if (ret < 0) {
            LOG(ERROR) << "Unable to close GateKeeper HAL";
        }
    }
    dlclose(module->dso);
}

void sizedBuffer2AidlHWToken(const uint8_t* buffer,
                             android::hardware::security::keymint::HardwareAuthToken* aidlToken) {
    const hw_auth_token_t* authToken = reinterpret_cast<const hw_auth_token_t*>(buffer);
    aidlToken->challenge = authToken->challenge;
    aidlToken->userId = authToken->user_id;
    aidlToken->authenticatorId = authToken->authenticator_id;
    // these are in network order: translate to host
    aidlToken->authenticatorType =
            static_cast<android::hardware::security::keymint::HardwareAuthenticatorType>(
                    be32toh(authToken->authenticator_type));
    aidlToken->timestamp.milliSeconds = be64toh(authToken->timestamp);
    aidlToken->mac.insert(aidlToken->mac.begin(), std::begin(authToken->hmac),
                          std::end(authToken->hmac));
}

::ndk::ScopedAStatus SGatekeeper::enroll(int32_t uid,
                                         const std::vector<uint8_t>& currentPasswordHandle,
                                         const std::vector<uint8_t>& currentPassword,
                                         const std::vector<uint8_t>& desiredPassword,
                                         GatekeeperEnrollResponse* rsp) {
    if (desiredPassword.size() == 0) {
        LOG(ERROR) << "Desired password size is 0";
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    }

    if (currentPasswordHandle.size() > 0) {
        if (currentPasswordHandle.size() != sizeof(::gatekeeper::password_handle_t)) {
            LOG(ERROR) << "Password handle has wrong length";
            return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
        }
    }

    uint8_t* enrolled_password_handle = nullptr;
    uint32_t enrolled_password_handle_length = 0;

    int ret = device->enroll(device, uid, currentPasswordHandle.data(),
                             currentPasswordHandle.size(), currentPassword.data(),
                             currentPassword.size(), desiredPassword.data(), desiredPassword.size(),
                             &enrolled_password_handle, &enrolled_password_handle_length);

    if (ret > 0) {
        LOG(ERROR) << "Enroll response has a retry error";
        *rsp = {ERROR_RETRY_TIMEOUT, static_cast<int32_t>(ret), 0, {}};
        return ndk::ScopedAStatus::ok();
    } else if (ret != ERROR_NONE) {
        LOG(ERROR) << "Enroll response has an error: " << ret;
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    } else {
        const auto* password_handle =
                reinterpret_cast<const ::gatekeeper::password_handle_t*>(enrolled_password_handle);
        *rsp = {STATUS_OK, 0, static_cast<int64_t>(password_handle->user_id),
                std::vector<uint8_t>(enrolled_password_handle,
                                     enrolled_password_handle + enrolled_password_handle_length)};
    }
    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus SGatekeeper::verify(int32_t uid, int64_t challenge,
                                         const std::vector<uint8_t>& enrolledPasswordHandle,
                                         const std::vector<uint8_t>& providedPassword,
                                         GatekeeperVerifyResponse* rsp) {
    if (enrolledPasswordHandle.size() == 0) {
        LOG(ERROR) << "Enrolled password size is 0";
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    }

    if (enrolledPasswordHandle.size() > 0) {
        if (enrolledPasswordHandle.size() != sizeof(::gatekeeper::password_handle_t)) {
            LOG(ERROR) << "Password handle has wrong length";
            return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
        }
    }

    uint8_t* auth_token = nullptr;
    uint32_t auth_token_length = 0;
    bool request_reenroll = false;

    int ret = device->verify(device, uid, challenge, enrolledPasswordHandle.data(),
                             enrolledPasswordHandle.size(), providedPassword.data(),
                             providedPassword.size(), &auth_token, &auth_token_length,
                             &request_reenroll);

    if (ret > 0) {
        LOG(ERROR) << "Verify request response gave retry error";
        *rsp = {ERROR_RETRY_TIMEOUT, static_cast<int32_t>(ret), {}};
        return ndk::ScopedAStatus::ok();
    } else if (ret != ERROR_NONE) {
        LOG(ERROR) << "Verify request response gave error: " << ret;
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    } else {
        // On Success, return GatekeeperVerifyResponse with Success Status, timeout{0} and
        // valid HardwareAuthToken.
        *rsp = {request_reenroll ? STATUS_REENROLL : STATUS_OK, 0, {}};
        // Convert the hw_auth_token_t to HardwareAuthToken in the response.
        sizedBuffer2AidlHWToken(auth_token, &rsp->hardwareAuthToken);
    }
    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus SGatekeeper::deleteUser(int32_t /*uid*/) {
    LOG(ERROR) << "deleteUser is unimplemented";
    return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_NOT_IMPLEMENTED));
}

::ndk::ScopedAStatus SGatekeeper::deleteAllUsers() {
    LOG(ERROR) << "deleteAllUsers is unimplemented";
    return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_NOT_IMPLEMENTED));
}

}  // namespace aidl::android::hardware::gatekeeper
