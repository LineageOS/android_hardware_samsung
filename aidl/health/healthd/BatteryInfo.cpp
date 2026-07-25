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

#include "samsung-health/BatteryInfo.h"
#include "samsung-health/Utils.h"

using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Trim;

namespace hardware {
namespace samsung {
namespace health {

BatteryInfo::BatteryInfo() {}

void BatteryInfo::RestoreBattType() {
    std::string batt_type_sysfs = kBattTypeSysfs;
    if (access(batt_type_sysfs.c_str(), R_OK) != 0) {
        return;
    }

    int presence = 0;
    int auth = 0;
    std::string tmp;
    if (ReadFileToString(kSecAuthPresence, &tmp)) ParseInt(Trim(tmp), &presence);
    if (ReadFileToString(kSecAuthBattAuth, &tmp)) ParseInt(Trim(tmp), &auth);

    // Fallback to EFS QR if sec_auth is not present
    const char* qr_source = (presence == 1 && auth == 1) ? kSecAuthQrCode : kHwParamBattQrEfs;
    std::string qr;
    if (!ReadFileToString(qr_source, &qr)) {
        return;
    }
    qr = Trim(qr);

    auto plus = qr.rfind('+');
    if (plus != std::string::npos && plus < 40) {
        qr = qr.substr(0, plus + 1) + "XXXXXXX";
    }
    LOG(INFO) << "initial batt_type: " << qr;

    WriteSysfs(batt_type_sysfs, qr);
}

// To-Do: Calculate battery manufacturing date

}  // namespace health
}  // namespace samsung
}  // namespace hardware
