/*
 * Copyright (C) 2018 The Android Open Source Project
 * Copyright (C) 2019, The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.1
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "android.hardware.radio@1.3-radio-service.samsung"

#include <android/hardware/radio/1.3/IRadio.h>
#include <hidl/HidlTransportSupport.h>

#include "Radio.h"

using android::OK;
using android::sp;
using android::status_t;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::radio::V1_3::IRadio;
using android::hardware::radio::V1_3::implementation::Radio;

int main() {
    configureRpcThreadpool(1, true);

    sp<IRadio> radio = new Radio(RIL1_SERVICE_NAME);
    status_t status = radio->registerAsService(RIL1_SERVICE_NAME);
    ALOGW_IF(status != OK, "Could not register IRadio v1.3 %s", RIL1_SERVICE_NAME);
    radio = new Radio(RIL2_SERVICE_NAME);
    status = radio->registerAsService(RIL2_SERVICE_NAME);
    ALOGW_IF(status != OK, "Could not register IRadio v1.3 %s", RIL2_SERVICE_NAME);
    ALOGD("Default service is ready.");

    joinRpcThreadpool();
    return 1;
}
