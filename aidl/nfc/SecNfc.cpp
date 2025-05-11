/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SecNfcExtns.h"
#include "SecNfc.h"
#include "HalSecNfc.h"

#include <android-base/logging.h>

typedef uint16_t NFCSTATUS; /* Return values */
#define NFCSTATUS_SUCCESS (0x0000)

#define CHK_STATUS(x)                                 \
  ((x) == NFCSTATUS_SUCCESS)                          \
      ? ndk::ScopedAStatus::ok()                      \
      : ndk::ScopedAStatus::fromServiceSpecificError( \
            static_cast<int32_t>(NfcStatus::FAILED));

extern bool nfc_debug_enabled;

namespace aidl {
namespace android {
namespace hardware {
namespace nfc {

std::shared_ptr<INfcClientCallback> Nfc::mCallback = nullptr;
AIBinder_DeathRecipient* clientDeathRecipient = nullptr;
std::mutex syncNfcOpenClose;

void OnDeath(void* cookie) {
  if (Nfc::mCallback != nullptr &&
      !AIBinder_isAlive(Nfc::mCallback->asBinder().get())) {
    std::lock_guard<std::mutex> lk(syncNfcOpenClose);
    LOG(INFO) << __func__ << " Nfc service has died";
    Nfc* nfc = static_cast<Nfc*>(cookie);
    nfc->close(NfcCloseType::DISABLE);
    LOG(INFO) << __func__ << " death NTF completed";
  }
}

::ndk::ScopedAStatus Nfc::open(
    const std::shared_ptr<INfcClientCallback>& clientCallback) {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::open Enter");
  if (clientCallback == nullptr) {
    ALOGD_IF(nfc_debug_enabled, "SecNfc::open null callback");
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }
  std::lock_guard<std::mutex> lk(syncNfcOpenClose);
  Nfc::mCallback = clientCallback;

  clientDeathRecipient = AIBinder_DeathRecipient_new(OnDeath);
  auto linkRet = AIBinder_linkToDeath(clientCallback->asBinder().get(),
                                      clientDeathRecipient, this /* cookie */);
  if (linkRet != STATUS_OK) {
    LOG(ERROR) << __func__ << ": linkToDeath failed: " << linkRet;
    // Just ignore the error.
  }

  NFCSTATUS status = nfc_hal_open(eventCallback, dataCallback);
  ALOGD_IF(nfc_debug_enabled, "SecNfc::open Exit");
  return CHK_STATUS(status);
}

::ndk::ScopedAStatus Nfc::close(NfcCloseType type) {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::close Enter");
  if (Nfc::mCallback == nullptr) {
    LOG(ERROR) << __func__ << "mCallback null";
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }

  NFCSTATUS status = nfc_hal_close();
  AIBinder_DeathRecipient_delete(clientDeathRecipient);
  clientDeathRecipient = nullptr;
  return CHK_STATUS(status);
}

::ndk::ScopedAStatus Nfc::coreInitialized() {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::coreInitialized");
  if (Nfc::mCallback == nullptr) {
    LOG(ERROR) << __func__ << "mCallback null";
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }

  NFCSTATUS status = nfc_hal_core_initialized_for_aidl();
  return CHK_STATUS(status);
}

::ndk::ScopedAStatus Nfc::factoryReset() {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::factoryReset");
  nfc_hal_factory_reset();
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Nfc::getConfig(NfcConfig* _aidl_return) {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::getConfig");
  NfcConfig config;
  nfc_hal_getVendorConfig_aidl(config);
  *_aidl_return = std::move(config);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Nfc::powerCycle() {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::powerCycle");
  if (Nfc::mCallback == nullptr) {
    LOG(ERROR) << __func__ << "mCallback null";
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }
  NFCSTATUS status = nfc_hal_power_cycle();
  return CHK_STATUS(status);
}

::ndk::ScopedAStatus Nfc::preDiscover() {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::preDiscover");
  if (Nfc::mCallback == nullptr) {
    LOG(ERROR) << __func__ << "mCallback null";
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }
  NFCSTATUS status = nfc_hal_pre_discover();
  return CHK_STATUS(status);
}

::ndk::ScopedAStatus Nfc::write(const std::vector<uint8_t>& data,
                                int32_t* _aidl_return) {
  ALOGD_IF(nfc_debug_enabled, "SecNfc::write");
  if (Nfc::mCallback == nullptr) {
    LOG(ERROR) << __func__ << "mCallback null";
    return ndk::ScopedAStatus::fromServiceSpecificError(
        static_cast<int32_t>(NfcStatus::FAILED));
  }
  *_aidl_return = nfc_hal_write(data.size(), &data[0]);
  return ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus Nfc::setEnableVerboseLogging(bool enable) {
  LOG(INFO) << "SecNfc::setLogging";
  nfc_hal_setLogging(enable);
  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Nfc::isVerboseLoggingEnabled(bool* _aidl_return) {
  *_aidl_return = nfc_hal_isLoggingEnabled();
  return ndk::ScopedAStatus::ok();
}

}  // namespace nfc
}  // namespace hardware
}  // namespace android
}  // namespace aidl
