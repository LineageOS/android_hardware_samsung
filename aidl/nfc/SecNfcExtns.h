/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/nfc/NfcConfig.h>
#include <aidl/android/hardware/nfc/PresenceCheckAlgorithm.h>
#include <aidl/android/hardware/nfc/ProtocolDiscoveryConfig.h>
#include <android-base/logging.h>
#include <log/log.h>

namespace aidl {
namespace android {
namespace hardware {
namespace nfc {

#define MAX_CONFIG_STRING_LEN 260
using ::aidl::android::hardware::nfc::NfcConfig;
using NfcConfig = aidl::android::hardware::nfc::NfcConfig;
using PresenceCheckAlgorithm =
    aidl::android::hardware::nfc::PresenceCheckAlgorithm;
using ProtocolDiscoveryConfig =
    aidl::android::hardware::nfc::ProtocolDiscoveryConfig;

void nfc_hal_getVendorConfig_aidl(NfcConfig& config);

}  // namespace nfc
}  // namespace hardware
}  // namespace android
}  // namespace aidl
