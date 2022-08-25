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

#include "ConvertUtils.h"
#include <android-base/logging.h>
#include <log/log.h>

using AidlSensorInfo = ::aidl::android::hardware::sensors::SensorInfo;
using AidlSensorType = ::aidl::android::hardware::sensors::SensorType;
using AidlEvent = ::aidl::android::hardware::sensors::Event;
using AidlSensorStatus = ::aidl::android::hardware::sensors::SensorStatus;
using ::aidl::android::hardware::sensors::AdditionalInfo;
using ::aidl::android::hardware::sensors::DynamicSensorInfo;
using ::android::hardware::sensors::V1_0::MetaDataEventType;
using V1_0SensorStatus = ::android::hardware::sensors::V1_0::SensorStatus;
using ::android::hardware::sensors::V1_0::AdditionalInfoType;
using V2_1SensorInfo = ::android::hardware::sensors::V2_1::SensorInfo;
using V2_1Event = ::android::hardware::sensors::V2_1::Event;
using V2_1SensorType = ::android::hardware::sensors::V2_1::SensorType;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

AidlSensorInfo convertSensorInfo(const V2_1SensorInfo& sensorInfo) {
    AidlSensorInfo aidlSensorInfo;
    aidlSensorInfo.sensorHandle = sensorInfo.sensorHandle;
    aidlSensorInfo.name = sensorInfo.name;
    aidlSensorInfo.vendor = sensorInfo.vendor;
    aidlSensorInfo.version = sensorInfo.version;
    aidlSensorInfo.type = (AidlSensorType)sensorInfo.type;
    aidlSensorInfo.typeAsString = sensorInfo.typeAsString;
    aidlSensorInfo.maxRange = sensorInfo.maxRange;
    aidlSensorInfo.resolution = sensorInfo.resolution;
    aidlSensorInfo.power = sensorInfo.power;
    aidlSensorInfo.minDelayUs = sensorInfo.minDelay;
    aidlSensorInfo.fifoReservedEventCount = sensorInfo.fifoReservedEventCount;
    aidlSensorInfo.fifoMaxEventCount = sensorInfo.fifoMaxEventCount;
    aidlSensorInfo.requiredPermission = sensorInfo.requiredPermission;
    aidlSensorInfo.maxDelayUs = sensorInfo.maxDelay;
    aidlSensorInfo.flags = sensorInfo.flags;
    return aidlSensorInfo;
}

