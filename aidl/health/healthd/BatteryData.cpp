/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "BatteryData"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "samsung-health/BatteryData.h"
#include "samsung-health/Utils.h"

using ::aidl::android::hardware::health::HealthInfo;
using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Split;
using ::android::base::Trim;
using ::android::base::WriteFully;

namespace hardware {
namespace samsung {
namespace health {

int ChargingTempBucket(int temp) {
    if (temp >= 550) return 0;  // >= 55.0
    if (temp > 449) return 1;   // 45.0 .. 54.9
    if (temp >= 100) return 2;  // 10.0 .. 44.9
    if (temp >= -50) return 3;  // -5.0 .. 9.9
    return 4;                   // < -5.0
}

BatteryData::BatteryData() {}

void BatteryData::UpdatePrevBattData() {
    std::string sysfs_data;
    if (!ReadFileToString(kPrevBattSysfs, &sysfs_data)) {
        return;
    }
    std::string efs_data;
    ReadFileToString(kPrevBattEfs, &efs_data);

    std::string sysfs_trim = Trim(sysfs_data);
    if (sysfs_trim.empty() || sysfs_trim == Trim(efs_data)) {
        return;
    }
    LOG(INFO) << "update previous battery data " << sysfs_trim;
    WriteEfs(kPrevBattEfs, sysfs_trim + "\n");
}

void BatteryData::UpdateAgingHistory(const HealthInfo& health_info) {
    int64_t now = static_cast<int64_t>(time(nullptr));
    bool charging = isChargerOnline(health_info);
    bool consider_flush = true;

    if (ah_have_prev_) {
        int64_t elapsed = now - ah_last_time_;
        if (elapsed < 0 || elapsed > kAgingMaxElapsed) {
            LOG(WARNING) << "Invalid elapsed time " << elapsed;
            consider_flush = false;
        } else {
            int bucket = ChargingTempBucket(ah_prev_temp_);
            if (!ah_prev_charging_) {
                ah_time_[bucket] += elapsed;
            } else {
                int delta = health_info.batteryLevel - ah_prev_level_;
                if (delta > 0) {
                    ah_cap_[bucket] += delta;
                }
            }
            ah_ts_[bucket] = now;
            ah_accum_time_ += static_cast<int>(elapsed);
            if (charging != ah_prev_charging_) {
                ah_accum_time_ += kAgingChargeChangeBoost;
            }
        }
    }

    if (consider_flush && ah_accum_time_ >= kAgingFlushThreshold) {
        FlushAgingHistory(now);
        ah_accum_time_ = 0;
        for (int i = 0; i < kNumTempBuckets; i++) {
            ah_cap_[i] = 0;
            ah_time_[i] = 0;
        }
    }

    ah_last_time_ = now;
    ah_prev_temp_ = health_info.batteryTemperatureTenthsCelsius;
    ah_prev_level_ = health_info.batteryLevel;
    ah_prev_charging_ = charging;
    ah_have_prev_ = true;
}

void BatteryData::FlushAgingHistory(int64_t now) {
    uint64_t out_ts;
    int32_t out_cap[kNumTempBuckets];
    uint64_t out_time[kNumTempBuckets];
    uint64_t out_tsb[kNumTempBuckets];

    std::string content;
    if (ReadFileToString(kBattTempChargeEfs, &content) && !content.empty()) {
        std::vector<std::string> tok = Split(content, ",");
        auto field_ll = [&](size_t i) -> long long {
            return i < tok.size() ? strtoll(Trim(tok[i]).c_str(), nullptr, 10) : 0;
        };
        auto field_ull = [&](size_t i) -> uint64_t {
            return i < tok.size() ? strtoull(Trim(tok[i]).c_str(), nullptr, 10) : 0;
        };
        out_ts = field_ull(0);
        for (int i = 0; i < kNumTempBuckets; i++) {
            out_cap[i] = ah_cap_[i] + static_cast<int32_t>(field_ll(1 + i));
            out_time[i] = static_cast<uint64_t>(ah_time_[i]) + field_ull(6 + i);
            out_tsb[i] = ah_ts_[i] >= 1 ? static_cast<uint64_t>(ah_ts_[i]) : field_ull(11 + i);
        }
    } else {
        LOG(ERROR) << kBattTempChargeEfs << " does not exist";
        out_ts = static_cast<uint64_t>(now);
        for (int i = 0; i < kNumTempBuckets; i++) {
            out_cap[i] = ah_cap_[i];
            out_time[i] = static_cast<uint64_t>(ah_time_[i]);
            out_tsb[i] = static_cast<uint64_t>(ah_ts_[i]);
        }
    }

    std::string record = std::to_string(out_ts) + ",";
    for (int i = 0; i < kNumTempBuckets; i++) record += std::to_string(out_cap[i]) + ",";
    for (int i = 0; i < kNumTempBuckets; i++) record += std::to_string(out_time[i]) + ",";
    for (int i = 0; i < kNumTempBuckets; i++) record += std::to_string(out_tsb[i]) + ",";
    record += "\n";

    int fd = open(kBattTempChargeEfs, O_RDWR | O_CREAT | O_CLOEXEC, 0660);
    if (fd < 0) {
        PLOG(ERROR) << "failed to open " << kBattTempChargeEfs;
        return;
    }
    fchown(fd, 1000 /* AID_SYSTEM */, 1000);
    if (!WriteFully(fd, record.data(), record.size())) {
        PLOG(ERROR) << "failed to write history to " << kBattTempChargeEfs;
    }
    close(fd);
}

void BatteryData::RestoreRtcStatus() {
    // Locate the RTC status node.
    const char* rtc_path = nullptr;
    int index = -1;
    if (access(kRtcStatusSysfs0, F_OK) == 0) {
        rtc_path = kRtcStatusSysfs0;
        index = 0;
    } else if (access(kRtcStatusSysfs1, F_OK) == 0) {
        rtc_path = kRtcStatusSysfs1;
        index = 1;
    } else {
        LOG(ERROR) << "Could not access rtc_status node";
        return;
    }
    LOG(INFO) << "rtc_status: Found index: " << index;

    int rtc_value = 0;
    std::string tmp;
    if (ReadFileToString(rtc_path, &tmp)) ParseInt(Trim(tmp), &rtc_value);

    // Seed the /efs reset counter (rtcc) if it does not exist yet.
    if (access(kRtcStatusEfs, F_OK) != 0) {
        if (WriteEfs(kRtcStatusEfs, "0")) {
            LOG(INFO) << "initialized rtcc";
        } else {
            LOG(ERROR) << "Could not create " << kRtcStatusEfs;
        }
    }

    if (rtc_value == 0) {
        LOG(INFO) << "RTC was not reset";
        return;
    }

    std::string prev;
    if (!ReadFileToString(kPrevBattEfs, &prev)) {
        LOG(ERROR) << "Could not access " << kPrevBattEfs;
        return;
    }
    int volt = 0;
    int temp = 0;
    int jig = 0;
    int chg = 0;
    if (sscanf(prev.c_str(), "%d, %d, %d, %d", &volt, &temp, &jig, &chg) != 4) {
        LOG(ERROR) << "Broken " << kPrevBattEfs;
    }
    LOG(INFO) << "rtc_status (volt:" << volt << ", temp:" << temp << ", jig:" << jig
              << ", chg:" << chg << ")";

    if (volt < 3700 || temp < 200 /* || temp > 300 */ || jig != 0) {
        return;
    }

    int rtcc = 0;
    if (ReadFileToString(kRtcStatusEfs, &tmp)) ParseInt(Trim(tmp), &rtcc);
    LOG(INFO) << "Update rtcc(" << (rtcc + 1) << ")";
    WriteEfs(kRtcStatusEfs, std::to_string(rtcc + 1));
}

void BatteryData::UpdateCapacityMax() {
    std::string content;
    if (oldCapacityMax < 0) {
        if (ReadFileToString(kCapacityMaxEfs, &content)) {
            ParseInt(Trim(content), &oldCapacityMax);
        }
    }

    if (!ReadFileToString(kCapacityMaxSysfs, &content)) {
        return;
    }
    int value = 0;
    if (!ParseInt(Trim(content), &value)) {
        return;
    }
    if (value == oldCapacityMax) {
        return;
    }
    LOG(INFO) << "" << kCapacityMaxEfs << " " << value << " (was " << oldCapacityMax << ")";
    if (WriteEfs(kCapacityMaxEfs, std::to_string(value) + "\n")) {
        oldCapacityMax = value;
    }
}

}  // namespace health
}  // namespace samsung
}  // namespace hardware
