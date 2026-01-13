/*
 * Copyright (C) 2021 The Android Open Source Project
 * Copyright (C) 2024 The LineageOS Project
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

#define LOG_TAG "android.hardware.usb.gadget-service.samsung"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <utils/Log.h>
#include "UsbGadget.h"

using android::OK;
using android::sp;
using android::status_t;

using aidl::android::hardware::usb::gadget::UsbGadget;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<UsbGadget> usbgadget = ndk::SharedRefBase::make<UsbGadget>();

    const std::string instance = std::string() + UsbGadget::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(usbgadget->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ALOGV("AIDL USB Gadget HAL about to start");
    ABinderProcess_joinThreadPool();
    return -1;  // Should never be reached
}
