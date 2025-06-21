/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <thread>
#include <chrono>
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sstream>

#include "SecUdfpsHelper.h"

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

SecUdfpsHelper::SecUdfpsHelper(std::shared_ptr<ISehSysInputDev> sehInput)
    : mSehInput(std::move(sehInput)) {
#if IS_QCOM_HBM
    mHbmWatcher.start();
#endif
}

template <typename T>
void SecUdfpsHelper::set(const std::string& path, const T& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG(ERROR) << UDFPS_TAG << "Failed to open file at path: " << path;
        return;
    }

    file << value << std::endl;
    if (file.fail()) {
        LOG(ERROR) << UDFPS_TAG << "Failed to write value: " << value << " to path: " << path;
    }
}

template <typename T>
T SecUdfpsHelper::get(const std::string& path, const T& defaultValue) {
    std::ifstream file(path);
    T result = defaultValue;

    if (file.is_open()) {
        file >> result;
        file.close();
    } else {
        LOG(ERROR) << UDFPS_TAG << "Failed to open file at path: " << path;
    }

    return result;
}

#if USE_SYSINPUT_HAL
void SecUdfpsHelper::runCommand(int inputDevice, const std::string& cmdString) {
    if (!mSehInput) {
        LOG(ERROR) << UDFPS_TAG << "SysInput HAL isn't registered to Fingerprint HAL";
        return;
    }

    aidl::vendor::samsung::hardware::sysinput::SehIntStringParcel sehParcel;
    ndk::ScopedAStatus status = mSehInput->runCommand(inputDevice, cmdString, &sehParcel);

    if (!status.isOk()) {
        LOG(ERROR) << UDFPS_TAG << "runCommand failed: " << status.getMessage();
    } else {
        LOG(INFO) << UDFPS_TAG << "runCommand outbuf=" << sehParcel.outbuf;
    }
}

int SecUdfpsHelper::setProperty(int inputDevice, int inputCommand, const std::string& cmdString) {
    if (!mSehInput) {
        LOG(ERROR) << UDFPS_TAG << "SysInput HAL isn't registered to Fingerprint HAL";
        return -1;
    }

    int32_t result = -1;
    auto status = mSehInput->setProperty(inputDevice, inputCommand, cmdString, &result);
    if (!status.isOk()) {
        LOG(ERROR) << UDFPS_TAG << "SysInput::setProperty failed: " << status.getMessage();
        return -1;
    }

    return result;
}

std::string SecUdfpsHelper::getProperty(int inputDevice, int inputCommand) {
    if (!mSehInput) {
        LOG(ERROR) << UDFPS_TAG << "SysInput HAL isn't registered to Fingerprint HAL";
        return "";
    }

    std::string result;
    auto status = mSehInput->getProperty(inputDevice, inputCommand, &result);
    if (!status.isOk()) {
        LOG(ERROR) << UDFPS_TAG << "SysInput::getProperty failed: " << status.getMessage();
        return "";
    }

    return result;
}

void SecUdfpsHelper::runCmd(const std::string& cmdString)
{
    runCommand(static_cast<int>(SemInputDevice::DEFAULT_TSP), cmdString);
}

int SecUdfpsHelper::setFodEnable(int i, int i2, int i3) {
    std::ostringstream oss;
    if (i == 1) {
        oss << i << "," << i2 << "," << i3;
    } else {
        oss << i;
    }
    return setProperty(static_cast<int>(SemInputDevice::DEFAULT_TSP), static_cast<int>(SemInputCommand::FOD), oss.str());
}

int SecUdfpsHelper::setFodIconVisible(int i) {
    return setProperty(static_cast<int>(SemInputDevice::DEFAULT_TSP), static_cast<int>(SemInputCommand::FOD_ICON_VISIBLE), std::to_string(i) + "");
}

int SecUdfpsHelper::setFodLpMode(int i) {
    return setProperty(static_cast<int>(SemInputDevice::DEFAULT_TSP), static_cast<int>(SemInputCommand::FOD_LP), std::to_string(i) + "");
}

int SecUdfpsHelper::setFodRect(const std::string& fodRect) {
    return setProperty(static_cast<int>(SemInputDevice::DEFAULT_TSP), static_cast<int>(SemInputCommand::FOD_RECT), fodRect);
}

