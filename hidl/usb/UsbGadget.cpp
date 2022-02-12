/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.usb.gadget@1.2-service.gs101"

#include "UsbGadget.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_2 {
namespace implementation {

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

Return<void> UsbGadget::getCurrentUsbFunctions(const sp<V1_0::IUsbGadgetCallback> &callback) {
    Return<void> ret = callback->getCurrentUsbFunctionsCb(
        mCurrentUsbFunctions,
        mCurrentUsbFunctionsApplied ? Status::FUNCTIONS_APPLIED : Status::FUNCTIONS_NOT_APPLIED);
    if (!ret.isOk())
        ALOGE("Call to getCurrentUsbFunctionsCb failed %s", ret.description().c_str());

    return Void();
}

Return<void> UsbGadget::getUsbSpeed(const sp<V1_2::IUsbGadgetCallback> &callback) {
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
            mUsbSpeed = UsbSpeed::RESERVED_SPEED;
    } else {
        ALOGE("Fail to read current speed");
        mUsbSpeed = UsbSpeed::UNKNOWN;
    }

    if (callback) {
        Return<void> ret = callback->getUsbSpeedCb(mUsbSpeed);

        if (!ret.isOk())
            ALOGE("Call to getUsbSpeedCb failed %s", ret.description().c_str());
    }

    return Void();
}

V1_0::Status UsbGadget::tearDownGadget() {
    if (resetGadget() != Status::SUCCESS)
        return Status::ERROR;

    if (monitorFfs.isMonitorRunning()) {
        monitorFfs.reset();
    } else {
        ALOGI("mMonitor not running");
    }
    return Status::SUCCESS;
}

static V1_0::Status validateAndSetVidPid(uint64_t functions) {
    V1_0::Status ret = Status::SUCCESS;
    std::string vendorFunctions = getVendorFunctions();

    switch (functions) {
        case static_cast<uint64_t>(GadgetFunction::MTP):
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee1");
            }
            break;
        case GadgetFunction::ADB | GadgetFunction::MTP:
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee2");
            }
            break;
        case static_cast<uint64_t>(GadgetFunction::RNDIS):
        case GadgetFunction::RNDIS | GadgetFunction::NCM:
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee3");
            }
            break;
        case GadgetFunction::ADB | GadgetFunction::RNDIS:
        case GadgetFunction::ADB | GadgetFunction::RNDIS | GadgetFunction::NCM:
            if (vendorFunctions == "dm") {
                ret = setVidPid("0x04e8", "0x6862");
            } else {
                if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                    ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                    ret = Status::CONFIGURATION_NOT_SUPPORTED;
                } else {
                    ret = setVidPid("0x18d1", "0x4ee4");
                }
            }
            break;
        case static_cast<uint64_t>(GadgetFunction::PTP):
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee5");
            }
            break;
        case GadgetFunction::ADB | GadgetFunction::PTP:
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee6");
            }
            break;
        case static_cast<uint64_t>(GadgetFunction::ADB):
            if (vendorFunctions == "dm") {
                ret = setVidPid("0x04e8", "0x6862");
            } else if (vendorFunctions == "etr_miu") {
                ret = setVidPid("0x18d1", "0x4ee2");
            } else if (vendorFunctions == "uwb_acm"){
                ret = setVidPid("0x18d1", "0x4ee2");
            } else {
                if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                    ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                    ret = Status::CONFIGURATION_NOT_SUPPORTED;
                } else {
                    ret = setVidPid("0x18d1", "0x4ee7");
                }
            }
            break;
        case static_cast<uint64_t>(GadgetFunction::MIDI):
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee8");
            }
            break;
        case GadgetFunction::ADB | GadgetFunction::MIDI:
            if (!(vendorFunctions == "user" || vendorFunctions == "")) {
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
                ret = Status::CONFIGURATION_NOT_SUPPORTED;
            } else {
                ret = setVidPid("0x18d1", "0x4ee9");
            }
            break;
        case static_cast<uint64_t>(GadgetFunction::ACCESSORY):
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d00");
            break;
        case GadgetFunction::ADB | GadgetFunction::ACCESSORY:
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d01");
            break;
        case static_cast<uint64_t>(GadgetFunction::AUDIO_SOURCE):
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d02");
            break;
        case GadgetFunction::ADB | GadgetFunction::AUDIO_SOURCE:
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d03");
            break;
        case GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d04");
            break;
        case GadgetFunction::ADB | GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x2d05");
            break;
        case static_cast<uint64_t>(GadgetFunction::NCM):
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x4eeb");
            break;
        case GadgetFunction::ADB | GadgetFunction::NCM:
            if (!(vendorFunctions == "user" || vendorFunctions == ""))
                ALOGE("Invalid vendorFunctions set: %s", vendorFunctions.c_str());
            ret = setVidPid("0x18d1", "0x4eec");
            break;
        default:
            ALOGE("Combination not supported");
            ret = Status::CONFIGURATION_NOT_SUPPORTED;
    }
    return ret;
}

