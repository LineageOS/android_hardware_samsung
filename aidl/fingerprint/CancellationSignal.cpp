/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CancellationSignal.h"

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

CancellationSignal::CancellationSignal(Session* session)
    : mSession(session) {
}

ndk::ScopedAStatus CancellationSignal::cancel() {
    return mSession->cancel();
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
