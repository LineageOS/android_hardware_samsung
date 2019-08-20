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
#define LOG_TAG "android.hardware.biometrics.fingerprint@2.1-service.samsung"

#include <android-base/logging.h>

#include <hardware/hw_auth_token.h>

#include <hardware/fingerprint.h>
#include <hardware/hardware.h>
#include "BiometricsFingerprint.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <unistd.h>

namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace V2_1 {
namespace implementation {

using RequestStatus = android::hardware::biometrics::fingerprint::V2_1::RequestStatus;

BiometricsFingerprint* BiometricsFingerprint::sInstance = nullptr;

BiometricsFingerprint::BiometricsFingerprint() : mClientCallback(nullptr) {
    sInstance = this;  // keep track of the most recent instance
    if (!openHal()) {
        LOG(ERROR) << "Can't open HAL module";
    }
}

BiometricsFingerprint::~BiometricsFingerprint() {
    if (ss_fingerprint_close() != 0) {
        LOG(ERROR) << "Can't close HAL module";
    }
}

Return<RequestStatus> BiometricsFingerprint::ErrorFilter(int32_t error) {
    switch (error) {
        case 0:
            return RequestStatus::SYS_OK;
        case -2:
            return RequestStatus::SYS_ENOENT;
        case -4:
            return RequestStatus::SYS_EINTR;
        case -5:
            return RequestStatus::SYS_EIO;
        case -11:
            return RequestStatus::SYS_EAGAIN;
        case -12:
            return RequestStatus::SYS_ENOMEM;
        case -13:
            return RequestStatus::SYS_EACCES;
        case -14:
            return RequestStatus::SYS_EFAULT;
        case -16:
            return RequestStatus::SYS_EBUSY;
        case -22:
            return RequestStatus::SYS_EINVAL;
        case -28:
            return RequestStatus::SYS_ENOSPC;
        case -110:
            return RequestStatus::SYS_ETIMEDOUT;
        default:
            LOG(ERROR) << "An unknown error returned from fingerprint vendor library: " << error;
            return RequestStatus::SYS_UNKNOWN;
    }
}

// Translate from errors returned by traditional HAL (see fingerprint.h) to
// HIDL-compliant FingerprintError.
FingerprintError BiometricsFingerprint::VendorErrorFilter(int32_t error, int32_t* vendorCode) {
    *vendorCode = 0;
    switch (error) {
        case FINGERPRINT_ERROR_HW_UNAVAILABLE:
            return FingerprintError::ERROR_HW_UNAVAILABLE;
        case FINGERPRINT_ERROR_UNABLE_TO_PROCESS:
            return FingerprintError::ERROR_UNABLE_TO_PROCESS;
        case FINGERPRINT_ERROR_TIMEOUT:
            return FingerprintError::ERROR_TIMEOUT;
        case FINGERPRINT_ERROR_NO_SPACE:
            return FingerprintError::ERROR_NO_SPACE;
        case FINGERPRINT_ERROR_CANCELED:
            return FingerprintError::ERROR_CANCELED;
        case FINGERPRINT_ERROR_UNABLE_TO_REMOVE:
            return FingerprintError::ERROR_UNABLE_TO_REMOVE;
        case FINGERPRINT_ERROR_LOCKOUT:
            return FingerprintError::ERROR_LOCKOUT;
        default:
            if (error >= FINGERPRINT_ERROR_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = error - FINGERPRINT_ERROR_VENDOR_BASE;
                return FingerprintError::ERROR_VENDOR;
            }
    }
    LOG(ERROR) << "Unknown error from fingerprint vendor library: " << error;
    return FingerprintError::ERROR_UNABLE_TO_PROCESS;
}

// Translate acquired messages returned by traditional HAL (see fingerprint.h)
// to HIDL-compliant FingerprintAcquiredInfo.
FingerprintAcquiredInfo BiometricsFingerprint::VendorAcquiredFilter(int32_t info,
                                                                    int32_t* vendorCode) {
    *vendorCode = 0;
    switch (info) {
        case FINGERPRINT_ACQUIRED_GOOD:
            return FingerprintAcquiredInfo::ACQUIRED_GOOD;
        case FINGERPRINT_ACQUIRED_PARTIAL:
            return FingerprintAcquiredInfo::ACQUIRED_PARTIAL;
        case FINGERPRINT_ACQUIRED_INSUFFICIENT:
            return FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT;
        case FINGERPRINT_ACQUIRED_IMAGER_DIRTY:
            return FingerprintAcquiredInfo::ACQUIRED_IMAGER_DIRTY;
        case FINGERPRINT_ACQUIRED_TOO_SLOW:
            return FingerprintAcquiredInfo::ACQUIRED_TOO_SLOW;
        case FINGERPRINT_ACQUIRED_TOO_FAST:
            return FingerprintAcquiredInfo::ACQUIRED_TOO_FAST;
        default:
            if (info >= FINGERPRINT_ACQUIRED_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = info - FINGERPRINT_ACQUIRED_VENDOR_BASE;
                return FingerprintAcquiredInfo::ACQUIRED_VENDOR;
            }
    }
    LOG(ERROR) << "Unknown acquiredmsg from fingerprint vendor library: " << info;
    return FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT;
}

Return<uint64_t> BiometricsFingerprint::setNotify(
    const sp<IBiometricsFingerprintClientCallback>& clientCallback) {
    std::lock_guard<std::mutex> lock(mClientCallbackMutex);
    mClientCallback = clientCallback;
    // This is here because HAL 2.1 doesn't have a way to propagate a
    // unique token for its driver. Subsequent versions should send a unique
    // token for each call to setNotify(). This is fine as long as there's only
    // one fingerprint device on the platform.
    return reinterpret_cast<uint64_t>(this);
}

Return<uint64_t> BiometricsFingerprint::preEnroll() {
    return ss_fingerprint_pre_enroll();
}

Return<RequestStatus> BiometricsFingerprint::enroll(const hidl_array<uint8_t, 69>& hat,
                                                    uint32_t gid, uint32_t timeoutSec) {
    const hw_auth_token_t* authToken = reinterpret_cast<const hw_auth_token_t*>(hat.data());

    return ErrorFilter(ss_fingerprint_enroll(authToken, gid, timeoutSec));
}

Return<RequestStatus> BiometricsFingerprint::postEnroll() {
    return ErrorFilter(ss_fingerprint_post_enroll());
}

Return<uint64_t> BiometricsFingerprint::getAuthenticatorId() {
    return ss_fingerprint_get_auth_id();
}

Return<RequestStatus> BiometricsFingerprint::cancel() {
    return ErrorFilter(ss_fingerprint_cancel());
}

Return<RequestStatus> BiometricsFingerprint::enumerate() {
    if (ss_fingerprint_enumerate != nullptr) {
        return ErrorFilter(ss_fingerprint_enumerate());
    }

    return RequestStatus::SYS_UNKNOWN;
}

Return<RequestStatus> BiometricsFingerprint::remove(uint32_t gid, uint32_t fid) {
    return ErrorFilter(ss_fingerprint_remove(gid, fid));
}

Return<RequestStatus> BiometricsFingerprint::setActiveGroup(uint32_t gid,
                                                            const hidl_string& storePath) {
    if (storePath.size() >= PATH_MAX || storePath.size() <= 0) {
        LOG(ERROR) << "Bad path length: " << storePath.size();
        return RequestStatus::SYS_EINVAL;
    }

    if (access(storePath.c_str(), W_OK)) {
        return RequestStatus::SYS_EINVAL;
    }

    return ErrorFilter(ss_fingerprint_set_active_group(gid, storePath.c_str()));
}

Return<RequestStatus> BiometricsFingerprint::authenticate(uint64_t operationId, uint32_t gid) {
    return ErrorFilter(ss_fingerprint_authenticate(operationId, gid));
}

IBiometricsFingerprint* BiometricsFingerprint::getInstance() {
    if (!sInstance) {
        sInstance = new BiometricsFingerprint();
    }
    return sInstance;
}

bool BiometricsFingerprint::openHal() {
    void* handle = dlopen("libbauthserver.so", RTLD_NOW);
    if (handle) {
        int err;

        ss_fingerprint_close =
            reinterpret_cast<typeof(ss_fingerprint_close)>(dlsym(handle, "ss_fingerprint_close"));
        ss_fingerprint_open =
            reinterpret_cast<typeof(ss_fingerprint_open)>(dlsym(handle, "ss_fingerprint_open"));

        ss_set_notify_callback = reinterpret_cast<typeof(ss_set_notify_callback)>(
            dlsym(handle, "ss_set_notify_callback"));
        ss_fingerprint_pre_enroll = reinterpret_cast<typeof(ss_fingerprint_pre_enroll)>(
            dlsym(handle, "ss_fingerprint_pre_enroll"));
        ss_fingerprint_enroll =
            reinterpret_cast<typeof(ss_fingerprint_enroll)>(dlsym(handle, "ss_fingerprint_enroll"));
        ss_fingerprint_post_enroll = reinterpret_cast<typeof(ss_fingerprint_post_enroll)>(
            dlsym(handle, "ss_fingerprint_post_enroll"));
        ss_fingerprint_get_auth_id = reinterpret_cast<typeof(ss_fingerprint_get_auth_id)>(
            dlsym(handle, "ss_fingerprint_get_auth_id"));
        ss_fingerprint_cancel =
            reinterpret_cast<typeof(ss_fingerprint_cancel)>(dlsym(handle, "ss_fingerprint_cancel"));
        ss_fingerprint_enumerate = reinterpret_cast<typeof(ss_fingerprint_enumerate)>(
            dlsym(handle, "ss_fingerprint_enumerate"));
        ss_fingerprint_remove =
            reinterpret_cast<typeof(ss_fingerprint_remove)>(dlsym(handle, "ss_fingerprint_remove"));
        ss_fingerprint_set_active_group = reinterpret_cast<typeof(ss_fingerprint_set_active_group)>(
            dlsym(handle, "ss_fingerprint_set_active_group"));
        ss_fingerprint_authenticate = reinterpret_cast<typeof(ss_fingerprint_authenticate)>(
            dlsym(handle, "ss_fingerprint_authenticate"));

        if ((err = ss_fingerprint_open(nullptr)) != 0) {
            LOG(ERROR) << "Can't open fingerprint, error: " << err;
            return false;
        }

        if ((err = ss_set_notify_callback(BiometricsFingerprint::notify)) != 0) {
            LOG(ERROR) << "Can't register fingerprint module callback, error: " << err;
            return false;
        }

        return true;
    }

    return false;
}

void BiometricsFingerprint::notify(const fingerprint_msg_t* msg) {
    BiometricsFingerprint* thisPtr =
        static_cast<BiometricsFingerprint*>(BiometricsFingerprint::getInstance());
    std::lock_guard<std::mutex> lock(thisPtr->mClientCallbackMutex);
    if (thisPtr == nullptr || thisPtr->mClientCallback == nullptr) {
        LOG(ERROR) << "Receiving callbacks before the client callback is registered.";
        return;
    }
    const uint64_t devId = 1;
    switch (msg->type) {
        case FINGERPRINT_ERROR: {
            int32_t vendorCode = 0;
            FingerprintError result = VendorErrorFilter(msg->data.error, &vendorCode);
            LOG(DEBUG) << "onError(" << static_cast<int>(result) << ")";
            if (!thisPtr->mClientCallback->onError(devId, result, vendorCode).isOk()) {
                LOG(ERROR) << "failed to invoke fingerprint onError callback";
            }
        } break;
        case FINGERPRINT_ACQUIRED: {
            int32_t vendorCode = 0;
            FingerprintAcquiredInfo result =
                VendorAcquiredFilter(msg->data.acquired.acquired_info, &vendorCode);
            LOG(DEBUG) << "onAcquired(" << static_cast<int>(result) << ")";
            if (!thisPtr->mClientCallback->onAcquired(devId, result, vendorCode).isOk()) {
                LOG(ERROR) << "failed to invoke fingerprint onAcquired callback";
            }
        } break;
        case FINGERPRINT_TEMPLATE_ENROLLING:
            LOG(DEBUG) << "onEnrollResult(fid=" << msg->data.enroll.finger.fid
                       << ", gid=" << msg->data.enroll.finger.gid
                       << ", rem=" << msg->data.enroll.samples_remaining << ")";
            if (thisPtr->mClientCallback
                    ->onEnrollResult(devId, msg->data.enroll.finger.fid,
                                     msg->data.enroll.finger.gid, msg->data.enroll.samples_remaining)
                    .isOk()) {
#ifdef USES_PERCENTAGE_SAMPLES
                fingerprint_msg_t* newMsg = (fingerprint_msg_t*)msg;
                newMsg->data.enroll.samples_remaining = 100 - msg->data.enroll.samples_remaining;
                msg = newMsg;
#endif
            } else {
                LOG(ERROR) << "failed to invoke fingerprint onEnrollResult callback";
            }
            break;
        case FINGERPRINT_TEMPLATE_REMOVED:
            LOG(DEBUG) << "onRemove(fid=" << msg->data.removed.finger.fid
                       << ", gid=" << msg->data.removed.finger.gid
                       << ", rem=" << msg->data.removed.remaining_templates << ")";
            if (!thisPtr->mClientCallback
                     ->onRemoved(devId, msg->data.removed.finger.fid, msg->data.removed.finger.gid,
                                 msg->data.removed.remaining_templates)
                     .isOk()) {
                LOG(ERROR) << "failed to invoke fingerprint onRemoved callback";
            }
            break;
        case FINGERPRINT_AUTHENTICATED:
            LOG(DEBUG) << "onAuthenticated(fid=" << msg->data.authenticated.finger.fid
                       << ", gid=" << msg->data.authenticated.finger.gid << ")";
            if (msg->data.authenticated.finger.fid != 0) {
                const uint8_t* hat = reinterpret_cast<const uint8_t*>(&msg->data.authenticated.hat);
                const hidl_vec<uint8_t> token(
                    std::vector<uint8_t>(hat, hat + sizeof(msg->data.authenticated.hat)));
                if (!thisPtr->mClientCallback
                         ->onAuthenticated(devId, msg->data.authenticated.finger.fid,
                                           msg->data.authenticated.finger.gid, token)
                         .isOk()) {
                    LOG(ERROR) << "failed to invoke fingerprint onAuthenticated callback";
                }
            } else {
                // Not a recognized fingerprint
                if (!thisPtr->mClientCallback
                         ->onAuthenticated(devId, msg->data.authenticated.finger.fid,
                                           msg->data.authenticated.finger.gid, hidl_vec<uint8_t>())
                         .isOk()) {
                    LOG(ERROR) << "failed to invoke fingerprint onAuthenticated callback";
                }
            }
            break;
        case FINGERPRINT_TEMPLATE_ENUMERATING:
            LOG(DEBUG) << "onEnumerate(fid=" << msg->data.enumerated.finger.fid
                       << ", gid=" << msg->data.enumerated.finger.gid
                       << ", rem=" << msg->data.enumerated.remaining_templates << ")";
            if (!thisPtr->mClientCallback
                     ->onEnumerate(devId, msg->data.enumerated.finger.fid,
                                   msg->data.enumerated.finger.gid,
                                   msg->data.enumerated.remaining_templates)
                     .isOk()) {
                LOG(ERROR) << "failed to invoke fingerprint onEnumerate callback";
            }
            break;
    }
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace hardware
}  // namespace android
