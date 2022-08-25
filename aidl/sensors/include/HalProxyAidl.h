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
#include "HalProxy.h"

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

class HalProxyAidl : public ::android::hardware::sensors::V2_1::implementation::HalProxy,
                     public ::aidl::android::hardware::sensors::BnSensors {
    ::ndk::ScopedAStatus activate(int32_t in_sensorHandle, bool in_enabled) override;
    ::ndk::ScopedAStatus batch(int32_t in_sensorHandle, int64_t in_samplingPeriodNs,
                               int64_t in_maxReportLatencyNs) override;
    ::ndk::ScopedAStatus configDirectReport(
            int32_t in_sensorHandle, int32_t in_channelHandle,
            ::aidl::android::hardware::sensors::ISensors::RateLevel in_rate,
            int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus flush(int32_t in_sensorHandle) override;
    ::ndk::ScopedAStatus getSensorsList(
            std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) override;
    ::ndk::ScopedAStatus initialize(
            const ::aidl::android::hardware::common::fmq::MQDescriptor<
                    ::aidl::android::hardware::sensors::Event,
                    ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>&
                    in_eventQueueDescriptor,
            const ::aidl::android::hardware::common::fmq::MQDescriptor<
                    int32_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite>&
                    in_wakeLockDescriptor,
            const std::shared_ptr<::aidl::android::hardware::sensors::ISensorsCallback>&
                    in_sensorsCallback) override;
    ::ndk::ScopedAStatus injectSensorData(
            const ::aidl::android::hardware::sensors::Event& in_event) override;
    ::ndk::ScopedAStatus registerDirectChannel(
            const ::aidl::android::hardware::sensors::ISensors::SharedMemInfo& in_mem,
            int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus setOperationMode(
            ::aidl::android::hardware::sensors::ISensors::OperationMode in_mode) override;
    ::ndk::ScopedAStatus unregisterDirectChannel(int32_t in_channelHandle) override;

    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
