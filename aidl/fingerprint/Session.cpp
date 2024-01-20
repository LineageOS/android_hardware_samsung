/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Session.h"
#include "VendorConstants.h"

#include <fingerprint.sysprop.h>

#include <android-base/logging.h>
#include <util/CancellationSignal.h>

#include <endian.h>
#include <future>

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
                 WorkerThread* worker)
    : mHal(hal),
      mUserId(userId),
      mCb(cb),
      mWorker(worker),
      mScheduledState(SessionState::IDLING),
      mCurrentState(SessionState::IDLING) {
    mDeathRecipient = AIBinder_DeathRecipient_new(onClientDeath);

    mHal.ss_fingerprint_set_active_group(userId, "");
}

ndk::ScopedAStatus Session::generateChallenge() {
    LOG(INFO) << "generateChallenge";
    scheduleStateOrCrash(SessionState::GENERATING_CHALLENGE);

    mWorker->schedule(Callable::from([this] {
        enterStateOrCrash(SessionState::GENERATING_CHALLENGE);
        std::uniform_int_distribution<int64_t> dist;
        uint64_t challenge = mHal.ss_fingerprint_pre_enroll();
        mCb->onChallengeGenerated(challenge);
        enterIdling();
    }));

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::revokeChallenge(int64_t challenge) {
    LOG(INFO) << "revokeChallenge";
    scheduleStateOrCrash(SessionState::REVOKING_CHALLENGE);

    mWorker->schedule(Callable::from([this, challenge] {
        enterStateOrCrash(SessionState::REVOKING_CHALLENGE);
        mHal.ss_fingerprint_post_enroll();
        mCb->onChallengeRevoked(challenge);
        enterIdling();
    }));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::enroll(const HardwareAuthToken& hat,
                                   std::shared_ptr<ICancellationSignal>* out) {
    LOG(INFO) << "enroll";
    scheduleStateOrCrash(SessionState::ENROLLING);

    std::promise<void> cancellationPromise;
    auto cancFuture = cancellationPromise.get_future();

    mWorker->schedule(Callable::from([this, hat, cancFuture = std::move(cancFuture)] {
        enterStateOrCrash(SessionState::ENROLLING);
        if (shouldCancel(cancFuture)) {
            cancel();
        } else {
            if (FingerprintHalProperties::force_calibrate().value_or(false)) {
                mHal.request(SEM_REQUEST_FORCE_CBGE, 1);
            }

            hw_auth_token_t authToken;
            memset(&authToken, 0, sizeof(hw_auth_token_t));
            authToken.challenge = hat.challenge;
            authToken.user_id = hat.userId;
            authToken.authenticator_id = hat.authenticatorId;
            // these are in host order: translate to network order
            authToken.authenticator_type = htobe32(static_cast<uint32_t>(hat.authenticatorType));
            authToken.timestamp = htobe64(hat.timestamp.milliSeconds);
            std::copy(hat.mac.begin(), hat.mac.end(), authToken.hmac);

            int32_t error = mHal.ss_fingerprint_enroll(&authToken, mUserId, 0 /* timeoutSec */);
            if (error) {
                LOG(ERROR) << "ss_fingerprint_enroll failed: " << error;
                mCb->onError(Error::UNABLE_TO_PROCESS, error);
            }
        }
        enterIdling();
    }));

    *out = SharedRefBase::make<CancellationSignal>(std::move(cancellationPromise));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::authenticate(int64_t operationId,
                                         std::shared_ptr<ICancellationSignal>* out) {
    LOG(INFO) << "authenticate";
    scheduleStateOrCrash(SessionState::AUTHENTICATING);

    std::promise<void> cancPromise;
    auto cancFuture = cancPromise.get_future();

    mWorker->schedule(Callable::from([this, operationId, cancFuture = std::move(cancFuture)] {
        enterStateOrCrash(SessionState::AUTHENTICATING);
        if (shouldCancel(cancFuture)) {
            cancel();
        } else {
            int32_t error = mHal.ss_fingerprint_authenticate(operationId, mUserId);
            if (error) {
                LOG(ERROR) << "ss_fingerprint_authenticate failed: " << error;
                mCb->onError(Error::UNABLE_TO_PROCESS, error);
            }
        }
        enterIdling();
    }));

    *out = SharedRefBase::make<CancellationSignal>(std::move(cancPromise));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::detectInteraction(std::shared_ptr<ICancellationSignal>* /*out*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Session::enumerateEnrollments() {
    LOG(INFO) << "enumerateEnrollments";
    scheduleStateOrCrash(SessionState::ENUMERATING_ENROLLMENTS);

    mWorker->schedule(Callable::from([this] {
        enterStateOrCrash(SessionState::ENUMERATING_ENROLLMENTS);
        int32_t error = mHal.ss_fingerprint_enumerate();
        if (error)
            LOG(ERROR) << "ss_fingerprint_enumerate failed: " << error;
        enterIdling();
    }));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::removeEnrollments(const std::vector<int32_t>& enrollmentIds) {
    LOG(INFO) << "removeEnrollments, size: " << enrollmentIds.size();
    scheduleStateOrCrash(SessionState::REMOVING_ENROLLMENTS);

    mWorker->schedule(Callable::from([this, enrollmentIds] {
        enterStateOrCrash(SessionState::REMOVING_ENROLLMENTS);
        for (int32_t enrollment : enrollmentIds) {
            int32_t error = mHal.ss_fingerprint_remove(mUserId, enrollment);
            if (error)
                LOG(ERROR) << "ss_fingerprint_remove failed: " << error;
        }
        enterIdling();
    }));

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::getAuthenticatorId() {
    LOG(INFO) << "getAuthenticatorId";
    scheduleStateOrCrash(SessionState::GETTING_AUTHENTICATOR_ID);

    mWorker->schedule(Callable::from([this] {
        enterStateOrCrash(SessionState::GETTING_AUTHENTICATOR_ID);
        mCb->onAuthenticatorIdRetrieved(mHal.ss_fingerprint_get_auth_id());
        enterIdling();
    }));

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::invalidateAuthenticatorId() {
    LOG(INFO) << "invalidateAuthenticatorId";
    scheduleStateOrCrash(SessionState::INVALIDATING_AUTHENTICATOR_ID);

    mWorker->schedule(Callable::from([this] {
        enterStateOrCrash(SessionState::INVALIDATING_AUTHENTICATOR_ID);
        mCb->onAuthenticatorIdInvalidated(mHal.ss_fingerprint_get_auth_id());
        enterIdling();
    }));

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::resetLockout(const HardwareAuthToken& /*hat*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Session::close() {
    LOG(INFO) << "close";
    // TODO(b/166800618): call enterIdling from the terminal callbacks and restore this check.
    // CHECK(mCurrentState == SessionState::IDLING) << "Can't close a non-idling session.
    // Crashing.";
    mCurrentState = SessionState::CLOSED;
    mCb->onSessionClosed();
    AIBinder_DeathRecipient_delete(mDeathRecipient);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerDown(int32_t /*pointerId*/, int32_t /*x*/, int32_t /*y*/, float /*minor*/,
                                          float /*major*/) {
    LOG(INFO) << "onPointerDown";
    mWorker->schedule(Callable::from([this] {
        if (FingerprintHalProperties::request_touch_event().value_or(false)) {
            mHal.request(SEM_REQUEST_TOUCH_EVENT, 2);
        }
        enterIdling();
    }));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onPointerUp(int32_t /*pointerId*/) {
    LOG(INFO) << "onPointerUp";
    mWorker->schedule(Callable::from([this] {
        if (FingerprintHalProperties::request_touch_event().value_or(false)) {
            mHal.request(SEM_REQUEST_TOUCH_EVENT, 1);
        }
        enterIdling();
    }));
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Session::onUiReady() {
    LOG(INFO) << "onUiReady";
    mWorker->schedule(Callable::from([this] {
        // TODO: stub
        enterIdling();
    }));
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

binder_status_t Session::linkToDeath(AIBinder* binder) {
    return AIBinder_linkToDeath(binder, mDeathRecipient, this);
}

void Session::scheduleStateOrCrash(SessionState state) {
    // TODO(b/166800618): call enterIdling from the terminal callbacks and restore these checks.
    // CHECK(mScheduledState == SessionState::IDLING);
    // CHECK(mCurrentState == SessionState::IDLING);
    mScheduledState = state;
}

void Session::enterStateOrCrash(SessionState state) {
    CHECK(mScheduledState == state);
    mCurrentState = state;
    mScheduledState = SessionState::IDLING;
}

void Session::enterIdling() {
    // TODO(b/166800618): call enterIdling from the terminal callbacks and rethink this conditional.
    if (mCurrentState != SessionState::CLOSED) {
        mCurrentState = SessionState::IDLING;
    }
}

bool Session::isClosed() {
    return mCurrentState == SessionState::CLOSED;
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

void Session::cancel() {
    int32_t ret = mHal.ss_fingerprint_cancel();

    if (ret == 0) {
        fingerprint_msg_t msg{};
        msg.type = FINGERPRINT_ERROR;
        msg.data.error = FINGERPRINT_ERROR_CANCELED;
        notify(&msg);
    }
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
                memset(&authToken, 0, sizeof(HardwareAuthToken));
                authToken.challenge = hat.challenge;
                authToken.userId = hat.user_id;
                authToken.authenticatorId = hat.authenticator_id;
                // these are in network order: translate to host
                authToken.authenticatorType =
                        static_cast<keymaster::HardwareAuthenticatorType>(
                                be32toh(hat.authenticator_type));
                authToken.timestamp.milliSeconds = be64toh(hat.timestamp);
                authToken.mac.insert(authToken.mac.begin(), std::begin(hat.hmac),
                                     std::end(hat.hmac));

                mCb->onAuthenticationSucceeded(msg->data.authenticated.finger.fid, authToken);
            } else {
                mCb->onAuthenticationFailed();
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
