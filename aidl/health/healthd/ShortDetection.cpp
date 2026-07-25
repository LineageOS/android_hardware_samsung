/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "ShortDetection"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "samsung-health/ShortDetection.h"
#include "samsung-health/Utils.h"

using ::aidl::android::hardware::health::HealthInfo;
using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Trim;

namespace hardware {
namespace samsung {
namespace health {

ShortDetection::ShortDetection() {}

void ShortDetection::RestoreCisdData() {
    // Restore each CISD counter from /efs back into its sysfs node (the kernel starts empty).
    for (const auto& e : kCisdDataEntries) {
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
            LOG(INFO) << "initial " << e.node << ": " << trimmed;
            WriteSysfs(sysfs_path, efs_data);
            if (e.clearEfsData && trimmed == "-1") {
                WriteEfs(e.efs, "0");
            }
        } else {
            WriteEfs(e.efs, "-1\n");
        }
    }
}

void ShortDetection::UpdateCisdData() {
    // Mirror each sysfs CISD data to EFS when it differs from the EFS stored data.
    for (const auto& e : kCisdDataEntries) {
        std::string sysfs_path = std::string(kSysfsBase) + "/" + e.node;
        std::string sysfs_data;
        if (!ReadFileToString(sysfs_path, &sysfs_data)) {
            continue;
        }
        std::string efs_data;
        bool efs_ok = ReadFileToString(e.efs, &efs_data);

        std::string sysfs_trim = Trim(sysfs_data);
        std::string efs_trim = Trim(efs_data);

        if (e.clearEfsData && efs_ok && efs_trim == "-1") {
            LOG(INFO) << "do initial " << e.node << " at next boot";
            continue;
        }

        if (sysfs_data.size() < kMaxCisdLen) {
            if (!sysfs_trim.empty() && efs_ok && sysfs_trim != efs_trim) {
                LOG(INFO) << "update " << e.node << " " << sysfs_trim;
                WriteEfs(e.efs, sysfs_trim + "\n");
            }
        } else {
            LOG(INFO) << "do initial " << e.node << " because broken data";
            WriteSysfs(sysfs_path, "-1\n");
        }
    }
}

void ShortDetection::UpdateCableCount(const HealthInfo& health_info) {
    bool online = isChargerOnline(health_info);
    bool was_online = prev_charger_online_;
    prev_charger_online_ = online;
    if (!online || was_online) {
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

}  // namespace health
}  // namespace samsung
}  // namespace hardware
