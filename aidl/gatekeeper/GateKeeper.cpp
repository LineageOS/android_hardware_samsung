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
#define LOG_TAG "android.hardware.gatekeeper-service.nonsecure"

#include <endian.h>

#include <android-base/logging.h>

#include "GateKeeper.h"

using ::gatekeeper::EnrollRequest;
using ::gatekeeper::EnrollResponse;
using ::gatekeeper::ERROR_NONE;
using ::gatekeeper::ERROR_RETRY;
using ::gatekeeper::SizedBuffer;
using ::gatekeeper::VerifyRequest;
using ::gatekeeper::VerifyResponse;

namespace aidl::android::hardware::gatekeeper {

SizedBuffer vec2sized_buffer(const std::vector<uint8_t>& vec) {
    if (vec.size() == 0 || vec.size() > std::numeric_limits<uint32_t>::max()) {
        return {};
    }
    auto unused = new uint8_t[vec.size()];
    std::copy(vec.begin(), vec.end(), unused);
    return {unused, static_cast<uint32_t>(vec.size())};
}

void sizedBuffer2AidlHWToken(SizedBuffer& buffer,
                             android::hardware::security::keymint::HardwareAuthToken* aidlToken) {
    const hw_auth_token_t* authToken =
            reinterpret_cast<const hw_auth_token_t*>(buffer.Data<uint8_t>());
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

SoftGateKeeperDevice::SoftGateKeeperDevice(::gatekeeper::SoftGateKeeper& impl) : impl_(impl) {}

::ndk::ScopedAStatus SoftGateKeeperDevice::enroll(int32_t uid,
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

    EnrollRequest request(uid, vec2sized_buffer(currentPasswordHandle),
                          vec2sized_buffer(desiredPassword), vec2sized_buffer(currentPassword));
    EnrollResponse response;
    impl_.Enroll(request, &response);
    if (response.error == ERROR_RETRY) {
        LOG(ERROR) << "Enroll response has a retry error";
        *rsp = {ERROR_RETRY_TIMEOUT, static_cast<int32_t>(response.retry_timeout), 0, {}};
        return ndk::ScopedAStatus::ok();
    } else if (response.error != ERROR_NONE) {
        LOG(ERROR) << "Enroll response has an error: " << response.error;
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    } else {
        const ::gatekeeper::password_handle_t* password_handle =
                response.enrolled_password_handle.Data<::gatekeeper::password_handle_t>();
        *rsp = {STATUS_OK,
                0,
                static_cast<int64_t>(password_handle->user_id),
                {response.enrolled_password_handle.Data<uint8_t>(),
                 (response.enrolled_password_handle.Data<uint8_t>() +
                  response.enrolled_password_handle.size())}};
    }
    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus SoftGateKeeperDevice::verify(
        int32_t uid, int64_t challenge, const std::vector<uint8_t>& enrolledPasswordHandle,
        const std::vector<uint8_t>& providedPassword, GatekeeperVerifyResponse* rsp) {
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

    VerifyRequest request(uid, challenge, vec2sized_buffer(enrolledPasswordHandle),
                          vec2sized_buffer(providedPassword));
    VerifyResponse response;
    impl_.Verify(request, &response);

    if (response.error == ERROR_RETRY) {
        LOG(ERROR) << "Verify request response gave retry error";
        *rsp = {ERROR_RETRY_TIMEOUT, static_cast<int32_t>(response.retry_timeout), {}};
        return ndk::ScopedAStatus::ok();
    } else if (response.error != ERROR_NONE) {
        LOG(ERROR) << "Verify request response gave error: " << response.error;
        return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_GENERAL_FAILURE));
    } else {
        // On Success, return GatekeeperVerifyResponse with Success Status, timeout{0} and
        // valid HardwareAuthToken.
        *rsp = {response.request_reenroll ? STATUS_REENROLL : STATUS_OK, 0, {}};
        // Convert the hw_auth_token_t to HardwareAuthToken in the response.
        sizedBuffer2AidlHWToken(response.auth_token, &rsp->hardwareAuthToken);
    }
    return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus SoftGateKeeperDevice::deleteUser(int32_t /*uid*/) {
    LOG(ERROR) << "deleteUser is unimplemented";
    return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_NOT_IMPLEMENTED));
}

::ndk::ScopedAStatus SoftGateKeeperDevice::deleteAllUsers() {
    LOG(ERROR) << "deleteAllUsers is unimplemented";
    return ndk::ScopedAStatus(AStatus_fromServiceSpecificError(ERROR_NOT_IMPLEMENTED));
}

}  // namespace aidl::android::hardware::gatekeeper
