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

#include "health-impl/Health.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/hardware/health/translate-ndk.h>
#include <health/utils.h>

#include "LinkedCallback.h"
#include "health-convert.h"

using std::string_literals::operator""s;

namespace aidl::android::hardware::health {

namespace {
// Wrap LinkedCallback::OnCallbackDied() into a void(void*).
void OnCallbackDiedWrapped(void* cookie) {
    LinkedCallback* linked = reinterpret_cast<LinkedCallback*>(cookie);
    linked->OnCallbackDied();
}
// Delete the owned cookie.
void onCallbackUnlinked(void* cookie) {
    LinkedCallback* linked = reinterpret_cast<LinkedCallback*>(cookie);
    delete linked;
}
}  // namespace

/*
// If you need to call healthd_board_init, construct the Health instance with
// the healthd_config after calling healthd_board_init:
class MyHealth : public Health {
  protected:
    MyHealth() : Health(CreateConfig()) {}
  private:
    static std::unique_ptr<healthd_config> CreateConfig() {
      auto config = std::make_unique<healthd_config>();
      ::android::hardware::health::InitHealthdConfig(config.get());
      healthd_board_init(config.get());
      return std::move(config);
    }
};
*/
Health::Health(std::string_view instance_name, std::unique_ptr<struct healthd_config>&& config)
    : instance_name_(instance_name),
      healthd_config_(std::move(config)),
      death_recipient_(AIBinder_DeathRecipient_new(&OnCallbackDiedWrapped)) {
    AIBinder_DeathRecipient_setOnUnlinked(death_recipient_.get(), onCallbackUnlinked);
    battery_monitor_.init(healthd_config_.get());
}

Health::~Health() {}

static inline ndk::ScopedAStatus TranslateStatus(::android::status_t err) {
    switch (err) {
        case ::android::OK:
            return ndk::ScopedAStatus::ok();
        case ::android::NAME_NOT_FOUND:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        default:
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    IHealth::STATUS_UNKNOWN, ::android::statusToString(err).c_str());
    }
}

//
// Getters.
//

template <typename T>
static ndk::ScopedAStatus GetProperty(::android::BatteryMonitor* monitor, int id, T defaultValue,
                                      T* out) {
    *out = defaultValue;
    struct ::android::BatteryProperty prop;
    ::android::status_t err = monitor->getProperty(static_cast<int>(id), &prop);
    if (err == ::android::OK) {
        *out = static_cast<T>(prop.valueInt64);
    } else {
        LOG(DEBUG) << "getProperty(" << id << ")"
                   << " fails: (" << err << ") " << ::android::statusToString(err);
    }
    return TranslateStatus(err);
}

ndk::ScopedAStatus Health::getChargeCounterUah(int32_t* out) {
    return GetProperty<int32_t>(&battery_monitor_, ::android::BATTERY_PROP_CHARGE_COUNTER, 0, out);
}

ndk::ScopedAStatus Health::getCurrentNowMicroamps(int32_t* out) {
    return GetProperty<int32_t>(&battery_monitor_, ::android::BATTERY_PROP_CURRENT_NOW, 0, out);
}

ndk::ScopedAStatus Health::getCurrentAverageMicroamps(int32_t* out) {
    return GetProperty<int32_t>(&battery_monitor_, ::android::BATTERY_PROP_CURRENT_AVG, 0, out);
}

ndk::ScopedAStatus Health::getCapacity(int32_t* out) {
    return GetProperty<int32_t>(&battery_monitor_, ::android::BATTERY_PROP_CAPACITY, 0, out);
}

ndk::ScopedAStatus Health::getEnergyCounterNwh(int64_t* out) {
    return GetProperty<int64_t>(&battery_monitor_, ::android::BATTERY_PROP_ENERGY_COUNTER, 0, out);
}

ndk::ScopedAStatus Health::getChargeStatus(BatteryStatus* out) {
    return GetProperty(&battery_monitor_, ::android::BATTERY_PROP_BATTERY_STATUS,
                       BatteryStatus::UNKNOWN, out);
}

