/*
 * Copyright (C) 2022-2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ChargingControl.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <fstream>

#define LOG_TAG "vendor.lineage.health-service.samsung.default"

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
static const std::vector<ChargingEnabledNode> kChargingEnabledNodes = {
#ifdef HEALTH_CHARGING_CONTROL_CHARGING_PATH
        {HEALTH_CHARGING_CONTROL_CHARGING_PATH,
         HEALTH_CHARGING_CONTROL_CHARGING_ENABLED,
         HEALTH_CHARGING_CONTROL_CHARGING_DISABLED,
         {}},
#else
        {"/sys/class/power_supply/battery/battery_charging_enabled", "1", "0",
         static_cast<int>(ChargingControlSupportedMode::TOGGLE) |
                 static_cast<int>(ChargingControlSupportedMode::BYPASS)},
        {"/sys/class/power_supply/battery/charging_enabled", "1", "0",
         static_cast<int>(ChargingControlSupportedMode::TOGGLE) |
                 static_cast<int>(ChargingControlSupportedMode::BYPASS)},
        {"/sys/class/power_supply/battery/input_suspend", "0", "1",
         static_cast<int>(ChargingControlSupportedMode::TOGGLE)},
        {"/sys/class/qcom-battery/input_suspend", "0", "1",
         static_cast<int>(ChargingControlSupportedMode::TOGGLE)},
#endif
};
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_DEADLINE
static const std::vector<std::string> kChargingDeadlineNodes = {
#ifdef HEALTH_CHARGING_CONTROL_DEADLINE_PATH
        HEALTH_CHARGING_CONTROL_DEADLINE_PATH,
#else
        "/sys/class/power_supply/battery/charge_deadline",
#endif
};
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_LIMIT
static const std::vector<ChargingLimitNode> kChargingLimitNodes = {
#if defined(HEALTH_CHARGING_CONTROL_LIMIT_START_PATH) && defined(HEALTH_CHARGING_CONTROL_LIMIT_STOP_PATH)
        {HEALTH_CHARGING_CONTROL_LIMIT_START_PATH,
         HEALTH_CHARGING_CONTROL_LIMIT_STOP_PATH},
#else
        {"/sys/devices/platform/google,charger/charge_start_level",
         "/sys/devices/platform/google,charger/charge_stop_level"},
#endif
};
#endif

#define OPEN_RETRY_COUNT 10

ChargingControl::ChargingControl()
    : mChargingEnabledNode(nullptr), mChargingDeadlineNode(nullptr), mChargingLimitNode(nullptr) {
#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
#ifdef HEALTH_CHARGING_CONTROL_CHARGING_PATH
    mChargingEnabledNode = &kChargingEnabledNodes[0];
#else
    while (!mChargingEnabledNode) {
        for (const auto& node : kChargingEnabledNodes) {
            for (int retries = 0; retries < OPEN_RETRY_COUNT; retries++) {
                if (access(node.path.c_str(), R_OK | W_OK) == 0) {
                    mChargingEnabledNode = &node;
                    break;
                }
                PLOG(WARNING) << "Failed to access() file " << node.path;
                usleep(10000);
            }
            if (mChargingEnabledNode) {
                break;
            }
        }
    }
#endif
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_DEADLINE
#ifdef HEALTH_CHARGING_CONTROL_DEADLINE_PATH
    mChargingEnabledNode = &kChargingDeadlineNodes[0];
#else
    while (!mChargingDeadlineNode) {
        for (const auto& node : kChargingDeadlineNodes) {
            for (int retries = 0; retries < OPEN_RETRY_COUNT; retries++) {
                if (access(node.c_str(), R_OK | W_OK) == 0) {
                    mChargingDeadlineNode = &node;
                    break;
                }
                PLOG(WARNING) << "Failed to access() file " << node;
                usleep(10000);
            }
            if (mChargingDeadlineNode) {
                break;
            }
        }
    }
#endif
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_LIMIT
#if defined(HEALTH_CHARGING_CONTROL_LIMIT_START_PATH) && defined(HEALTH_CHARGING_CONTROL_LIMIT_STOP_PATH)
    mChargingLimitNode = &kChargingLimitNodes[0];
#else
    while (!mChargingLimitNode) {
        for (const auto& node : kChargingLimitNodes) {
            for (int retries = 0; retries < OPEN_RETRY_COUNT; retries++) {
                if (access(node.min_path.c_str(), R_OK | W_OK) == 0 &&
                    access(node.max_path.c_str(), R_OK | W_OK) == 0) {
                    mChargingLimitNode = &node;
                    break;
                }
                PLOG(WARNING) << "Failed to access() file " << node.min_path << " or "
                              << node.max_path;
                usleep(10000);
            }
            if (mChargingLimitNode) {
                break;
            }
        }
    }
#endif
#endif
}

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
ndk::ScopedAStatus ChargingControl::getChargingEnabled(bool* _aidl_return) {
    std::string content;
    if (!android::base::ReadFileToString(mChargingEnabledNode->path, &content, true)) {
        LOG(ERROR) << "Failed to read current charging enabled value";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    content = android::base::Trim(content);

    if (content == mChargingEnabledNode->value_true) {
        *_aidl_return = true;
    } else if (content == mChargingEnabledNode->value_false) {
        *_aidl_return = false;
    } else {
        LOG(ERROR) << "Unknown value " << content;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ChargingControl::setChargingEnabled(bool enabled) {
    const auto& value =
            enabled ? mChargingEnabledNode->value_true : mChargingEnabledNode->value_false;
    if (!android::base::WriteStringToFile(value, mChargingEnabledNode->path, true)) {
        LOG(ERROR) << "Failed to write to charging enable node: " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}
#else
ndk::ScopedAStatus ChargingControl::getChargingEnabled(bool* /* _aidl_return */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ChargingControl::setChargingEnabled(bool /* enabled */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_DEADLINE
ndk::ScopedAStatus ChargingControl::getChargingDeadline(int64_t* _aidl_return) {
    std::string content;
    if (!android::base::ReadFileToString(*mChargingDeadlineNode, &content, true)) {
        LOG(ERROR) << "Failed to read current charging deadline value";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    content = android::base::Trim(content);

    *_aidl_return = std::stoll(content);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ChargingControl::setChargingDeadline(int64_t deadline) {
    std::string content = std::to_string(deadline);
    if (!android::base::WriteStringToFile(content, *mChargingDeadlineNode, true)) {
        LOG(ERROR) << "Failed to write to charging deadline node: " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}
#else
ndk::ScopedAStatus ChargingControl::getChargingDeadline(int64_t* /* deadline */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ChargingControl::setChargingDeadline(int64_t /* deadline */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_LIMIT
ndk::ScopedAStatus ChargingControl::getChargingLimit(ChargingLimitInfo* _aidl_return) {
    std::string content;
    if (!android::base::ReadFileToString(mChargingLimitNode->min_path, &content, true)) {
        LOG(ERROR) << "Failed to read current charging limit min value";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    _aidl_return->min = std::stoll(content);

    if (!android::base::ReadFileToString(mChargingLimitNode->max_path, &content, true)) {
        LOG(ERROR) << "Failed to read current charging limit max value";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    _aidl_return->max = std::stoll(content);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ChargingControl::setChargingLimit(const ChargingLimitInfo& limit) {
    if (!android::base::WriteStringToFile(std::to_string(limit.max), mChargingLimitNode->max_path,
                                          true)) {
        LOG(ERROR) << "Failed to write to charging limit max node: " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (!android::base::WriteStringToFile(std::to_string(limit.min), mChargingLimitNode->min_path,
                                          true)) {
        LOG(ERROR) << "Failed to write to charging limit min node: " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}
#else
ndk::ScopedAStatus ChargingControl::getChargingLimit(ChargingLimitInfo* /* _aidl_return */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus ChargingControl::setChargingLimit(const ChargingLimitInfo& /* limit */) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}
#endif

ndk::ScopedAStatus ChargingControl::getSupportedMode(int* _aidl_return) {
    int mode = 0;

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
    mode |= static_cast<int>(ChargingControlSupportedMode::TOGGLE);
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_BYPASS
    mode |= static_cast<int>(ChargingControlSupportedMode::BYPASS);
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_DEADLINE
    mode |= static_cast<int>(ChargingControlSupportedMode::DEADLINE);
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_LIMIT
    mode |= static_cast<int>(ChargingControlSupportedMode::LIMIT);
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
    *_aidl_return = mChargingEnabledNode->supported_mode.value_or(mode);
#else
    *_aidl_return = mode;
#endif

    return ndk::ScopedAStatus::ok();
}

binder_status_t ChargingControl::dump(int fd, const char** /* args */, uint32_t /* numArgs */) {
    int supportedMode;
    getSupportedMode(&supportedMode);

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_TOGGLE
    bool isChargingEnabled;
    getChargingEnabled(&isChargingEnabled);
    dprintf(fd, "Charging control node selected: %s\n", mChargingEnabledNode->path.c_str());
    dprintf(fd, "Charging enabled: %s\n", isChargingEnabled ? "true" : "false");
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_DEADLINE
    dprintf(fd, "Charging deadline node selected: %s\n", mChargingDeadlineNode->c_str());
#endif

#ifdef HEALTH_CHARGING_CONTROL_SUPPORTS_LIMIT
    dprintf(fd, "Charging limit node selected: %s %s\n", mChargingLimitNode->min_path.c_str(),
            mChargingLimitNode->max_path.c_str());
#endif

    dprintf(fd, "Charging control supported mode: %d\n", supportedMode);

    return STATUS_OK;
}

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
