/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.usb.gadget.aidl-service"

#include "UsbGadget.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/properties.h>

#include <aidl/android/frameworks/stats/IStats.h>

namespace aidl {
namespace android {
namespace hardware {
namespace usb {
namespace gadget {

string enabledPath;

using ::android::base::GetBoolProperty;
using ::android::hardware::google::pixel::usb::kUvcEnabled;

UsbGadget::UsbGadget() {
    if (access(OS_DESC_PATH, R_OK) != 0) {
        ALOGE("configfs setup not done yet");
        abort();
    }
}

void currentFunctionsAppliedCallback(bool functionsApplied, void *payload) {
    UsbGadget *gadget = (UsbGadget *)payload;
    gadget->mCurrentUsbFunctionsApplied = functionsApplied;
}

ScopedAStatus UsbGadget::getCurrentUsbFunctions(const shared_ptr<IUsbGadgetCallback> &callback,
        int64_t in_transactionId) {
    ScopedAStatus ret = callback->getCurrentUsbFunctionsCb(
        mCurrentUsbFunctions,
        mCurrentUsbFunctionsApplied ? Status::FUNCTIONS_APPLIED : Status::FUNCTIONS_NOT_APPLIED,
        in_transactionId);
    if (!ret.isOk())
        ALOGE("Call to getCurrentUsbFunctionsCb failed %s", ret.getDescription().c_str());

    return ScopedAStatus::ok();
}

ScopedAStatus UsbGadget::getUsbSpeed(const shared_ptr<IUsbGadgetCallback> &callback,
        int64_t in_transactionId) {
    std::string current_speed;
    if (ReadFileToString(SPEED_PATH, &current_speed)) {
        current_speed = Trim(current_speed);
        ALOGI("current USB speed is %s", current_speed.c_str());
        if (current_speed == "low-speed")
            mUsbSpeed = UsbSpeed::LOWSPEED;
        else if (current_speed == "full-speed")
            mUsbSpeed = UsbSpeed::FULLSPEED;
        else if (current_speed == "high-speed")
            mUsbSpeed = UsbSpeed::HIGHSPEED;
        else if (current_speed == "super-speed")
            mUsbSpeed = UsbSpeed::SUPERSPEED;
        else if (current_speed == "super-speed-plus")
            mUsbSpeed = UsbSpeed::SUPERSPEED_10Gb;
        else if (current_speed == "UNKNOWN")
            mUsbSpeed = UsbSpeed::UNKNOWN;
        else
            mUsbSpeed = UsbSpeed::UNKNOWN;
    } else {
        ALOGE("Fail to read current speed");
        mUsbSpeed = UsbSpeed::UNKNOWN;
    }

    if (callback) {
        ScopedAStatus ret = callback->getUsbSpeedCb(mUsbSpeed, in_transactionId);

        if (!ret.isOk())
            ALOGE("Call to getUsbSpeedCb failed %s", ret.getDescription().c_str());
    }

    return ScopedAStatus::ok();
}

Status UsbGadget::tearDownGadget() {
    if (Status(resetGadget()) != Status::SUCCESS){
        return Status::ERROR;
    }

    if (monitorFfs.isMonitorRunning()) {
        monitorFfs.reset();
    } else {
        ALOGI("mMonitor not running");
    }
    return Status::SUCCESS;
}

static Status validateAndSetVidPid(int64_t functions) {
    Status ret;
    const char *vid, *pid;

    switch (functions) {
        case GadgetFunction::MTP:
            vid = "0x04E8";
            pid = "0x6860";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::MTP:
            vid = "0x04E8";
            pid = "0x6860";
            break;
        case GadgetFunction::RNDIS:
            vid = "0x04E8";
            pid = "0x6863";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::RNDIS:
            vid = "0x04E8";
            pid = "0x6864";
            break;
        case GadgetFunction::PTP:
            vid = "0x04E8";
            pid = "0x6865";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::PTP:
            vid = "0x04E8";
            pid = "0x6865";
            break;
        case GadgetFunction::ADB:
            vid = "0x04E8";
            pid = "0x685D";
            break;
        case GadgetFunction::MIDI:
            vid = "0x04E8";
            pid = "0x686C";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::MIDI:
            vid = "0x04E8";
            pid = "0x686C";
            break;
        case GadgetFunction::ACCESSORY:
            vid = "0x18D1";
            pid = "0x2D00";
            break;
        case GadgetFunction::ADB |
                 GadgetFunction::ACCESSORY:
            vid = "0x18d1";
            pid = "0x2d01";
            break;
        case GadgetFunction::AUDIO_SOURCE:
            vid = "0x18D1";
            pid = "0x2D02";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::AUDIO_SOURCE:
            vid = "0x18D1";
            pid = "0x2D03";
            break;
        case GadgetFunction::ACCESSORY |
                GadgetFunction::AUDIO_SOURCE:
            vid = "0x18D1";
            pid = "0x2D04";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::ACCESSORY |
                GadgetFunction::AUDIO_SOURCE:
            vid = "0x18D1";
            pid = "0x2D05";
            break;
        case GadgetFunction::NCM:
            vid = "0x04E8";
            pid = "0x685D";
            break;
        case GadgetFunction::ADB |
                GadgetFunction::NCM:
            vid = "0x04E8";
            pid = "0x685D";
            break;
        /*
         * There are no known Samsung devices that implement the
         * device as webcam feature, so for the following functions
         * we use Google's vid/pid.
         */
        case GadgetFunction::UVC:
            vid = "0x18D1";
            pid = "0x4EED";
            break;
        case GadgetFunction::ADB | GadgetFunction::UVC:
            vid = "0x18D1";
            pid = "0x4EEE";
            break;
        default:
            ALOGE("Combination not supported");
            ret = Status::CONFIGURATION_NOT_SUPPORTED;
            goto error;
    }

    ret = Status(setVidPid(vid, pid));
    if (ret != Status::SUCCESS) {
        ALOGE("Failed to update vid/pid");
        goto error;
    }

error:
    return ret;
}

ScopedAStatus UsbGadget::reset(const shared_ptr<IUsbGadgetCallback> &callback,
        int64_t in_transactionId) {
    ALOGI("USB Gadget reset");

    if (!WriteStringToFile("none", PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled down");
        if (callback)
            callback->resetCb(Status::ERROR, in_transactionId);
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Gadget cannot be pulled down");
    }

    usleep(kDisconnectWaitUs);

    if (!WriteStringToFile(kGadgetName, PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled up");
        if (callback)
            callback->resetCb(Status::ERROR, in_transactionId);
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Gadget cannot be pulled up");
    }
    if (callback)
        callback->resetCb(Status::SUCCESS, in_transactionId);

    return ScopedAStatus::ok();
}

Status UsbGadget::setupFunctions(long functions,
        const shared_ptr<IUsbGadgetCallback> &callback, uint64_t timeout,
        int64_t in_transactionId) {
    bool ffsEnabled = false;
    int i = 0;

    if (Status(addGenericAndroidFunctions(&monitorFfs, functions, &ffsEnabled, &i)) !=
        Status::SUCCESS)
        return Status::ERROR;

    if ((functions & GadgetFunction::ADB) != 0) {
        ffsEnabled = true;
        if (Status(addAdb(&monitorFfs, &i)) != Status::SUCCESS)
            return Status::ERROR;
    }

    if ((functions & GadgetFunction::NCM) != 0) {
        ALOGI("setCurrentUsbFunctions ncm");
        if (linkFunction("ncm.gs9", i++))
            return Status::ERROR;
    }

    // Pull up the gadget right away when there are no ffs functions.
    if (!ffsEnabled) {
        if (!WriteStringToFile(kGadgetName, PULLUP_PATH))
            return Status::ERROR;
        mCurrentUsbFunctionsApplied = true;
        if (callback)
            callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS, in_transactionId);
        return Status::SUCCESS;
    }

    monitorFfs.registerFunctionsAppliedCallback(&currentFunctionsAppliedCallback, this);
    // Monitors the ffs paths to pull up the gadget when descriptors are written.
    // Also takes of the pulling up the gadget again if the userspace process
    // dies and restarts.
    monitorFfs.startMonitor();

    if (kDebug)
        ALOGI("Mainthread in Cv");

    if (callback) {
        bool pullup = monitorFfs.waitForPullUp(timeout);
        ScopedAStatus ret = callback->setCurrentUsbFunctionsCb(
            functions, pullup ? Status::SUCCESS : Status::ERROR, in_transactionId);
        if (!ret.isOk()) {
            ALOGE("setCurrentUsbFunctionsCb error %s", ret.getDescription().c_str());
            return Status::ERROR;
        }
    }
    return Status::SUCCESS;
}


ScopedAStatus UsbGadget::setCurrentUsbFunctions(long functions,
                                               const shared_ptr<IUsbGadgetCallback> &callback,
                                               int64_t timeout,
                                               int64_t in_transactionId) {
    std::unique_lock<std::mutex> lk(mLockSetCurrentFunction);
    std::string current_usb_power_operation_mode, current_usb_type;
    std::string usb_limit_sink_enable;

    mCurrentUsbFunctions = functions;
    mCurrentUsbFunctionsApplied = false;

    // Unlink the gadget and stop the monitor if running.
    Status status = tearDownGadget();
    if (status != Status::SUCCESS) {
        goto error;
    }

    ALOGI("Returned from tearDown gadget");

    // Leave the gadget pulled down to give time for the host to sense disconnect.
    usleep(kDisconnectWaitUs);

    if (functions == GadgetFunction::NONE) {
        if (callback == NULL)
            return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "callback == NULL");
        ScopedAStatus ret = callback->setCurrentUsbFunctionsCb(functions, status, in_transactionId);
        if (!ret.isOk())
            ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.getDescription().c_str());
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Error while calling setCurrentUsbFunctionsCb");
    }

    status = validateAndSetVidPid(functions);

    if (status != Status::SUCCESS) {
        goto error;
    }

    status = setupFunctions(functions, callback, timeout, in_transactionId);
    if (status != Status::SUCCESS) {
        goto error;
    }

    ALOGI("Usb Gadget setcurrent functions called successfully");
    return ScopedAStatus::ok();

error:
    ALOGI("Usb Gadget setcurrent functions failed");
    if (callback == NULL)
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Usb Gadget setcurrent functions failed");
    ScopedAStatus ret = callback->setCurrentUsbFunctionsCb(functions, status, in_transactionId);
    if (!ret.isOk())
        ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.getDescription().c_str());
    return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Error while calling setCurrentUsbFunctionsCb");
}
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
}  // aidl
