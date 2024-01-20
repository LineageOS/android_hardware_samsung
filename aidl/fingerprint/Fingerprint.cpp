/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Fingerprint.h"
#include "VendorConstants.h"

#include <fingerprint.sysprop.h>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <fcntl.h>
#include <linux/uinput.h>

using namespace ::android::fingerprint::samsung;

using ::android::base::ParseInt;
using ::android::base::Split;

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace {
constexpr int SENSOR_ID = 0;
constexpr common::SensorStrength SENSOR_STRENGTH = common::SensorStrength::STRONG;
constexpr int MAX_ENROLLMENTS_PER_USER = 4;
constexpr bool SUPPORTS_NAVIGATION_GESTURES = false;
constexpr char HW_COMPONENT_ID[] = "fingerprintSensor";
constexpr char HW_VERSION[] = "vendor/model/revision";
constexpr char FW_VERSION[] = "1.01";
constexpr char SERIAL_NUMBER[] = "00000001";
constexpr char SW_COMPONENT_ID[] = "matchingAlgorithm";
constexpr char SW_VERSION[] = "vendor/version/revision";
}

static Fingerprint* sInstance;

Fingerprint::Fingerprint() {
    sInstance = this; // keep track of the most recent instance
    if (!mHal.openHal(Fingerprint::notify)) {
        LOG(ERROR) << "Can't open HAL module";
    }

    std::string sensorTypeProp = FingerprintHalProperties::type().value_or("");
    if (sensorTypeProp == "" || sensorTypeProp == "default" || sensorTypeProp == "rear")
        mSensorType = FingerprintSensorType::REAR;
    else if (sensorTypeProp == "udfps")
        mSensorType = FingerprintSensorType::UNDER_DISPLAY_ULTRASONIC;
    else if (sensorTypeProp == "udfps_optical")
        mSensorType = FingerprintSensorType::UNDER_DISPLAY_OPTICAL;
    else if (sensorTypeProp == "side")
        mSensorType = FingerprintSensorType::POWER_BUTTON;
    else if (sensorTypeProp == "home")
        mSensorType = FingerprintSensorType::HOME_BUTTON;
    else
        mSensorType = FingerprintSensorType::UNKNOWN;

    mMaxEnrollmentsPerUser =
            FingerprintHalProperties::max_enrollments_per_user().value_or(MAX_ENROLLMENTS_PER_USER);
    mSupportsGestures =
            FingerprintHalProperties::supports_gestures().value_or(SUPPORTS_NAVIGATION_GESTURES);

    if (mSupportsGestures) {
        mHal.request(FINGERPRINT_REQUEST_NAVIGATION_MODE_START, 1);

        uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (uinputFd < 0) {
            LOG(ERROR) << "Unable to open uinput node";
            goto skip_uinput_setup;
        }

        int err = ioctl(uinputFd, UI_SET_EVBIT, EV_KEY) |
              ioctl(uinputFd, UI_SET_KEYBIT, KEY_UP) |
              ioctl(uinputFd, UI_SET_KEYBIT, KEY_DOWN);
        if (err != 0) {
            LOG(ERROR) << "Unable to enable key events";
            goto skip_uinput_setup;
        }

        struct uinput_user_dev uidev;
        sprintf(uidev.name, "uinput-sec-fp");
        uidev.id.bustype = BUS_VIRTUAL;

        err = write(uinputFd, &uidev, sizeof(uidev));
        if (err < 0) {
            LOG(ERROR) << "Write user device to uinput node failed";
            goto skip_uinput_setup;
        }

        err = ioctl(uinputFd, UI_DEV_CREATE);
        if (err < 0) {
            LOG(ERROR) << "Unable to create uinput device";
            goto skip_uinput_setup;
        }

        LOG(INFO) << "Successfully registered uinput-sec-fp for fingerprint gestures";
    }
skip_uinput_setup:
    return;
}

