/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SamsungHealth"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "samsung-health/Utils.h"

using ::android::base::ParseInt;
using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteFully;

namespace hardware {
namespace samsung {
namespace health {

static bool WriteFileSync(const std::string& path, const std::string& data, bool create) {
    int flags = O_RDWR | O_TRUNC | O_CLOEXEC | (create ? O_CREAT : 0);
    int fd = open(path.c_str(), flags, 0660);
    if (fd < 0) {
        PLOG(ERROR) << "Could not open " << path;
        return false;
    }
    if (create) {
        fchmod(fd, 0660);
    }
    lseek(fd, 0, SEEK_SET);
    bool ok = WriteFully(fd, data.data(), data.size());
    if (!ok) {
        PLOG(ERROR) << "Could not write " << path;
    }
    fdatasync(fd);
    close(fd);
    return ok;
}

bool WriteSysfs(const std::string& path, const std::string& data) {
    return WriteFileSync(path, data, false);
}

bool WriteEfs(const std::string& path, const std::string& data) {
    return WriteFileSync(path, data, true);
}

}  // namespace health
}  // namespace samsung
}  // namespace hardware