ndk::ScopedAStatus Health::setChargingPolicy(BatteryChargingPolicy in_value) {
    in_value = static_cast<BatteryChargingPolicy>(0);
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Health::getChargingPolicy(BatteryChargingPolicy* out) {
    return GetProperty(&battery_monitor_, ::android::BATTERY_PROP_CHARGING_POLICY,
                       BatteryChargingPolicy::DEFAULT, out);
}

ndk::ScopedAStatus Health::getBatteryHealthData(BatteryHealthData* out) {
    if (auto res =
                GetProperty<int64_t>(&battery_monitor_, ::android::BATTERY_PROP_MANUFACTURING_DATE,
                                     0, &out->batteryManufacturingDateSeconds);
        !res.isOk()) {
        LOG(WARNING) << "Cannot get Manufacturing_date: " << res.getDescription();
    }
    if (auto res = GetProperty<int64_t>(&battery_monitor_, ::android::BATTERY_PROP_FIRST_USAGE_DATE,
                                        0, &out->batteryFirstUsageSeconds);
        !res.isOk()) {
        LOG(WARNING) << "Cannot get First_usage_date: " << res.getDescription();
    }
    if (auto res = GetProperty<int64_t>(&battery_monitor_, ::android::BATTERY_PROP_STATE_OF_HEALTH,
                                        0, &out->batteryStateOfHealth);
        !res.isOk()) {
        LOG(WARNING) << "Cannot get Battery_state_of_health: " << res.getDescription();
    }
    if (auto res = battery_monitor_.getSerialNumber(&out->batterySerialNumber);
        res != ::android::OK) {
        LOG(WARNING) << "Cannot get Battery_serial_number: "
                     << TranslateStatus(res).getDescription();
    }

    int64_t part_status = static_cast<int64_t>(BatteryPartStatus::UNSUPPORTED);
    if (auto res = GetProperty<int64_t>(&battery_monitor_, ::android::BATTERY_PROP_PART_STATUS,
                                        static_cast<int64_t>(BatteryPartStatus::UNSUPPORTED),
                                        &part_status);
        !res.isOk()) {
        LOG(WARNING) << "Cannot get Battery_part_status: " << res.getDescription();
    }
    out->batteryPartStatus = static_cast<BatteryPartStatus>(part_status);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Health::getDiskStats(std::vector<DiskStats>*) {
    // This implementation does not support DiskStats. An implementation may extend this
    // class and override this function to support disk stats.
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Health::getStorageInfo(std::vector<StorageInfo>*) {
    // This implementation does not support StorageInfo. An implementation may extend this
    // class and override this function to support storage info.
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Health::getHingeInfo(std::vector<HingeInfo>*) {
    // This implementation does not support HingeInfo. An implementation may extend this
    // class and override this function to support storage info.
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Health::getHealthInfo(HealthInfo* out) {
    battery_monitor_.updateValues();

    *out = battery_monitor_.getHealthInfo();

    // Fill in storage infos; these aren't retrieved by BatteryMonitor.
    if (auto res = getStorageInfo(&out->storageInfos); !res.isOk()) {
        if (res.getServiceSpecificError() == 0 &&
            res.getExceptionCode() != EX_UNSUPPORTED_OPERATION) {
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    IHealth::STATUS_UNKNOWN,
                    ("getStorageInfo fails: " + res.getDescription()).c_str());
        }
        LOG(DEBUG) << "getHealthInfo: getStorageInfo fails with service-specific error, clearing: "
                   << res.getDescription();
        out->storageInfos = {};
    }
    if (auto res = getDiskStats(&out->diskStats); !res.isOk()) {
        if (res.getServiceSpecificError() == 0 &&
            res.getExceptionCode() != EX_UNSUPPORTED_OPERATION) {
            return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                    IHealth::STATUS_UNKNOWN,
                    ("getDiskStats fails: " + res.getDescription()).c_str());
        }
        LOG(DEBUG) << "getHealthInfo: getDiskStats fails with service-specific error, clearing: "
                   << res.getDescription();
        out->diskStats = {};
    }

    // A subclass may want to update health info struct before returning it.
    UpdateHealthInfo(out);

    return ndk::ScopedAStatus::ok();
}

binder_status_t Health::dump(int fd, const char**, uint32_t) {
    battery_monitor_.dumpState(fd);

    ::android::base::WriteStringToFd("\ngetHealthInfo -> ", fd);
    HealthInfo health_info;
    auto res = getHealthInfo(&health_info);
    if (res.isOk()) {
        ::android::base::WriteStringToFd(health_info.toString(), fd);
    } else {
        ::android::base::WriteStringToFd(res.getDescription(), fd);
    }
    ::android::base::WriteStringToFd("\n", fd);

    fsync(fd);
    return STATUS_OK;
}

std::optional<bool> Health::ShouldKeepScreenOn() {
    if (!healthd_config_->screen_on) {
        return std::nullopt;
    }

    HealthInfo health_info;
    auto res = getHealthInfo(&health_info);
    if (!res.isOk()) {
        return std::nullopt;
    }

    ::android::BatteryProperties props = {};
    convert(health_info, &props);
    return healthd_config_->screen_on(&props);
}

//
// Subclass helpers / overrides
//

void Health::UpdateHealthInfo(HealthInfo* /* health_info */) {
    /*
        // Sample code for a subclass to implement this:
        // If you need to modify values (e.g. batteryChargeTimeToFullNowSeconds), do it here.
        health_info->batteryChargeTimeToFullNowSeconds = calculate_charge_time_seconds();

        // If you need to call healthd_board_battery_update, modify its signature
        // and implementation to operate on HealthInfo directly, then call:
        healthd_board_battery_update(health_info);
    */
}

//
// Methods that handle callbacks.
//

ndk::ScopedAStatus Health::registerCallback(const std::shared_ptr<IHealthInfoCallback>& callback) {
    if (callback == nullptr) {
        // For now, this shouldn't happen because argument is not nullable.
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    {
        std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);
        auto linked_callback_result = LinkedCallback::Make(ref<Health>(), callback);
        if (!linked_callback_result.ok()) {
            return ndk::ScopedAStatus::fromStatus(-linked_callback_result.error().code());
        }
        callbacks_[*linked_callback_result] = callback;
        // unlock
    }

    HealthInfo health_info;
    if (auto res = getHealthInfo(&health_info); !res.isOk()) {
        LOG(WARNING) << "Cannot call getHealthInfo: " << res.getDescription();
        // No health info to send, so return early.
        return ndk::ScopedAStatus::ok();
    }

    auto res = callback->healthInfoChanged(health_info);
    if (!res.isOk()) {
        LOG(DEBUG) << "Cannot call healthInfoChanged:" << res.getDescription()
                   << ". Do nothing here if callback is dead as it will be cleaned up later.";
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Health::unregisterCallback(
        const std::shared_ptr<IHealthInfoCallback>& callback) {
    if (callback == nullptr) {
        // For now, this shouldn't happen because argument is not nullable.
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);

    auto matches = [callback](const auto& cb) {
        return cb->asBinder() == callback->asBinder();  // compares binder object
    };
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

// A combination of the HIDL version
//   android::hardware::health::V2_1::implementation::Health::update() and
//   android::hardware::health::V2_1::implementation::BinderHealth::update()
ndk::ScopedAStatus Health::update() {
    HealthInfo health_info;
    if (auto res = getHealthInfo(&health_info); !res.isOk()) {
        LOG(DEBUG) << "Cannot call getHealthInfo for update(): " << res.getDescription();
        // Propagate service specific errors. If there's none, report unknown error.
        if (res.getServiceSpecificError() != 0 ||
            res.getExceptionCode() == EX_UNSUPPORTED_OPERATION) {
            return res;
        }
        return ndk::ScopedAStatus::fromServiceSpecificErrorWithMessage(
                IHealth::STATUS_UNKNOWN, res.getDescription().c_str());
    }
    battery_monitor_.logValues();
    OnHealthInfoChanged(health_info);
    return ndk::ScopedAStatus::ok();
}

void Health::OnHealthInfoChanged(const HealthInfo& health_info) {
    // Notify all callbacks
    std::unique_lock<decltype(callbacks_lock_)> lock(callbacks_lock_);
    for (const auto& [_, callback] : callbacks_) {
        auto res = callback->healthInfoChanged(health_info);
        if (!res.isOk()) {
            LOG(DEBUG) << "Cannot call healthInfoChanged:" << res.getDescription()
                       << ". Do nothing here if callback is dead as it will be cleaned up later.";
        }
    }
    lock.unlock();

    // Let HalHealthLoop::OnHealthInfoChanged() adjusts uevent / wakealarm periods
}

void Health::BinderEvent(uint32_t /*epevents*/) {
    if (binder_fd_ >= 0) {
        ABinderProcess_handlePolledCommands();
    }
}

void Health::OnInit(HalHealthLoop* hal_health_loop, struct healthd_config* config) {
    LOG(INFO) << instance_name_ << " instance initializing with healthd_config...";

    // Similar to HIDL's android::hardware::health::V2_1::implementation::HalHealthLoop::Init,
    // copy configuration parameters to |config| for HealthLoop (e.g. uevent / wake alarm periods)
    *config = *healthd_config_.get();

    binder_status_t status = ABinderProcess_setupPolling(&binder_fd_);

    if (status == ::STATUS_OK && binder_fd_ >= 0) {
        std::shared_ptr<Health> thiz = ref<Health>();
        auto binder_event = [thiz](auto*, uint32_t epevents) { thiz->BinderEvent(epevents); };
        if (hal_health_loop->RegisterEvent(binder_fd_, binder_event, EVENT_NO_WAKEUP_FD) != 0) {
            PLOG(ERROR) << instance_name_ << " instance: Register for binder events failed";
        }
    }

    std::string health_name = IHealth::descriptor + "/"s + instance_name_;
    CHECK_EQ(STATUS_OK, AServiceManager_addService(this->asBinder().get(), health_name.c_str()))
            << instance_name_ << ": Failed to register HAL";

    LOG(INFO) << instance_name_ << ": Hal init done";
}

// Unlike hwbinder, for binder, there's no need to explicitly call flushCommands()
// in PrepareToWait(). See b/139697085.

}  // namespace aidl::android::hardware::health
