/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "health-impl/SehHealth.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android/binder_ibinder.h>
#include <utils/Errors.h>

#include <health-impl/SehLinkedCallback.h>

using ::aidl::android::hardware::health::HealthInfo;

namespace aidl::vendor::samsung::hardware::health {

namespace {
void OnCallbackDiedWrapped(void* cookie) {
    LOG(ERROR) << "OnCallbackDiedWrapped";
    SehLinkedCallback* linked = reinterpret_cast<SehLinkedCallback*>(cookie);
    linked->OnCallbackDied();
}

void onCallbackUnlinked(void* cookie) {
    SehLinkedCallback* linked = reinterpret_cast<SehLinkedCallback*>(cookie);
    delete linked;
}

bool ReadSysfsInt(const std::string& path, int32_t* out) {
    std::string content;
    if (!::android::base::ReadFileToString(path, &content)) {
        return false;
    }
    return ::android::base::ParseInt(::android::base::Trim(content), out);
}
}  // namespace

SehHealth::SehHealth(std::string_view instance_name,
                     std::shared_ptr<::aidl::android::hardware::health::IHealth> health)
    : instance_name_(instance_name),
      health_(std::move(health)),
      death_recipient_(AIBinder_DeathRecipient_new(&OnCallbackDiedWrapped)) {
    AIBinder_DeathRecipient_setOnUnlinked(death_recipient_.get(), onCallbackUnlinked);
}

SehHealth::~SehHealth() {}

//
// Getters.
//

void SehHealth::FillSehExtras(SehHealthInfo* out) {
    // batteryCurrentNow mirrors the AOSP current reading; fall back to the raw sysfs node.
    if (out->aospHealthInfo.batteryCurrentMicroamps != 0) {
        out->batteryCurrentNow = out->aospHealthInfo.batteryCurrentMicroamps;
    } else {
        ReadSysfsInt("/sys/class/power_supply/battery/current_now", &out->batteryCurrentNow);
    }

    // Workaround: These paths must be handled in libbatterymonitor
    // but it is currently not possible for us.
    ReadSysfsInt("/sys/class/power_supply/battery/online", &out->batteryOnline);
    ReadSysfsInt("/sys/class/power_supply/battery/charge_type", &out->batteryChargeType);
    ReadSysfsInt("/sys/class/power_supply/battery/batt_current_event", &out->batteryCurrentEvent);

    int32_t online = 0;
    if (ReadSysfsInt("/sys/class/power_supply/otg/online", &online)) {
        out->chargerOtgOnline = online != 0;
    }
    if (ReadSysfsInt("/sys/class/power_supply/pogo/online", &online)) {
        out->chargerPogoOnline = online != 0;
    }

    // TODO: device-specific sysfs nodes to verify per model?
    //   batteryPowerSharingOnline, batteryHighVoltageCharger, batteryEvent,
    //   wirelessPowerSharingTxEvent
}

ndk::ScopedAStatus SehHealth::getSehHealthInfo(SehHealthInfo* out) {
    *out = {};
    if (auto res = health_->getHealthInfo(&out->aospHealthInfo); !res.isOk()) {
        return res;
    }
    FillSehExtras(out);
    return ndk::ScopedAStatus::ok();
}

binder_status_t SehHealth::dump(int fd, const char**, uint32_t) {
    ::android::base::WriteStringToFd("\ngetSehHealthInfo -> ", fd);
    SehHealthInfo info;
    auto res = getSehHealthInfo(&info);
    if (res.isOk()) {
        ::android::base::WriteStringToFd(info.toString(), fd);
    } else {
        ::android::base::WriteStringToFd(res.getDescription(), fd);
    }
    ::android::base::WriteStringToFd("\n", fd);

    fsync(fd);
    return STATUS_OK;
}

//
// Charging param persistence.
//

ndk::ScopedAStatus SehHealth::sehWriteEnableToParam(int32_t in_offset, bool in_enable) {
    LOG(INFO) << "param offset: " << in_offset << ", enable: " << in_enable;

    int fd = open("/dev/block/param", O_RDWR);
    if (fd < 0) {
        LOG(ERROR) << "/dev/block/param can not open";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    int written;
    if (in_offset == -2) {
        int32_t paramOffset = 0;
        ::android::base::ParseInt(::android::base::GetProperty("ro.boot.wc.param.offset", ""),
                                  &paramOffset);

        int32_t icInfo = -1;
        if (!ReadSysfsInt("/sys/class/power_supply/battery/wc_param_info", &icInfo)) {
            icInfo = -1;
        }
        LOG(INFO) << "icInfo :" << icInfo << ", offset : " << paramOffset;

        if (lseek(fd, paramOffset, SEEK_SET) == -1) {
            LOG(ERROR) << "offset error";
            fsync(fd);
            close(fd);
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        written = write(fd, &icInfo, sizeof(icInfo));
    } else {
        if (lseek(fd, in_offset, SEEK_SET) == -1) {
            LOG(ERROR) << "offset error";
            fsync(fd);
            close(fd);
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        }
        const char* value = in_enable ? "1" : "0";
        written = write(fd, value, strlen(value));
    }

    if (written < 1) {
        LOG(ERROR) << "can not write enable";
    }
    LOG(INFO) << "error size: " << written;

    fsync(fd);
    close(fd);

    return written > 0 ? ndk::ScopedAStatus::ok()
                       : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

//
// Methods that handle callbacks.
//

ndk::ScopedAStatus SehHealth::registerCallback(
        const std::shared_ptr<ISehHealthInfoCallback>& callback) {
    if (callback == nullptr) {
        // For now, this shouldn't happen because argument is not nullable.
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    {
        std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);
        auto linked_callback_result = SehLinkedCallback::Make(ref<SehHealth>(), callback);
        if (!linked_callback_result.ok()) {
            return ndk::ScopedAStatus::fromStatus(-linked_callback_result.error().code());
        }
        callbacks_[*linked_callback_result] = callback;
        // unlock
    }

    SehHealthInfo info;
    if (auto res = getSehHealthInfo(&info); !res.isOk()) {
        LOG(WARNING) << "Cannot call getSehHealthInfo: " << res.getDescription();
        // No health info to send, so return early.
        return ndk::ScopedAStatus::ok();
    }

    auto res = callback->healthInfoChanged(info);
    if (!res.isOk()) {
        LOG(DEBUG) << "Cannot call healthInfoChanged:" << res.getDescription()
                   << ". Do nothing here if callback is dead as it will be cleaned up later.";
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehHealth::unregisterCallback(
        const std::shared_ptr<ISehHealthInfoCallback>& callback) {
    if (callback == nullptr) {
        // For now, this shouldn't happen because argument is not nullable.
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);

    bool removed = false;
    for (auto it = callbacks_.begin(); it != callbacks_.end();) {
        if (it->second->asBinder() == callback->asBinder()) {
            auto status = AIBinder_unlinkToDeath(callback->asBinder().get(), death_recipient_.get(),
                                                 reinterpret_cast<void*>(it->first));
            if (status != STATUS_OK && status != STATUS_DEAD_OBJECT) {
                LOG(WARNING) << __func__
                             << "Cannot unlink to death: " << ::android::statusToString(status);
            }
            it = callbacks_.erase(it);
            removed = true;
        } else {
            it++;
        }
    }
    return removed ? ndk::ScopedAStatus::ok()
                   : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}

ndk::ScopedAStatus SehHealth::update() {
    LOG(DEBUG) << "[api] update";
    SehHealthInfo info;
    if (auto res = getSehHealthInfo(&info); !res.isOk()) {
        LOG(DEBUG) << "Cannot call getSehHealthInfo for update(): " << res.getDescription();
        return res;
    }
    OnHealthInfoChanged(info);
    return ndk::ScopedAStatus::ok();
}

void SehHealth::OnHealthInfoChanged(const HealthInfo& health_info) {
    SehHealthInfo info;
    info.aospHealthInfo = health_info;
    FillSehExtras(&info);
    OnHealthInfoChanged(info);
}

void SehHealth::OnHealthInfoChanged(const SehHealthInfo& info) {
    std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);
    for (const auto& [_, callback] : callbacks_) {
        auto res = callback->healthInfoChanged(info);
        if (!res.isOk()) {
            LOG(DEBUG) << "Cannot call healthInfoChanged:" << res.getDescription()
                       << ". Do nothing here if callback is dead as it will be cleaned up later.";
        }
    }
}

}  // namespace aidl::vendor::samsung::hardware::health