ndk::ScopedAStatus Fingerprint::getSensorProps(std::vector<SensorProps>* out) {
    std::vector<common::ComponentInfo> componentInfo = {
            {HW_COMPONENT_ID, HW_VERSION, FW_VERSION, SERIAL_NUMBER, "" /* softwareVersion */},
            {SW_COMPONENT_ID, "" /* hardwareVersion */, "" /* firmwareVersion */,
            "" /* serialNumber */, SW_VERSION}};
    common::CommonProps commonProps = {SENSOR_ID, SENSOR_STRENGTH,
                                       mMaxEnrollmentsPerUser, componentInfo};

    SensorLocation sensorLocation;
    std::string loc = FingerprintHalProperties::sensor_location().value_or("");
    std::vector<std::string> dim = Split(loc, "|");
    if (dim.size() >= 3 && dim.size() <= 4) {
        ParseInt(dim[0], &sensorLocation.sensorLocationX);
        ParseInt(dim[1], &sensorLocation.sensorLocationY);
        ParseInt(dim[2], &sensorLocation.sensorRadius);

        if (dim.size() >= 4)
            sensorLocation.display = dim[3];
    } else if(loc.length() > 0) {
        LOG(WARNING) << "Invalid sensor location input (x|y|radius|display): " << loc;
    }

    LOG(INFO) << "Sensor type: " << ::android::internal::ToString(mSensorType)
              << " location: " << sensorLocation.toString();

    *out = {{commonProps,
             mSensorType,
             {sensorLocation},
             mSupportsGestures,
             false,
             false,
             false,
             std::nullopt}};

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Fingerprint::createSession(int32_t /*sensorId*/, int32_t userId,
                                              const std::shared_ptr<ISessionCallback>& cb,
                                              std::shared_ptr<ISession>* out) {
    CHECK(mSession == nullptr || mSession->isClosed()) << "Open session already exists!";

    mSession = SharedRefBase::make<Session>(mHal, userId, cb, mLockoutTracker);
    *out = mSession;

    mSession->linkToDeath(cb->asBinder().get());

    return ndk::ScopedAStatus::ok();
}

void Fingerprint::notify(const fingerprint_msg_t* msg) {
    Fingerprint* thisPtr = sInstance;
    if (msg->type == FINGERPRINT_ACQUIRED
        && msg->data.acquired.acquired_info > SEM_FINGERPRINT_EVENT_BASE) {
        thisPtr->handleEvent(msg->data.acquired.acquired_info);
        return;
    }

    if (thisPtr->mSession == nullptr || thisPtr->mSession->isClosed()) {
        LOG(ERROR) << "Receiving callbacks before a session is opened.";
        return;
    }

    thisPtr->mSession->notify(msg);
}

void Fingerprint::handleEvent(int eventCode) {
    switch (eventCode) {
        case SEM_FINGERPRINT_EVENT_GESTURE_SWIPE_DOWN:
        case SEM_FINGERPRINT_EVENT_GESTURE_SWIPE_UP:
            if (!mSupportsGestures) return;

            struct input_event event {};
            int keycode = eventCode == SEM_FINGERPRINT_EVENT_GESTURE_SWIPE_UP ?
                          KEY_UP : KEY_DOWN;

            // Report the key
            event.type = EV_KEY;
            event.code = keycode;
            event.value = 1;
            if (write(uinputFd, &event, sizeof(event)) < 0) {
                LOG(ERROR) << "Write EV_KEY to uinput node failed";
                return;
            }

            // Force a flush with an EV_SYN
            event.type = EV_SYN;
            event.code = SYN_REPORT;
            event.value = 0;
            if (write(uinputFd, &event, sizeof(event)) < 0) {
                LOG(ERROR) << "Write EV_SYN to uinput node failed";
                return;
            }

            // Report the key
            event.type = EV_KEY;
            event.code = keycode;
            event.value = 0;
            if (write(uinputFd, &event, sizeof(event)) < 0) {
                LOG(ERROR) << "Write EV_KEY to uinput node failed";
                return;
            }

            // Force a flush with an EV_SYN
            event.type = EV_SYN;
            event.code = SYN_REPORT;
            event.value = 0;
            if (write(uinputFd, &event, sizeof(event)) < 0) {
                LOG(ERROR) << "Write EV_SYN to uinput node failed";
                return;
            }
        break;
    }
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
