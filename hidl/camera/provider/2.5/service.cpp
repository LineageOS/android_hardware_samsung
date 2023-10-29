/*
 * Copyright 2019 The Android Open Source Project
 * Copyright 2021 The LineageOS Project
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

#define LOG_TAG "android.hardware.camera.provider@2.5-service.samsung"

#include <android/hardware/camera/provider/2.5/ICameraProvider.h>
#include <binder/ProcessState.h>
#include <hidl/HidlLazyUtils.h>
#include <hidl/HidlTransportSupport.h>

#include "CameraProvider_2_5.h"
#include "SamsungCameraProvider.h"

using android::status_t;
using android::hardware::camera::provider::V2_5::ICameraProvider;

int main()
{
    using namespace android::hardware::camera::provider::V2_5::implementation;

    ALOGI("CameraProvider@2.5 legacy service is starting.");

    ::android::hardware::configureRpcThreadpool(/*threads*/ HWBINDER_THREAD_COUNT, /*willJoin*/ true);

    ::android::sp<ICameraProvider> provider = new CameraProvider<SamsungCameraProvider>();

    status_t status = provider->registerAsService("legacy/0");
    LOG_ALWAYS_FATAL_IF(status != android::OK, "Error while registering provider service: %d",
            status);

    ::android::hardware::joinRpcThreadpool();

    return 0;
}
