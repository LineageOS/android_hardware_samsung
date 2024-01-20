/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>

#include <hardware/fingerprint.h>
#include <thread/WorkerThread.h>

#include "LegacyHAL.h"
#include "LockoutTracker.h"

using ::aidl::android::hardware::biometrics::common::ICancellationSignal;
using ::aidl::android::hardware::biometrics::common::OperationContext;
using ::aidl::android::hardware::biometrics::fingerprint::PointerContext;
using ::aidl::android::hardware::keymaster::HardwareAuthToken;

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

enum class SessionState {
    IDLING,
    CLOSED,
    GENERATING_CHALLENGE,
    REVOKING_CHALLENGE,
    ENROLLING,
    AUTHENTICATING,
    DETECTING_INTERACTION,
    ENUMERATING_ENROLLMENTS,
    REMOVING_ENROLLMENTS,
    GETTING_AUTHENTICATOR_ID,
    INVALIDATING_AUTHENTICATOR_ID,
    RESETTING_LOCKOUT,
};

void onClientDeath(void* cookie);

class Session : public BnSession {
public:
    Session(LegacyHAL hal, int userId, std::shared_ptr<ISessionCallback> cb,
            WorkerThread* worker, LockoutTracker lockoutTracker);
    ndk::ScopedAStatus generateChallenge() override;
    ndk::ScopedAStatus revokeChallenge(int64_t challenge) override;
    ndk::ScopedAStatus enroll(const HardwareAuthToken& hat,
                              std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus authenticate(int64_t operationId,
                                    std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus detectInteraction(
            std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus enumerateEnrollments() override;
    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& enrollmentIds) override;
    ndk::ScopedAStatus getAuthenticatorId() override;
    ndk::ScopedAStatus invalidateAuthenticatorId() override;
    ndk::ScopedAStatus resetLockout(const HardwareAuthToken& hat) override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus onPointerDown(int32_t pointerId, int32_t x, int32_t y, float minor,
                                     float major) override;
    ndk::ScopedAStatus onPointerUp(int32_t pointerId) override;
    ndk::ScopedAStatus onUiReady() override;
    ndk::ScopedAStatus authenticateWithContext(
            int64_t operationId, const OperationContext& context,
            std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus enrollWithContext(
            const HardwareAuthToken& hat, const OperationContext& context,
            std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus detectInteractionWithContext(
            const OperationContext& context,
            std::shared_ptr<ICancellationSignal>* out) override;
    ndk::ScopedAStatus onPointerDownWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus onPointerUpWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus onContextChanged(const OperationContext& context) override;
    ndk::ScopedAStatus onPointerCancelWithContext(const PointerContext& context) override;
    ndk::ScopedAStatus setIgnoreDisplayTouches(bool shouldIgnore) override;

    binder_status_t linkToDeath(AIBinder* binder);
    bool isClosed();

    void notify(
        const fingerprint_msg_t* msg);

private:
    LegacyHAL mHal;
    LockoutTracker mLockoutTracker;

    Error VendorErrorFilter(int32_t error, int32_t* vendorCode);
    AcquiredInfo VendorAcquiredFilter(int32_t info, int32_t* vendorCode);
    void cancel();
    bool checkSensorLockout();
    void clearLockout();
    void startLockoutTimer(int64_t timeout);
    void lockoutTimerExpired();

    // lockout timer
    bool mIsLockoutTimerStarted = false;
    bool mIsLockoutTimerAborted = false;

    // Crashes the HAL if it's not currently idling because that would be an invalid state machine
    // transition. Otherwise, sets the scheduled state to the given state.
    void scheduleStateOrCrash(SessionState state);

    // Crashes the HAL if the provided state doesn't match the previously scheduled state.
    // Otherwise, transitions into the provided state, clears the scheduled state, and notifies
    // the client about the transition by calling ISessionCallback#onStateChanged.
    void enterStateOrCrash(SessionState state);

    // Sets the current state to SessionState::IDLING and notifies the client about the transition
    // by calling ISessionCallback#onStateChanged.
    void enterIdling();

    // The user ID for which this session was created.
    int32_t mUserId;

    // Callback for talking to the framework. This callback must only be called from non-binder
    // threads to prevent nested binder calls and consequently a binder thread exhaustion.
    // Practically, it means that this callback should always be called from the worker thread.
    std::shared_ptr<ISessionCallback> mCb;

    // Worker thread that allows to schedule tasks for asynchronous execution.
    WorkerThread* mWorker;

    // Simple representation of the session's state machine. These are atomic because they can be
    // modified from both the main and the worker threads.
    std::atomic<SessionState> mScheduledState;
    std::atomic<SessionState> mCurrentState;

    // Binder death handler.
    AIBinder_DeathRecipient* mDeathRecipient;
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