void convertToHidlEvent(const AidlEvent& aidlEvent, V2_1Event* hidlEvent) {
    static_assert(decltype(hidlEvent->u.data)::elementCount() == 16);
    hidlEvent->timestamp = aidlEvent.timestamp;
    hidlEvent->sensorHandle = aidlEvent.sensorHandle;
    hidlEvent->sensorType = (V2_1SensorType)aidlEvent.sensorType;

    switch (aidlEvent.sensorType) {
        case AidlSensorType::META_DATA:
            hidlEvent->u.meta.what =
                    (MetaDataEventType)aidlEvent.payload.get<Event::EventPayload::meta>().what;
            break;
        case AidlSensorType::ACCELEROMETER:
        case AidlSensorType::MAGNETIC_FIELD:
        case AidlSensorType::ORIENTATION:
        case AidlSensorType::GYROSCOPE:
        case AidlSensorType::GRAVITY:
        case AidlSensorType::LINEAR_ACCELERATION:
            hidlEvent->u.vec3.x = aidlEvent.payload.get<Event::EventPayload::vec3>().x;
            hidlEvent->u.vec3.y = aidlEvent.payload.get<Event::EventPayload::vec3>().y;
            hidlEvent->u.vec3.z = aidlEvent.payload.get<Event::EventPayload::vec3>().z;
            hidlEvent->u.vec3.status =
                    (V1_0SensorStatus)aidlEvent.payload.get<Event::EventPayload::vec3>().status;
            break;
        case AidlSensorType::GAME_ROTATION_VECTOR:
            hidlEvent->u.vec4.x = aidlEvent.payload.get<Event::EventPayload::vec4>().x;
            hidlEvent->u.vec4.y = aidlEvent.payload.get<Event::EventPayload::vec4>().y;
            hidlEvent->u.vec4.z = aidlEvent.payload.get<Event::EventPayload::vec4>().z;
            hidlEvent->u.vec4.w = aidlEvent.payload.get<Event::EventPayload::vec4>().w;
            break;
        case AidlSensorType::ROTATION_VECTOR:
        case AidlSensorType::GEOMAGNETIC_ROTATION_VECTOR:
            std::copy(aidlEvent.payload.get<Event::EventPayload::data>().values.data(),
                      aidlEvent.payload.get<Event::EventPayload::data>().values.data() + 5,
                      hidlEvent->u.data.data());
            break;
        case AidlSensorType::ACCELEROMETER_UNCALIBRATED:
        case AidlSensorType::MAGNETIC_FIELD_UNCALIBRATED:
        case AidlSensorType::GYROSCOPE_UNCALIBRATED:
            hidlEvent->u.uncal.x = aidlEvent.payload.get<Event::EventPayload::uncal>().x;
            hidlEvent->u.uncal.y = aidlEvent.payload.get<Event::EventPayload::uncal>().y;
            hidlEvent->u.uncal.z = aidlEvent.payload.get<Event::EventPayload::uncal>().z;
            hidlEvent->u.uncal.x_bias = aidlEvent.payload.get<Event::EventPayload::uncal>().xBias;
            hidlEvent->u.uncal.y_bias = aidlEvent.payload.get<Event::EventPayload::uncal>().yBias;
            hidlEvent->u.uncal.z_bias = aidlEvent.payload.get<Event::EventPayload::uncal>().zBias;
            break;
        case AidlSensorType::DEVICE_ORIENTATION:
        case AidlSensorType::LIGHT:
        case AidlSensorType::PRESSURE:
        case AidlSensorType::PROXIMITY:
        case AidlSensorType::RELATIVE_HUMIDITY:
        case AidlSensorType::AMBIENT_TEMPERATURE:
        case AidlSensorType::SIGNIFICANT_MOTION:
        case AidlSensorType::STEP_DETECTOR:
        case AidlSensorType::TILT_DETECTOR:
        case AidlSensorType::WAKE_GESTURE:
        case AidlSensorType::GLANCE_GESTURE:
        case AidlSensorType::PICK_UP_GESTURE:
        case AidlSensorType::WRIST_TILT_GESTURE:
        case AidlSensorType::STATIONARY_DETECT:
        case AidlSensorType::MOTION_DETECT:
        case AidlSensorType::HEART_BEAT:
        case AidlSensorType::LOW_LATENCY_OFFBODY_DETECT:
        case AidlSensorType::HINGE_ANGLE:
            hidlEvent->u.scalar = aidlEvent.payload.get<Event::EventPayload::scalar>();
            break;
        case AidlSensorType::STEP_COUNTER:
            hidlEvent->u.stepCount = aidlEvent.payload.get<AidlEvent::EventPayload::stepCount>();
            break;
        case AidlSensorType::HEART_RATE:
            hidlEvent->u.heartRate.bpm =
                    aidlEvent.payload.get<AidlEvent::EventPayload::heartRate>().bpm;
            hidlEvent->u.heartRate.status =
                    (V1_0SensorStatus)aidlEvent.payload.get<Event::EventPayload::heartRate>()
                            .status;
            break;
        case AidlSensorType::POSE_6DOF:
            std::copy(std::begin(aidlEvent.payload.get<AidlEvent::EventPayload::pose6DOF>().values),
                      std::end(aidlEvent.payload.get<AidlEvent::EventPayload::pose6DOF>().values),
                      hidlEvent->u.pose6DOF.data());
            break;
        case AidlSensorType::DYNAMIC_SENSOR_META:
            hidlEvent->u.dynamic.connected =
                    aidlEvent.payload.get<Event::EventPayload::dynamic>().connected;
            hidlEvent->u.dynamic.sensorHandle =
                    aidlEvent.payload.get<Event::EventPayload::dynamic>().sensorHandle;
            std::copy(
                    std::begin(
                            aidlEvent.payload.get<AidlEvent::EventPayload::dynamic>().uuid.values),
                    std::end(aidlEvent.payload.get<AidlEvent::EventPayload::dynamic>().uuid.values),
                    hidlEvent->u.dynamic.uuid.data());
            break;
        case AidlSensorType::ADDITIONAL_INFO: {
            const AdditionalInfo& additionalInfo =
                    aidlEvent.payload.get<AidlEvent::EventPayload::additional>();
            hidlEvent->u.additional.type = (AdditionalInfoType)additionalInfo.type;
            hidlEvent->u.additional.serial = additionalInfo.serial;

            switch (additionalInfo.payload.getTag()) {
                case AdditionalInfo::AdditionalInfoPayload::Tag::dataInt32: {
                    const auto& aidlData =
                            additionalInfo.payload
                                    .get<AdditionalInfo::AdditionalInfoPayload::dataInt32>()
                                    .values;
                    std::copy(std::begin(aidlData), std::end(aidlData),
                              hidlEvent->u.additional.u.data_int32.data());
                    break;
                }
                case AdditionalInfo::AdditionalInfoPayload::Tag::dataFloat: {
                    const auto& aidlData =
                            additionalInfo.payload
                                    .get<AdditionalInfo::AdditionalInfoPayload::dataFloat>()
                                    .values;
                    std::copy(std::begin(aidlData), std::end(aidlData),
                              hidlEvent->u.additional.u.data_float.data());
                    break;
                }
                default:
                    ALOGE("Invalid sensor additioanl info tag: %d",
                          static_cast<int32_t>(additionalInfo.payload.getTag()));
                    break;
            }
            break;
        }
        case AidlSensorType::HEAD_TRACKER: {
            const auto& ht = aidlEvent.payload.get<Event::EventPayload::headTracker>();
            hidlEvent->u.data[0] = ht.rx;
            hidlEvent->u.data[1] = ht.ry;
            hidlEvent->u.data[2] = ht.rz;
            hidlEvent->u.data[3] = ht.vx;
            hidlEvent->u.data[4] = ht.vy;
            hidlEvent->u.data[5] = ht.vz;

            // IMPORTANT: Because we want to preserve the data range of discontinuityCount,
            // we assume the data can be interpreted as an int32_t directly (e.g. the underlying
            // HIDL HAL must be using memcpy or equivalent to store this value).
            *(reinterpret_cast<int32_t*>(&hidlEvent->u.data[6])) = ht.discontinuityCount;
            break;
        }
        default: {
            CHECK_GE((int32_t)aidlEvent.sensorType, (int32_t)SensorType::DEVICE_PRIVATE_BASE);
            std::copy(std::begin(aidlEvent.payload.get<AidlEvent::EventPayload::data>().values),
                      std::end(aidlEvent.payload.get<AidlEvent::EventPayload::data>().values),
                      hidlEvent->u.data.data());
            break;
        }
    }
}

