/*
 * SPDX-FileCopyrightText: 2016 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.bluetooth@1.0-service.samsung"

#include <hidl/HidlTransportSupport.h>
#include <utils/Log.h>

#include "bluetooth_hci.h"

// Extra threads make priority inheritance faster.
static const size_t kMaxThreads = 5;

using android::sp;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::bluetooth::V1_0::IBluetoothHci;
using android::hardware::bluetooth::V1_0::implementation::BluetoothHci;

int main() {
    configureRpcThreadpool(kMaxThreads, true /* callerWillJoin */);

    sp<IBluetoothHci> hci = new BluetoothHci();
    if (hci->registerAsService() != android::OK) {
        ALOGE("Could not register IBluetoothHci");
        return 1;
    }

    joinRpcThreadpool();
    return 1;
}
