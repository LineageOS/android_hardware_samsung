/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CancellationSignal.h"
#include "Legacy2Aidl.h"
#include "Session.h"
#include "VendorConstants.h"

#include <fingerprint.sysprop.h>

#include <android-base/logging.h>

#include <endian.h>
#include <thread>

using namespace ::android::fingerprint::samsung;

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

void onClientDeath(void* cookie) {
    LOG(INFO) << "FingerprintService has died";
    Session* session = static_cast<Session*>(cookie);
    if (session && !session->isClosed()) {
        session->close();
    }
}

Session::Session(LegacyHAL hal, int userId, std::shared_ptr<ISessionCallback> cb,
                 LockoutTracker lockoutTracker)
    : mHal(hal),
      mLockoutTracker(lockoutTracker),
      mUserId(userId),
      mCb(cb) {
    mDeathRecipient = AIBinder_DeathRecipient_new(onClientDeath);

    char filename[64];
    snprintf(filename, sizeof(filename), "/data/vendor_de/%d/fpdata/");
    mHal.ss_fingerprint_set_active_group(userId, &filename[0]);
}

ndk::ScopedAStatus Session::generateChallenge() {
    LOG(INFO) << "generateChallenge";

    uint64_t challenge = mHal.ss_fingerprint_pre_enroll();
    mCb->onChallengeGenerated(challenge);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::revokeChallenge(int64_t challenge) {
    LOG(INFO) << "revokeChallenge";

    mHal.ss_fingerprint_post_enroll();
    mCb->onChallengeRevoked(challenge);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enroll(const HardwareAuthToken& hat,
                                   std::shared_ptr<ICancellationSignal>* out) {
    LOG(INFO) << "enroll";

    if (FingerprintHalProperties::force_calibrate().value_or(false)) {
        mHal.request(SEM_REQUEST_FORCE_CBGE, 1);
    }

    hw_auth_token_t authToken;
    translate(hat, authToken);

    int32_t error = mHal.ss_fingerprint_enroll(&authToken, mUserId, 0 /* timeoutSec */);
    if (error) {
        LOG(ERROR) << "ss_fingerprint_enroll failed: " << error;
        mCb->onError(Error::UNABLE_TO_PROCESS, error);
    }

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticate(int64_t operationId,
                                         std::shared_ptr<ICancellationSignal>* out) {
    LOG(INFO) << "authenticate";

    int32_t error = mHal.ss_fingerprint_authenticate(operationId, mUserId);
    if (error) {
        LOG(ERROR) << "ss_fingerprint_authenticate failed: " << error;
        mCb->onError(Error::UNABLE_TO_PROCESS, error);
    }

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::detectInteraction(std::shared_ptr<ICancellationSignal>* out) {
    LOG(INFO) << "detectInteraction";

    LOG(DEBUG) << "Detect interaction is not supported";
    mCb->onError(Error::UNABLE_TO_PROCESS, 0 /* vendorCode */);

    *out = SharedRefBase::make<CancellationSignal>(this);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enumerateEnrollments() {
    LOG(INFO) << "enumerateEnrollments";

    int32_t error = mHal.ss_fingerprint_enumerate();
    if (error)
        LOG(ERROR) << "ss_fingerprint_enumerate failed: " << error;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    LOG(INFO) << "removeEnrollments, size: " << enrollmentIds.size();

    for (int32_t enrollment : enrollmentIds) {
        int32_t error = mHal.ss_fingerprint_remove(mUserId, enrollment);
        if (error)
            LOG(ERROR) << "ss_fingerprint_remove failed: " << error;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getAuthenticatorId() {
    LOG(INFO) << "getAuthenticatorId";

    mCb->onAuthenticatorIdRetrieved(mHal.ss_fingerprint_get_auth_id());

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::invalidateAuthenticatorId() {
    LOG(INFO) << "invalidateAuthenticatorId";

    mCb->onAuthenticatorIdInvalidated(mHal.ss_fingerprint_get_auth_id());

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::resetLockout(const HardwareAuthToken& /*hat*/) {
    LOG(INFO) << "resetLockout";

    clearLockout(true);
    mIsLockoutTimerAborted = true;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::close() {
    LOG(INFO) << "close";
    mClosed = true;
    mCb->onSessionClosed();
    AIBinder_DeathRecipient_delete(mDeathRecipient);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerDown(int32_t /*pointerId*/, int32_t /*x*/, int32_t /*y*/, float /*minor*/,
                                          float /*major*/) {
    LOG(INFO) << "onPointerDown";

    if (FingerprintHalProperties::request_touch_event().value_or(false)) {
        mHal.request(SEM_REQUEST_TOUCH_EVENT, 2);
    }
    checkSensorLockout();

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerUp(int32_t /*pointerId*/) {
    LOG(INFO) << "onPointerUp";

    if (FingerprintHalProperties::request_touch_event().value_or(false)) {
        mHal.request(SEM_REQUEST_TOUCH_EVENT, 1);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onUiReady() {
    LOG(INFO) << "onUiReady";

    // TODO: stub

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticateWithContext(
        int64_t operationId, const OperationContext& /*context*/,
        std::shared_ptr<ICancellationSignal>* out) {
    return authenticate(operationId, out);
}

ndk::ScopedAStatus Session::enrollWithContext(const HardwareAuthToken& hat,
                                              const OperationContext& /*context*/,
                                              std::shared_ptr<ICancellationSignal>* out) {
    return enroll(hat, out);
}

ndk::ScopedAStatus Session::detectInteractionWithContext(const OperationContext& /*context*/,
                                              std::shared_ptr<ICancellationSignal>* out) {
    return detectInteraction(out);
}

ndk::ScopedAStatus Session::onPointerDownWithContext(const PointerContext& context) {
    return onPointerDown(context.pointerId, context.x, context.y, context.minor, context.major);
}

ndk::ScopedAStatus Session::onPointerUpWithContext(const PointerContext& context) {
    return onPointerUp(context.pointerId);
}

ndk::ScopedAStatus Session::onContextChanged(const OperationContext& /*context*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerCancelWithContext(const PointerContext& /*context*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::setIgnoreDisplayTouches(bool /*shouldIgnore*/) {
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::cancel() {
    int32_t ret = mHal.ss_fingerprint_cancel();

    if (ret == 0) {
        mCb->onError(Error::CANCELED, 0 /* vendorCode */);

        return ndk::ScopedAStatus::ok();
    } else {
        return ndk::ScopedAStatus::fromServiceSpecificError(ret);
    }
}

binder_status_t Session::linkToDeath(AIBinder* binder) {
    return AIBinder_linkToDeath(binder, mDeathRecipient, this);
}

bool Session::isClosed() {
    return mClosed;
}

// Translate from errors returned by traditional HAL (see fingerprint.h) to
// AIDL-compliant Error
Error Session::VendorErrorFilter(int32_t error, int32_t* vendorCode) {
    *vendorCode = 0;

    switch (error) {
        case FINGERPRINT_ERROR_HW_UNAVAILABLE:
            return Error::HW_UNAVAILABLE;
        case FINGERPRINT_ERROR_UNABLE_TO_PROCESS:
            return Error::UNABLE_TO_PROCESS;
        case FINGERPRINT_ERROR_TIMEOUT:
            return Error::TIMEOUT;
        case FINGERPRINT_ERROR_NO_SPACE:
            return Error::NO_SPACE;
        case FINGERPRINT_ERROR_CANCELED:
            return Error::CANCELED;
        case FINGERPRINT_ERROR_UNABLE_TO_REMOVE:
            return Error::UNABLE_TO_REMOVE;
        case FINGERPRINT_ERROR_LOCKOUT: {
            *vendorCode = FINGERPRINT_ERROR_LOCKOUT;
            return Error::VENDOR;
        }
        default:
            if (error >= FINGERPRINT_ERROR_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = error - FINGERPRINT_ERROR_VENDOR_BASE;
                return Error::VENDOR;
            }
    }
    LOG(ERROR) << "Unknown error from fingerprint vendor library: " << error;
    return Error::UNABLE_TO_PROCESS;
}

// Translate acquired messages returned by traditional HAL (see fingerprint.h)
// to AIDL-compliant AcquiredInfo
AcquiredInfo Session::VendorAcquiredFilter(int32_t info, int32_t* vendorCode) {
    *vendorCode = 0;

    switch (info) {
        case FINGERPRINT_ACQUIRED_GOOD:
            return AcquiredInfo::GOOD;
        case FINGERPRINT_ACQUIRED_PARTIAL:
            return AcquiredInfo::PARTIAL;
        case FINGERPRINT_ACQUIRED_INSUFFICIENT:
            return AcquiredInfo::INSUFFICIENT;
        case FINGERPRINT_ACQUIRED_IMAGER_DIRTY:
            return AcquiredInfo::SENSOR_DIRTY;
        case FINGERPRINT_ACQUIRED_TOO_SLOW:
            return AcquiredInfo::TOO_SLOW;
        case FINGERPRINT_ACQUIRED_TOO_FAST:
            return AcquiredInfo::TOO_FAST;
        default:
            if (info >= FINGERPRINT_ACQUIRED_VENDOR_BASE) {
                // vendor specific code.
                *vendorCode = info - FINGERPRINT_ACQUIRED_VENDOR_BASE;
                return AcquiredInfo::VENDOR;
            }
    }
    LOG(ERROR) << "Unknown acquiredmsg from fingerprint vendor library: " << info;
    return AcquiredInfo::INSUFFICIENT;
}

bool Session::checkSensorLockout() {
    LockoutMode lockoutMode = mLockoutTracker.getMode();
    if (lockoutMode == LockoutMode::PERMANENT) {
        LOG(ERROR) << "Fail: lockout permanent";
        mCb->onLockoutPermanent();
        mIsLockoutTimerAborted = true;
        return true;
    } else if (lockoutMode == LockoutMode::TIMED) {
        int64_t timeLeft = mLockoutTracker.getLockoutTimeLeft();
        LOG(ERROR) << "Fail: lockout timed " << timeLeft;
        mCb->onLockoutTimed(timeLeft);
        if (!mIsLockoutTimerStarted) startLockoutTimer(timeLeft);
        return true;
    }
    return false;
}

void Session::clearLockout(bool clearAttemptCounter) {
    mLockoutTracker.reset(clearAttemptCounter);
    mCb->onLockoutCleared();
}

void Session::startLockoutTimer(int64_t timeout) {
    mIsLockoutTimerAborted = false;
    std::function<void()> action =
            std::bind(&Session::lockoutTimerExpired, this);
    std::thread([timeout, action]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        action();
    }).detach();

    mIsLockoutTimerStarted = true;
}

void Session::lockoutTimerExpired() {
    if (!mIsLockoutTimerAborted)
        clearLockout(false);

    mIsLockoutTimerStarted = false;
    mIsLockoutTimerAborted = false;
}

void Session::notify(const fingerprint_msg_t* msg) {
    switch (msg->type) {
        case FINGERPRINT_ERROR: {
            int32_t vendorCode = 0;
            Error result = VendorErrorFilter(msg->data.error, &vendorCode);
            LOG(DEBUG) << "onError(" << static_cast<int>(result) << ")";
            mCb->onError(result, vendorCode);
        } break;
        case FINGERPRINT_ACQUIRED: {
            int32_t vendorCode = 0;
            AcquiredInfo result =
                VendorAcquiredFilter(msg->data.acquired.acquired_info, &vendorCode);
            LOG(DEBUG) << "onAcquired(" << static_cast<int>(result) << ")";
            mCb->onAcquired(result, vendorCode);
        } break;
        case FINGERPRINT_TEMPLATE_ENROLLING:
            if (FingerprintHalProperties::uses_percentage_samples().value_or(false)) {
                const_cast<fingerprint_msg_t*>(msg)->data.enroll.samples_remaining =
                    100 - msg->data.enroll.samples_remaining;
            }
            if (FingerprintHalProperties::cancel_on_enroll_completion().value_or(false)) {
                if (msg->data.enroll.samples_remaining == 0)
                    mHal.ss_fingerprint_cancel();
            }
            LOG(DEBUG) << "onEnrollResult(fid=" << msg->data.enroll.finger.fid
                       << ", gid=" << msg->data.enroll.finger.gid
                       << ", rem=" << msg->data.enroll.samples_remaining << ")";
            mCb->onEnrollmentProgress(msg->data.enroll.finger.fid,
                                      msg->data.enroll.samples_remaining);
            break;
        case FINGERPRINT_TEMPLATE_REMOVED: {
            LOG(DEBUG) << "onRemove(fid=" << msg->data.removed.finger.fid
                       << ", gid=" << msg->data.removed.finger.gid
                       << ", rem=" << msg->data.removed.remaining_templates << ")";
            std::vector<int> enrollments;
            enrollments.push_back(msg->data.removed.finger.fid);
            mCb->onEnrollmentsRemoved(enrollments);
        } break;
        case FINGERPRINT_AUTHENTICATED: {
            LOG(DEBUG) << "onAuthenticated(fid=" << msg->data.authenticated.finger.fid
                       << ", gid=" << msg->data.authenticated.finger.gid << ")";
            if (msg->data.authenticated.finger.fid != 0) {
                const hw_auth_token_t hat = msg->data.authenticated.hat;
                HardwareAuthToken authToken;
                translate(hat, authToken);

                mCb->onAuthenticationSucceeded(msg->data.authenticated.finger.fid, authToken);
                mLockoutTracker.reset(true);
            } else {
                mCb->onAuthenticationFailed();
                mLockoutTracker.addFailedAttempt();
                checkSensorLockout();
            }
        } break;
        case FINGERPRINT_TEMPLATE_ENUMERATING: {
            LOG(DEBUG) << "onEnumerate(fid=" << msg->data.enumerated.finger.fid
                       << ", gid=" << msg->data.enumerated.finger.gid
                       << ", rem=" << msg->data.enumerated.remaining_templates << ")";
            std::vector<int> enrollments;
            enrollments.push_back(msg->data.enumerated.finger.fid);
            mCb->onEnrollmentsEnumerated(enrollments);
        } break;
    }
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
