/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/sysinput/ISehSysInputDev.h>
#include <aidl/vendor/samsung/hardware/sysinput/SehIntStringParcel.h>
#include <android/binder_manager.h>
#include <android/binder_interface_utils.h>
#include <android-base/logging.h>
#include <memory>

#include "SemInputConstants.h"

#define UDFPS_TAG "[SecUdfpsHelper]: "

inline std::shared_ptr<aidl::vendor::samsung::hardware::sysinput::ISehSysInputDev>
getSehSysInputDev(const std::string& instanceName = "default") {
    using aidl::vendor::samsung::hardware::sysinput::ISehSysInputDev;

#if USE_SYSINPUT_HAL
    const std::string sehInstance = std::string() + ISehSysInputDev::descriptor + "/" + instanceName;

    ndk::SpAIBinder binder(AServiceManager_waitForService(sehInstance.c_str()));
    if (!binder.get()) {
        LOG(ERROR) << "Failed to get ISehSysInputDev service: " << sehInstance;
        return nullptr;
    }

    std::shared_ptr<ISehSysInputDev> dev = ISehSysInputDev::fromBinder(binder);
    if (!dev) {
        LOG(ERROR) << "Failed to register ISehSysInputDev interface";
    } 

    return dev;
#else
    LOG(ERROR) << "Fingerprint HAL doesn't use SysInput Interface";
    return nullptr;
#endif
}

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

using ::aidl::vendor::samsung::hardware::sysinput::ISehSysInputDev;

class SecUdfpsHelper {
public:
    explicit SecUdfpsHelper(std::shared_ptr<ISehSysInputDev> sehInput);

#if USE_SYSINPUT_HAL
    void runCmd(const std::string& cmdString);
    int setFodEnable(int i, int i2 = 0, int i3 = 0);
    int setFodIconVisible(int i);
    int setFodLpMode(int i);
    int setFodRect(const std::string& fodRect);
#else
    void setFodEnable(bool enable);
    void setFodRect(const std::string& fodRect);
#endif

    template <typename T> T get(const std::string& path, const T& def);
    template <typename T> void set(const std::string& path, const T& value);

private:
    std::shared_ptr<ISehSysInputDev> mSehInput;

#if USE_SYSINPUT_HAL
    void runCommand(int inputDevice, const std::string& cmdString);
    int setProperty(int inputDevice, int inputCommand, const std::string& cmdString);
    std::string getProperty(int inputDevice, int inputCommand);
#else
    static constexpr const char* TSP_CMD = "/sys/class/sec/tsp/cmd";
    static constexpr const char* TSP_CMD_LIST = "/sys/class/sec/tsp/cmd_list";
#endif

#if IS_QCOM_HBM
    class HbmWatcher {
        public:
            void start();
        private:
            void monitorLoop();
    };
    HbmWatcher mHbmWatcher;

/*
 * finger_mask_state -> Provides info about HBM status through DRM property
 * finger_mask_brightness -> Provides default brightness value used by HBM to FP HAL
 * actual_mask_brightness -> Current brightness value of the HBM
 * mask_brightness -> Sets brightness of the HBM
 * To-Do: fp_green_circle support?
*/
    static constexpr const char* FP_MASK_ENABLED = "/sys/devices/virtual/lcd/panel/finger_mask_state";
    static constexpr const char* FP_MASK_DEFAULT_BRIGHT = "/sys/devices/virtual/lcd/panel/finger_mask_brightness";
    static constexpr const char* HBM_BRIGHT_RO = "/sys/devices/virtual/lcd/panel/actual_mask_brightness";
    static constexpr const char* HBM_BRIGHT_WO = "/sys/devices/virtual/lcd/panel/mask_brightness";
    static constexpr int DEFAULT_MASK_BRIGHTNESS = 319;
#endif

};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