// Currently, calculating FodRect is complicated. Request original coordinates from stock OneUI temporarily as a FP HAL property.
/*
int SecUdfpsHelper::setFodRect(int i, int i2, int i3, int i4) {
    std::ostringstream oss;
    oss << i << "," << i2 << "," << i3 << "," << i4;
    return setProperty(static_cast<int>(SemInputDevice::DEFAULT_TSP), static_cast<int>(SemInputCommand::FOD_RECT), oss.str());
}
*/

#else
void SecUdfpsHelper::setFodEnable(bool enable) {
    if (enable) {
        LOG(INFO) << UDFPS_TAG << "Enabling FOD press";
        set(TSP_CMD, "fod_enable,1,1,0");
    } else {
        LOG(INFO) << UDFPS_TAG << "Disabling FOD press";
        set(TSP_CMD, "fod_enable,0,0,0");
    }
}

void SecUdfpsHelper::setFodRect(const std::string& fodRect) {
    std::ifstream file(TSP_CMD_LIST);
    if (!file.is_open()) {
        LOG(ERROR) << UDFPS_TAG << "Failed to open TSP_CMD_LIST file, skipping setFodRect...";
        return;
    }

    std::string line;
    bool cmd_support = false;

    while (getline(file, line)) {
        if (line == "set_fod_rect") {
            cmd_support = true;

            if (!fodRect.empty()) {
                LOG(INFO) << UDFPS_TAG << "Writing set_fod_rect," << fodRect << " to TSP Sponge";
                set(TSP_CMD, "set_fod_rect," + fodRect);
            } else {
                LOG(INFO) << UDFPS_TAG << "Rectangle FOD location is not defined, skipping setFodRect...";
            }
            return;
        }
    }

    if (!cmd_support) {
        LOG(INFO) << UDFPS_TAG << "set_fod_rect command is not available to TSP, skipping setFodRect...";
    }
}
#endif

#if IS_QCOM_HBM
void SecUdfpsHelper::HbmWatcher::start() {
    std::thread([this]() {
        monitorLoop();
    }).detach();
}

void SecUdfpsHelper::HbmWatcher::monitorLoop() {
    const std::string enabledPath = SecUdfpsHelper::FP_MASK_ENABLED;
    const std::string brightReadPath = SecUdfpsHelper::FP_MASK_DEFAULT_BRIGHT;
    const std::string brightWritePath = SecUdfpsHelper::HBM_BRIGHT_WO;
    const int defBrightness = SecUdfpsHelper::DEFAULT_MASK_BRIGHTNESS;
    std::string lastValue;

    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        LOG(ERROR) << UDFPS_TAG << "Failed to initialize HbmWatcher";
        return;
    }

    int wd = inotify_add_watch(fd, enabledPath.c_str(), IN_MODIFY);
    if (wd < 0) {
        LOG(ERROR) << UDFPS_TAG << "Failed to register HbmWatcher";
        close(fd);
        return;
    }

    LOG(INFO) << UDFPS_TAG << "Started monitoring " << enabledPath;

    char buffer[4096];
    struct pollfd pfd = { .fd = fd, .events = POLLIN };

    while (true) {
        int res = poll(&pfd, 1, -1);
        if (res > 0 && (pfd.revents & POLLIN)) {
            ssize_t len = read(fd, buffer, sizeof(buffer));
            if (len <= 0) continue;

            std::ifstream in(enabledPath);
            std::string value;
            in >> value;
            in.close();

            if (value == "1" && lastValue != "1") {
                std::string brightness;
                std::ifstream brightIn(brightReadPath);
                if (brightIn.is_open()) {
                    brightIn >> brightness;
                    brightIn.close();
                } else {
                    brightness = std::to_string(defBrightness);
                    LOG(WARNING) << UDFPS_TAG << "Kernel does not provide an HBM brightness level. Using default brightness: " << brightness;
                }

                std::ofstream brightOut(brightWritePath);
                if (brightOut.is_open()) {
                    brightOut << brightness << std::endl;
                    brightOut.close();
                    LOG(INFO) << UDFPS_TAG << "Set HBM brightness to: " << brightness;
                } else {
                    LOG(ERROR) << UDFPS_TAG << "Failed to open write path: " << brightWritePath;
                }
            }

            lastValue = value;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}
#endif

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
