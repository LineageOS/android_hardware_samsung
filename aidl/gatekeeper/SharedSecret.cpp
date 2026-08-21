/*
 * Copyright 2024, The Android Open Source Project
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

#include "SharedSecret.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

#include <openssl/rand.h>

#include <aidl/android/hardware/security/sharedsecret/BnSharedSecret.h>
#include <aidl/android/hardware/security/sharedsecret/SharedSecretParameters.h>
#include <android-base/logging.h>
#include <keymaster/android_keymaster_messages.h>
#include <keymaster/android_keymaster_utils.h>
#include <keymaster/km_openssl/ckdf.h>
#include <keymaster/km_openssl/hmac.h>

namespace aidl::android::hardware::security::sharedsecret {

namespace {

inline ::ndk::ScopedAStatus kmError2ScopedAStatus(const keymaster_error_t value) {
    return (value == KM_ERROR_OK ? ::ndk::ScopedAStatus::ok()
                                 : ::ndk::ScopedAStatus(AStatus_fromServiceSpecificError(
                                           static_cast<int32_t>(value))));
}

}  // namespace

::ndk::ScopedAStatus SoftSharedSecret::getSharedSecretParameters(
        SharedSecretParameters* out_params) {
    std::lock_guard lock(mutex_);
    if (seed_.empty()) {
        seed_.resize(32, 0);
    }
    out_params->seed = seed_;
    if (nonce_.empty()) {
        nonce_.resize(32, 0);
        RAND_bytes(nonce_.data(), 32);
    }
    out_params->nonce = nonce_;
    LOG(INFO) << "Presented shared secret parameters with seed size " << out_params->seed.size()
              << " and nonce size " << out_params->nonce.size();
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus SoftSharedSecret::computeSharedSecret(
        const std::vector<SharedSecretParameters>& params, std::vector<uint8_t>* sharing_check) {
    std::lock_guard lock(mutex_);
    LOG(INFO) << "Computing shared secret";
    // Reimplemented based on SoftKeymasterEnforcement, which does not expose
    // enough functionality to satisfy the GateKeeper interface
    keymaster::KeymasterKeyBlob key_agreement_key;
    if (key_agreement_key.Reset(32) == nullptr) {
        LOG(ERROR) << "key agreement key memory allocation failed";
        return kmError2ScopedAStatus(KM_ERROR_MEMORY_ALLOCATION_FAILED);
    }
    // Matching:
    // - kFakeAgreementKey in system/keymaster/km_openssl/soft_keymaster_enforcement.cpp
    // - Keys::kak in hardware/interfaces/security/keymint/aidl/default/ta/soft.rs
    std::memset(key_agreement_key.writable_data(), 0, 32);
    keymaster::KeymasterBlob label((uint8_t*)KEY_AGREEMENT_LABEL, strlen(KEY_AGREEMENT_LABEL));
    if (label.data == nullptr) {
        LOG(ERROR) << "label memory allocation failed";
        return kmError2ScopedAStatus(KM_ERROR_MEMORY_ALLOCATION_FAILED);
    }

    static_assert(sizeof(keymaster_blob_t) == sizeof(keymaster::KeymasterBlob));

    bool found_mine = false;
    std::vector<keymaster::KeymasterBlob> context_blobs;
    for (const auto& param : params) {
        auto& seed_blob = context_blobs.emplace_back();
        if (seed_blob.Reset(param.seed.size()) == nullptr) {
            LOG(ERROR) << "seed memory allocation failed";
            return kmError2ScopedAStatus(KM_ERROR_MEMORY_ALLOCATION_FAILED);
        }
        std::copy(param.seed.begin(), param.seed.end(), seed_blob.writable_data());
        auto& nonce_blob = context_blobs.emplace_back();
        if (nonce_blob.Reset(param.nonce.size()) == nullptr) {
            LOG(ERROR) << "Nonce memory allocation failed";
            return kmError2ScopedAStatus(KM_ERROR_MEMORY_ALLOCATION_FAILED);
        }
        std::copy(param.nonce.begin(), param.nonce.end(), nonce_blob.writable_data());
        if (param.seed == seed_ && param.nonce == nonce_) {
            found_mine = true;
        }
    }
    if (!found_mine) {
        LOG(ERROR) << "Did not receive my own shared secret parameter back";
        return kmError2ScopedAStatus(KM_ERROR_INVALID_ARGUMENT);
    }
    auto context_blobs_ptr = reinterpret_cast<keymaster_blob_t*>(context_blobs.data());
    if (hmac_key_.Reset(32) == nullptr) {
        LOG(ERROR) << "hmac key allocation failed";
        return kmError2ScopedAStatus(KM_ERROR_MEMORY_ALLOCATION_FAILED);
    }
    auto error = keymaster::ckdf(key_agreement_key, label, context_blobs_ptr, context_blobs.size(),
                                 &hmac_key_);
    if (error != KM_ERROR_OK) {
        LOG(ERROR) << "CKDF failed";
        return kmError2ScopedAStatus(error);
    }

    keymaster::HmacSha256 hmac_impl;
    if (!hmac_impl.Init(hmac_key_.key_material, hmac_key_.key_material_size)) {
        LOG(ERROR) << "hmac initialization failed";
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    sharing_check->clear();
    sharing_check->resize(32, 0);
    if (!hmac_impl.Sign((const uint8_t*)KEY_CHECK_LABEL, strlen(KEY_CHECK_LABEL),
                        sharing_check->data(), sharing_check->size())) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ::ndk::ScopedAStatus::ok();
}

keymaster::KeymasterKeyBlob SoftSharedSecret::HmacKey() const {
    std::lock_guard lock(mutex_);
    return hmac_key_;
}

}  // namespace aidl::android::hardware::security::sharedsecret
