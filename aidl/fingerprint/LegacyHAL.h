/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hardware/fingerprint.h>

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

class LegacyHAL {
public:
    bool openHal(fingerprint_notify_t notify);
    int request(int cmd, int param);

    int (*ss_fingerprint_close)();
    int (*ss_fingerprint_open)(const char* id);

    int (*ss_set_notify_callback)(fingerprint_notify_t notify);
    uint64_t (*ss_fingerprint_pre_enroll)();
    int (*ss_fingerprint_enroll)(const hw_auth_token_t* hat, uint32_t gid, uint32_t timeout_sec);
    int (*ss_fingerprint_post_enroll)();
    uint64_t (*ss_fingerprint_get_auth_id)();
    int (*ss_fingerprint_cancel)();
    int (*ss_fingerprint_enumerate)();
    int (*ss_fingerprint_remove)(uint32_t gid, uint32_t fid);
    int (*ss_fingerprint_set_active_group)(uint32_t gid, const char* store_path);
    int (*ss_fingerprint_authenticate)(uint64_t operation_id, uint32_t gid);
    int (*ss_fingerprint_request)(uint32_t cmd, char *inBuf, uint32_t inBuf_length, char *outBuf, uint32_t outBuf_length, uint32_t param);
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