void convertToAidlEvent(const V2_1Event& hidlEvent, AidlEvent* aidlEvent) {
    static_assert(decltype(hidlEvent.u.data)::elementCount() == 16);
    aidlEvent->timestamp = hidlEvent.timestamp;
    aidlEvent->sensorHandle = hidlEvent.sensorHandle;
    aidlEvent->sensorType = (AidlSensorType)hidlEvent.sensorType;
    switch (hidlEvent.sensorType) {
        case V2_1SensorType::META_DATA: {
            AidlEvent::EventPayload::MetaData meta;
            meta.what = (Event::EventPayload::MetaData::MetaDataEventType)hidlEvent.u.meta.what;
            aidlEvent->payload.set<Event::EventPayload::meta>(meta);
            break;
        }
        case V2_1SensorType::ACCELEROMETER:
        case V2_1SensorType::MAGNETIC_FIELD:
        case V2_1SensorType::ORIENTATION:
        case V2_1SensorType::GYROSCOPE:
        case V2_1SensorType::GRAVITY:
        case V2_1SensorType::LINEAR_ACCELERATION: {
            AidlEvent::EventPayload::Vec3 vec3;
            vec3.x = hidlEvent.u.vec3.x;
            vec3.y = hidlEvent.u.vec3.y;
            vec3.z = hidlEvent.u.vec3.z;
            vec3.status = (SensorStatus)hidlEvent.u.vec3.status;
            aidlEvent->payload.set<Event::EventPayload::vec3>(vec3);
            break;
        }
        case V2_1SensorType::GAME_ROTATION_VECTOR: {
            AidlEvent::EventPayload::Vec4 vec4;
            vec4.x = hidlEvent.u.vec4.x;
            vec4.y = hidlEvent.u.vec4.y;
            vec4.z = hidlEvent.u.vec4.z;
            vec4.w = hidlEvent.u.vec4.w;
            aidlEvent->payload.set<Event::EventPayload::vec4>(vec4);
            break;
        }
        case V2_1SensorType::ROTATION_VECTOR:
        case V2_1SensorType::GEOMAGNETIC_ROTATION_VECTOR: {
            AidlEvent::EventPayload::Data data;
            std::copy(hidlEvent.u.data.data(), hidlEvent.u.data.data() + 5,
                      std::begin(data.values));
            aidlEvent->payload.set<Event::EventPayload::data>(data);
            break;
        }
        case V2_1SensorType::MAGNETIC_FIELD_UNCALIBRATED:
        case V2_1SensorType::GYROSCOPE_UNCALIBRATED:
        case V2_1SensorType::ACCELEROMETER_UNCALIBRATED: {
            AidlEvent::EventPayload::Uncal uncal;
            uncal.x = hidlEvent.u.uncal.x;
            uncal.y = hidlEvent.u.uncal.y;
            uncal.z = hidlEvent.u.uncal.z;
            uncal.xBias = hidlEvent.u.uncal.x_bias;
            uncal.yBias = hidlEvent.u.uncal.y_bias;
            uncal.zBias = hidlEvent.u.uncal.z_bias;
            aidlEvent->payload.set<Event::EventPayload::uncal>(uncal);
            break;
        }
        case V2_1SensorType::DEVICE_ORIENTATION:
        case V2_1SensorType::LIGHT:
        case V2_1SensorType::PRESSURE:
        case V2_1SensorType::PROXIMITY:
        case V2_1SensorType::RELATIVE_HUMIDITY:
        case V2_1SensorType::AMBIENT_TEMPERATURE:
        case V2_1SensorType::SIGNIFICANT_MOTION:
        case V2_1SensorType::STEP_DETECTOR:
        case V2_1SensorType::TILT_DETECTOR:
        case V2_1SensorType::WAKE_GESTURE:
        case V2_1SensorType::GLANCE_GESTURE:
        case V2_1SensorType::PICK_UP_GESTURE:
        case V2_1SensorType::WRIST_TILT_GESTURE:
        case V2_1SensorType::STATIONARY_DETECT:
        case V2_1SensorType::MOTION_DETECT:
        case V2_1SensorType::HEART_BEAT:
        case V2_1SensorType::LOW_LATENCY_OFFBODY_DETECT:
        case V2_1SensorType::HINGE_ANGLE:
            aidlEvent->payload.set<Event::EventPayload::scalar>(hidlEvent.u.scalar);
            break;
        case V2_1SensorType::STEP_COUNTER:
            aidlEvent->payload.set<Event::EventPayload::stepCount>(hidlEvent.u.stepCount);
            break;
        case V2_1SensorType::HEART_RATE: {
            AidlEvent::EventPayload::HeartRate heartRate;
            heartRate.bpm = hidlEvent.u.heartRate.bpm;
            heartRate.status = (SensorStatus)hidlEvent.u.heartRate.status;
            aidlEvent->payload.set<Event::EventPayload::heartRate>(heartRate);
            break;
        }
        case V2_1SensorType::POSE_6DOF: {
            AidlEvent::EventPayload::Pose6Dof pose6Dof;
            std::copy(hidlEvent.u.pose6DOF.data(),
                      hidlEvent.u.pose6DOF.data() + hidlEvent.u.pose6DOF.size(),
                      std::begin(pose6Dof.values));
            aidlEvent->payload.set<Event::EventPayload::pose6DOF>(pose6Dof);
            break;
        }
        case V2_1SensorType::DYNAMIC_SENSOR_META: {
            DynamicSensorInfo dynamicSensorInfo;
            dynamicSensorInfo.connected = hidlEvent.u.dynamic.connected;
            dynamicSensorInfo.sensorHandle = hidlEvent.u.dynamic.sensorHandle;
            std::copy(hidlEvent.u.dynamic.uuid.data(),
                      hidlEvent.u.dynamic.uuid.data() + hidlEvent.u.dynamic.uuid.size(),
                      std::begin(dynamicSensorInfo.uuid.values));
            aidlEvent->payload.set<Event::EventPayload::dynamic>(dynamicSensorInfo);
            break;
        }
        case V2_1SensorType::ADDITIONAL_INFO: {
            AdditionalInfo additionalInfo;
            additionalInfo.type = (AdditionalInfo::AdditionalInfoType)hidlEvent.u.additional.type;
            additionalInfo.serial = hidlEvent.u.additional.serial;

            AdditionalInfo::AdditionalInfoPayload::Int32Values int32Values;
            std::copy(hidlEvent.u.additional.u.data_int32.data(),
                      hidlEvent.u.additional.u.data_int32.data() +
                              hidlEvent.u.additional.u.data_int32.size(),
                      std::begin(int32Values.values));
            additionalInfo.payload.set<AdditionalInfo::AdditionalInfoPayload::dataInt32>(
                    int32Values);
            aidlEvent->payload.set<Event::EventPayload::additional>(additionalInfo);
            break;
        }
        default: {
            if (static_cast<int32_t>(hidlEvent.sensorType) ==
                static_cast<int32_t>(AidlSensorType::HEAD_TRACKER)) {
                Event::EventPayload::HeadTracker headTracker;
                headTracker.rx = hidlEvent.u.data[0];
                headTracker.ry = hidlEvent.u.data[1];
                headTracker.rz = hidlEvent.u.data[2];
                headTracker.vx = hidlEvent.u.data[3];
                headTracker.vy = hidlEvent.u.data[4];
                headTracker.vz = hidlEvent.u.data[5];

                // IMPORTANT: Because we want to preserve the data range of discontinuityCount,
                // we assume the data can be interpreted as an int32_t directly (e.g. the underlying
                // HIDL HAL must be using memcpy or equivalent to store this value).
                headTracker.discontinuityCount =
                        *(reinterpret_cast<const int32_t*>(&hidlEvent.u.data[6]));

                aidlEvent->payload.set<Event::EventPayload::Tag::headTracker>(headTracker);
            } else {
                CHECK_GE((int32_t)hidlEvent.sensorType,
                         (int32_t)V2_1SensorType::DEVICE_PRIVATE_BASE);
                AidlEvent::EventPayload::Data data;
                std::copy(hidlEvent.u.data.data(),
                          hidlEvent.u.data.data() + hidlEvent.u.data.size(),
                          std::begin(data.values));
                aidlEvent->payload.set<Event::EventPayload::data>(data);
            }
            break;
        }
    }
}

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
