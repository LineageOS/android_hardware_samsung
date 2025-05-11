/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "secnfc-service"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <thread>

#include "SecNfc.h"

using ::aidl::android::hardware::nfc::Nfc;
using namespace std;

int main() {
  ALOGI("SEC NFC AIDL HAL starting up");
  if (!ABinderProcess_setThreadPoolMaxThreadCount(1)) {
    ALOGE("failed to set thread pool max thread count");
    return 1;
  }
  std::shared_ptr<Nfc> nfc_service = ndk::SharedRefBase::make<Nfc>();

  const std::string nfcInstName = std::string() + Nfc::descriptor + "/default";
  binder_status_t status = AServiceManager_addService(
      nfc_service->asBinder().get(), nfcInstName.c_str());
  CHECK(status == STATUS_OK);

  ABinderProcess_joinThreadPool();
  return 0;
}