Return<Status> UsbGadget::reset() {
    ALOGI("USB Gadget reset");

    if (!WriteStringToFile("none", PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled down");
        return Status::ERROR;
    }

    usleep(kDisconnectWaitUs);

    if (!WriteStringToFile(kGadgetName, PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled up");
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

V1_0::Status UsbGadget::setupFunctions(uint64_t functions,
                                       const sp<V1_0::IUsbGadgetCallback> &callback,
                                       uint64_t timeout) {
    bool ffsEnabled = false;
    int i = 0;

    if (addGenericAndroidFunctions(&monitorFfs, functions, &ffsEnabled, &i) !=
        Status::SUCCESS)
        return Status::ERROR;

    std::string vendorFunctions = getVendorFunctions();

    if (vendorFunctions == "dm") {
        ALOGI("enable usbradio debug functions");
        if ((functions & GadgetFunction::RNDIS) != 0) {
            if (linkFunction("acm.gs6", i++))
	        return Status::ERROR;
            if (linkFunction("dm.gs7", i++))
                return Status::ERROR;
        } else {
            if (linkFunction("dm.gs7", i++))
                return Status::ERROR;
            if (linkFunction("acm.gs6", i++))
                return Status::ERROR;
	}
    } else if (vendorFunctions == "etr_miu") {
        ALOGI("enable etr_miu functions");
        if (linkFunction("etr_miu.gs11", i++))
            return Status::ERROR;
    } else if (vendorFunctions == "uwb_acm") {
        ALOGI("enable uwb acm function");
        if (linkFunction("acm.uwb0", i++))
            return Status::ERROR;
    }

    if ((functions & GadgetFunction::ADB) != 0) {
        ffsEnabled = true;
        if (addAdb(&monitorFfs, &i) != Status::SUCCESS)
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
            callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
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
        Return<void> ret = callback->setCurrentUsbFunctionsCb(
            functions, pullup ? Status::SUCCESS : Status::ERROR);
        if (!ret.isOk())
            ALOGE("setCurrentUsbFunctionsCb error %s", ret.description().c_str());
    }

    return Status::SUCCESS;
}

Return<void> UsbGadget::setCurrentUsbFunctions(uint64_t functions,
                                               const sp<V1_0::IUsbGadgetCallback> &callback,
                                               uint64_t timeout) {
    std::unique_lock<std::mutex> lk(mLockSetCurrentFunction);

    mCurrentUsbFunctions = functions;
    mCurrentUsbFunctionsApplied = false;

    // Unlink the gadget and stop the monitor if running.
    V1_0::Status status = tearDownGadget();
    if (status != Status::SUCCESS) {
        goto error;
    }

    ALOGI("Returned from tearDown gadget");

    // Leave the gadget pulled down to give time for the host to sense disconnect.
    usleep(kDisconnectWaitUs);

    if (functions == static_cast<uint64_t>(GadgetFunction::NONE)) {
        if (callback == NULL)
            return Void();
        Return<void> ret = callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS);
        if (!ret.isOk())
            ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.description().c_str());
        return Void();
    }

    status = validateAndSetVidPid(functions);

    if (status != Status::SUCCESS) {
        goto error;
    }

    status = setupFunctions(functions, callback, timeout);
    if (status != Status::SUCCESS) {
        goto error;
    }

    if (functions & GadgetFunction::NCM) {
        SetProperty("vendor.usb.dwc3_irq", "big");
    } else {
        SetProperty("vendor.usb.dwc3_irq", "medium");
    }

    ALOGI("Usb Gadget setcurrent functions called successfully");
    return Void();

error:
    ALOGI("Usb Gadget setcurrent functions failed");
    if (callback == NULL)
        return Void();
    Return<void> ret = callback->setCurrentUsbFunctionsCb(functions, status);
    if (!ret.isOk())
        ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.description().c_str());
    return Void();
}
}  // namespace implementation
}  // namespace V1_2
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
