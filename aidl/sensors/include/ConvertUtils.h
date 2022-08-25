/*
 * Copyright (C) 2021 The Android Open Source Project
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

#pragma once

#include <aidl/android/hardware/sensors/BnSensors.h>
#include <android/hardware/sensors/2.1/types.h>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

/**
 * Generates an AIDL SensorInfo instance from the passed HIDL V2.1 SensorInfo instance.
 */
::aidl::android::hardware::sensors::SensorInfo convertSensorInfo(
        const ::android::hardware::sensors::V2_1::SensorInfo& sensorInfo);

/**
 * Populates a HIDL V2.1 Event instance based on an AIDL Event instance.
 */
void convertToHidlEvent(const ::aidl::android::hardware::sensors::Event& aidlEvent,
                        ::android::hardware::sensors::V2_1::Event* hidlEvent);

/**
 * Populates an AIDL Event instance based on a HIDL V2.1 Event instance.
 */
void convertToAidlEvent(const ::android::hardware::sensors::V2_1::Event& hidlEvent,
                        ::aidl::android::hardware::sensors::Event* aidlEvent);

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl