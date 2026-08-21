/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <mutex>

#include <aidl/android/hardware/security/sharedsecret/BnSharedSecret.h>
#include <aidl/android/hardware/security/sharedsecret/SharedSecretParameters.h>
#include <keymaster/km_openssl/soft_keymaster_enforcement.h>

namespace aidl::android::hardware::security::sharedsecret {

class SoftSharedSecret : public BnSharedSecret {
  public:
    ::ndk::ScopedAStatus getSharedSecretParameters(SharedSecretParameters* params) override;
    ::ndk::ScopedAStatus computeSharedSecret(const std::vector<SharedSecretParameters>& params,
                                             std::vector<uint8_t>* sharingCheck) override;

    keymaster::KeymasterKeyBlob HmacKey() const;

  private:
    mutable std::mutex mutex_;
    std::vector<std::uint8_t> seed_;
    std::vector<std::uint8_t> nonce_;
    keymaster::KeymasterKeyBlob hmac_key_;
};

}  // namespace aidl::android::hardware::security::sharedsecret
