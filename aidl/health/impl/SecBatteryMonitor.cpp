/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "health-impl/SecBatteryMonitor.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

using ::aidl::android::hardware::health::HealthInfo;
using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Split;
using ::android::base::Trim;
using ::android::base::WriteFully;

namespace aidl::vendor::samsung::hardware::health {

namespace {

constexpr const char* kSysfsBase = "/sys/class/power_supply/battery";
constexpr const char* kCableCountEfs = "/efs/FactoryApp/batt_cable_count";
constexpr const char* kCapacityMaxSysfs = "/sys/class/power_supply/battery/batt_capacity_max";
constexpr const char* kCapacityMaxEfs = "/efs/Battery/batt_capacity_max";
constexpr const char* kErrWthmSysfs = "/sys/class/power_supply/battery/err_wthm";
constexpr const char* kErrWthmEfs = "/efs/Battery/err_wthm";
constexpr const char* kPrevBattSysfs = "/sys/class/power_supply/battery/prev_battery_data";
constexpr const char* kPrevBattEfs = "/efs/FactoryApp/prev_batt_data";

struct CisdEntry {
    const char* node;     // sysfs attribute name under kSysfsBase
    const char* efs;      // matching /efs file
    bool clear_sentinel;  // cisd_data uses "-1" to request a clear at next boot
};

constexpr CisdEntry kCisdEntries[] = {
        {"cisd_data", "/efs/FactoryApp/cisd_data", true},
        {"cisd_wc_data", "/efs/FactoryApp/cisd_wc_data", false},
        {"cisd_dcerr_data", "/efs/FactoryApp/cisd_dcerr_data", false},
        {"cisd_power_data", "/efs/FactoryApp/cisd_power_data", false},
        {"cisd_pd_data", "/efs/FactoryApp/cisd_pd_data", false},
        {"cisd_cable_data", "/efs/FactoryApp/cisd_cable_data", false},
        {"cisd_tx_data", "/efs/FactoryApp/cisd_tx_data", false},
        {"cisd_event_data", "/efs/FactoryApp/cisd_event_data", false},
};

constexpr size_t kMaxCisdLen = 1024;  // larger is treated as broken data

// Battery-aging history (temperature-vs-charge) constants.
constexpr const char* kBattTempChargeEfs = "/efs/FactoryApp/batt_temp_charge";
constexpr int64_t kAgingMaxElapsed = 7200;     // (2h): longer gaps (suspend) are discarded
constexpr int kAgingFlushThreshold = 1800;     // 30min of accumulated time triggers a flush
constexpr int kAgingChargeChangeBoost = 1800;  // a charge-state change forces the next flush

// Map a battery temperature (tenths of a degree C) to its aging bucket
int TempBucket(int temp_tenths) {
    if (temp_tenths >= 550) return 0;  // >= 55.0
    if (temp_tenths > 449) return 1;   // 45.0 .. 54.9
    if (temp_tenths >= 100) return 2;  // 10.0 .. 44.9
    if (temp_tenths >= -50) return 3;  // -5.0 .. 9.9
    return 4;                          // < -5.0
}

bool ChargerOnline(const HealthInfo& health_info) {
    return health_info.chargerAcOnline || health_info.chargerUsbOnline ||
           health_info.chargerWirelessOnline || health_info.chargerDockOnline;
}

bool WriteFileSync(const std::string& path, const std::string& data, bool create) {
    int flags = O_RDWR | O_TRUNC | O_CLOEXEC | (create ? O_CREAT : 0);
    int fd = open(path.c_str(), flags, 0660);
    if (fd < 0) {
        PLOG(ERROR) << "healthd: Could not open " << path;
        return false;
    }
    if (create) {
        fchmod(fd, 0660);
    }
    lseek(fd, 0, SEEK_SET);
    bool ok = WriteFully(fd, data.data(), data.size());
    if (!ok) {
        PLOG(ERROR) << "healthd: Could not write " << path;
    }
    fdatasync(fd);
    close(fd);
    return ok;
}

bool WriteSysfs(const std::string& path, const std::string& data) {
    return WriteFileSync(path, data, /* create */ false);
}

bool WriteEfs(const std::string& path, const std::string& data) {
    return WriteFileSync(path, data, /* create */ true);
}

}  // namespace

void SecBatteryMonitor::Restore() {
    // Seed the cached single-int values from /efs so Persist() only writes on real changes.
    std::string content;
    if (ReadFileToString(kCapacityMaxEfs, &content)) {
        ParseInt(Trim(content), &old_capacity_max_);
    }
    if (ReadFileToString(kErrWthmEfs, &content)) {
        ParseInt(Trim(content), &old_err_wthm_);
    }

    // Restore each CISD counter from /efs back into its sysfs node (the kernel starts empty).
    for (const auto& e : kCisdEntries) {
        std::string sysfs_path = std::string(kSysfsBase) + "/" + e.node;
        if (access(sysfs_path.c_str(), R_OK) != 0) {
            continue;
        }
        std::string efs_data;
        if (!ReadFileToString(e.efs, &efs_data)) {
            continue;
        }
        if (efs_data.size() >= 1 && efs_data.size() < kMaxCisdLen) {
            std::string trimmed = Trim(efs_data);
            LOG(INFO) << "healthd: initial " << e.node << ": " << trimmed;
            WriteSysfs(sysfs_path, efs_data);
            // A stored "-1" means "clear pending": after pushing it to sysfs, reset /efs to "0".
            if (e.clear_sentinel && trimmed == "-1") {
                WriteEfs(e.efs, "0");
            }
        } else {
            // Missing/broken persisted data: reinitialize the /efs copy.
            WriteEfs(e.efs, "-1\n");
        }
    }

    // prev_batt_data has no efs->sysfs restore; init just seeds the /efs file if it is missing.
    if (access(kPrevBattEfs, F_OK) != 0) {
        LOG(INFO) << "healthd: create prev battery data";
        WriteEfs(kPrevBattEfs, "0, 0, 0, 0\n");
    }
}

void SecBatteryMonitor::Persist(const HealthInfo& health_info) {
    // Mirror each sysfs CISD counter out to /efs when it differs from the stored copy.
    for (const auto& e : kCisdEntries) {
        std::string sysfs_path = std::string(kSysfsBase) + "/" + e.node;
        std::string sysfs_data;
        if (!ReadFileToString(sysfs_path, &sysfs_data)) {
            continue;  // node not present on this device
        }
        std::string efs_data;
        bool efs_ok = ReadFileToString(e.efs, &efs_data);

        std::string sysfs_trim = Trim(sysfs_data);
        std::string efs_trim = Trim(efs_data);

        if (e.clear_sentinel && efs_ok && efs_trim == "-1") {
            LOG(INFO) << "healthd: do initial " << e.node << " at next boot because clear data";
            continue;
        }

        if (sysfs_data.size() < kMaxCisdLen) {
            if (!sysfs_trim.empty() && efs_ok && sysfs_trim != efs_trim) {
                LOG(INFO) << "healthd: update " << e.node << " " << sysfs_trim;
                WriteEfs(e.efs, sysfs_trim + "\n");
            }
        } else {
            LOG(INFO) << "healthd: do initial " << e.node << " because broken data";
            WriteSysfs(sysfs_path, "-1\n");
        }
    }

    PersistCableCount(health_info);
    PersistIntNode(kCapacityMaxSysfs, kCapacityMaxEfs, &old_capacity_max_);
    PersistIntNode(kErrWthmSysfs, kErrWthmEfs, &old_err_wthm_);
    PersistPrevBattData();
    UpdateAgingHistory(health_info);
}

void SecBatteryMonitor::PersistCableCount(const HealthInfo& health_info) {
    bool online = health_info.chargerAcOnline || health_info.chargerUsbOnline ||
                  health_info.chargerWirelessOnline || health_info.chargerDockOnline;
    bool rising_edge = online && !prev_charger_online_;
    prev_charger_online_ = online;
    if (!rising_edge) {
        return;
    }

    int count = 0;
    std::string content;
    if (ReadFileToString(kCableCountEfs, &content)) {
        ParseInt(Trim(content), &count);
    }
    std::string out = std::to_string(count + 1) + "\n";
    WriteEfs(kCableCountEfs, out);
    WriteSysfs(std::string(kSysfsBase) + "/cisd_wire_count", out);
}

void SecBatteryMonitor::PersistIntNode(const char* sysfs_path, const char* efs_path,
                                       int* old_value) {
    std::string content;
    if (!ReadFileToString(sysfs_path, &content)) {
        return;
    }
    int value = 0;
    if (!ParseInt(Trim(content), &value)) {
        return;
    }
    if (value == *old_value) {
        return;
    }
    LOG(INFO) << "healthd: " << efs_path << " " << value << " (was " << *old_value << ")";
    if (WriteEfs(efs_path, std::to_string(value) + "\n")) {
        *old_value = value;
    }
}

void SecBatteryMonitor::PersistPrevBattData() {
    std::string sysfs_data;
    if (!ReadFileToString(kPrevBattSysfs, &sysfs_data)) {
        return;  // node not present on this device
    }
    std::string efs_data;
    ReadFileToString(kPrevBattEfs, &efs_data);

    std::string sysfs_trim = Trim(sysfs_data);
    if (sysfs_trim.empty() || sysfs_trim == Trim(efs_data)) {
        return;
    }
    LOG(INFO) << "healthd: update prev_batt_data " << sysfs_trim;
    WriteEfs(kPrevBattEfs, sysfs_trim + "\n");
}

void SecBatteryMonitor::UpdateAgingHistory(const HealthInfo& health_info) {
    int64_t now = static_cast<int64_t>(time(nullptr));
    bool charging = ChargerOnline(health_info);
    bool consider_flush = true;

    if (ah_have_prev_) {
        int64_t elapsed = now - ah_last_time_;
        if (elapsed < 0 || elapsed > kAgingMaxElapsed) {
            LOG(WARNING) << "healthd: Invalid elapsed time " << elapsed;
            consider_flush = false;
        } else {
            int bucket = TempBucket(ah_prev_temp_);
            if (!ah_prev_charging_) {
                // Discharging: accumulate dwell time in this temperature bucket.
                ah_time_[bucket] += elapsed;
            } else {
                // Charging: accumulate the capacity gained in this temperature bucket.
                int delta = health_info.batteryLevel - ah_prev_level_;
                if (delta > 0) {
                    ah_cap_[bucket] += delta;
                }
            }
            ah_ts_[bucket] = now;
            ah_accum_time_ += static_cast<int>(elapsed);
            // A charge/discharge transition forces the record to be flushed promptly.
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
            // ah_ts_ (per-bucket last-touch time) is intentionally not reset.
        }
    }

    ah_last_time_ = now;
    ah_prev_temp_ = health_info.batteryTemperatureTenthsCelsius;
    ah_prev_level_ = health_info.batteryLevel;
    ah_prev_charging_ = charging;
    ah_have_prev_ = true;
}

void SecBatteryMonitor::FlushAgingHistory(int64_t now) {
    uint64_t out_ts;
    int32_t out_cap[kNumTempBuckets];
    uint64_t out_time[kNumTempBuckets];
    uint64_t out_tsb[kNumTempBuckets];

    std::string content;
    if (ReadFileToString(kBattTempChargeEfs, &content) && !content.empty()) {
        // Merge with the stored record: ts, cap[5], time[5], last-touch[5] (16 comma-sep fields).
        std::vector<std::string> tok = Split(content, ",");
        auto field_ll = [&](size_t i) -> long long {
            return i < tok.size() ? strtoll(Trim(tok[i]).c_str(), nullptr, 10) : 0;
        };
        auto field_ull = [&](size_t i) -> uint64_t {
            return i < tok.size() ? strtoull(Trim(tok[i]).c_str(), nullptr, 10) : 0;
        };
        out_ts = field_ull(0);  // creation timestamp is preserved across flushes
        for (int i = 0; i < kNumTempBuckets; i++) {
            out_cap[i] = ah_cap_[i] + static_cast<int32_t>(field_ll(1 + i));
            out_time[i] = static_cast<uint64_t>(ah_time_[i]) + field_ull(6 + i);
            out_tsb[i] = ah_ts_[i] >= 1 ? static_cast<uint64_t>(ah_ts_[i]) : field_ull(11 + i);
        }
    } else {
        // No prior record: seed from the in-memory buckets and stamp the creation time.
        LOG(ERROR) << "healthd: " << kBattTempChargeEfs << " does not exist";
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
        PLOG(ERROR) << "healthd: failed to open " << kBattTempChargeEfs;
        return;
    }
    fchown(fd, 1000 /* AID_SYSTEM */, 1000);
    if (!WriteFully(fd, record.data(), record.size())) {
        PLOG(ERROR) << "healthd: failed to write history to " << kBattTempChargeEfs;
    }
    close(fd);
}

}  // namespace aidl::vendor::samsung::hardware::health
