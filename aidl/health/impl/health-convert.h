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

#include <aidl/android/hardware/health/HealthInfo.h>
#include <batteryservice/BatteryService.h>
#include <healthd/healthd.h>

// Conversion between healthd types and AIDL health HAL types. Note that most
// of the conversion loses information, because these types have a different
// set of information.

namespace aidl::android::hardware::health {

void convert(const HealthInfo& info, struct ::android::BatteryProperties* out);

}  // namespace aidl::android::hardware::health
