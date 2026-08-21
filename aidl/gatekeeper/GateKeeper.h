/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <aidl/android/hardware/gatekeeper/BnGatekeeper.h>
#include <gatekeeper/gatekeeper_messages.h>

#include "SoftGateKeeper.h"

namespace aidl::android::hardware::gatekeeper {

class SoftGateKeeperDevice : public BnGatekeeper {
  public:
    SoftGateKeeperDevice(::gatekeeper::SoftGateKeeper&);
    /**
     * Enrolls password_payload, which should be derived from a user selected pin
     * or password, with the authentication factor private key used only for
     * enrolling authentication factor data.
     *
     * Returns: 0 on success or an error code less than 0 on error.
     * On error, enrolled_password_handle will not be allocated.
     */
    ::ndk::ScopedAStatus enroll(int32_t uid, const std::vector<uint8_t>& currentPasswordHandle,
                                const std::vector<uint8_t>& currentPassword,
                                const std::vector<uint8_t>& desiredPassword,
                                GatekeeperEnrollResponse* _aidl_return) override;
    /**
     * Verifies provided_password matches enrolled_password_handle.
     *
     * Implementations of this module may retain the result of this call
     * to attest to the recency of authentication.
     *
     * On success, writes the address of a verification token to auth_token,
     * usable to attest password verification to other trusted services. Clients
     * may pass NULL for this value.
     *
     * Returns: 0 on success or an error code less than 0 on error
     * On error, verification token will not be allocated
     */
    ::ndk::ScopedAStatus verify(int32_t uid, int64_t challenge,
                                const std::vector<uint8_t>& enrolledPasswordHandle,
                                const std::vector<uint8_t>& providedPassword,
                                GatekeeperVerifyResponse* _aidl_return) override;

    ::ndk::ScopedAStatus deleteAllUsers() override;

    ::ndk::ScopedAStatus deleteUser(int32_t uid) override;

  private:
    ::gatekeeper::SoftGateKeeper& impl_;
};

}  // namespace aidl::android::hardware::gatekeeper
