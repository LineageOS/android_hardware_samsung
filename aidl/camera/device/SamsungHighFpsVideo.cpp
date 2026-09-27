/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "CamDev-HighFps"

#include "SamsungHighFpsVideo.h"

#include <log/log.h>
#include <system/camera_vendor_tags.h>
#include <system/graphics-base.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifndef CAMERA_HIGH_FPS_VIDEO_CLIENT
// samsung.android.control.cameraClient value injected for high fps sessions. The HAL treats
// 1 and 5 as the Samsung camera app and 2, 3, 4 and 6 as Samsung camera SDK clients; both
// lift the 30 fps limit. The SDK path changes the fewest other HAL behaviours.
#define CAMERA_HIGH_FPS_VIDEO_CLIENT 2
#endif

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

using common::helper::CameraMetadata;

namespace {

// samsung.android.scaler.availableVideoConfigurations: (width, height, minFps, maxFps, x, y)
constexpr size_t kVideoConfigStride = 6;
// The HAL validates ranges with a maximum in [31, 119]; 120 fps and above is high speed video.
constexpr int32_t kMinHighFps = 31;
constexpr int32_t kMaxHighFps = 119;
constexpr int64_t kNsPerSec = 1000000000LL;

struct SamsungTags {
    bool valid = false;
    uint32_t availableVideoConfigurations = 0;
    uint32_t cameraClient = 0;
    uint32_t recordingMaxFps = 0;
    uint32_t recordingMinFps = 0;
};

std::once_flag sInitFlag;
SamsungTags sTags;

bool findTag(const vendor_tag_ops_t& ops, const std::vector<uint32_t>& tags, const char* section,
             const char* name, uint32_t* out) {
    for (uint32_t tag : tags) {
        const char* s = ops.get_section_name(&ops, tag);
        const char* n = ops.get_tag_name(&ops, tag);
        if (s != nullptr && n != nullptr && strcmp(s, section) == 0 && strcmp(n, name) == 0) {
            *out = tag;
            return true;
        }
    }
    return false;
}

bool hasPair(const camera_metadata_ro_entry& entry, int32_t a, int32_t b) {
    for (size_t i = 0; i + 1 < entry.count; i += 2) {
        if (entry.data.i32[i] == a && entry.data.i32[i + 1] == b) {
            return true;
        }
    }
    return false;
}

// Returns true if |a| has an entry, other than the AE target fps range, that is missing from
// or different in |b|.
bool otherParamsDiffer(const camera_metadata_t* a, const camera_metadata_t* b) {
    size_t count = a != nullptr ? get_camera_metadata_entry_count(a) : 0;
    for (size_t i = 0; i < count; i++) {
        camera_metadata_ro_entry_t ea, eb;
        if (get_camera_metadata_ro_entry(a, i, &ea) != 0) {
            return true;
        }
        if (ea.tag == ANDROID_CONTROL_AE_TARGET_FPS_RANGE) {
            continue;
        }
        if (b == nullptr || find_camera_metadata_ro_entry(b, ea.tag, &eb) != 0 ||
            ea.type != eb.type || ea.count != eb.count ||
            memcmp(ea.data.u8, eb.data.u8, camera_metadata_type_size[ea.type] * ea.count) != 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

void SamsungHighFpsVideo::init(const sp<common::helper::SamsungCameraModule>& module) {
    std::call_once(sInitFlag, [&module]() {
        if (module == nullptr || !module->isVendorTagDefined()) {
            return;
        }

        vendor_tag_ops_t ops{};
        module->getVendorTagOps(&ops);
        if (ops.get_tag_count == nullptr || ops.get_all_tags == nullptr ||
            ops.get_section_name == nullptr || ops.get_tag_name == nullptr) {
            return;
        }

        int count = ops.get_tag_count(&ops);
        if (count <= 0) {
            return;
        }
        std::vector<uint32_t> tags(count);
        ops.get_all_tags(&ops, tags.data());

        SamsungTags t;
        t.valid = findTag(ops, tags, "samsung.android.scaler", "availableVideoConfigurations",
                          &t.availableVideoConfigurations) &&
                  findTag(ops, tags, "samsung.android.control", "cameraClient", &t.cameraClient) &&
                  findTag(ops, tags, "samsung.android.control", "recordingMaxFps",
                          &t.recordingMaxFps) &&
                  findTag(ops, tags, "samsung.android.control", "recordingMinFps",
                          &t.recordingMinFps);
        if (!t.valid) {
            ALOGW("%s: Samsung video vendor tags not found, high fps video disabled", __FUNCTION__);
            return;
        }
        sTags = t;
    });
}

void SamsungHighFpsVideo::patchCharacteristics(CameraMetadata* chars) {
    if (chars == nullptr || !sTags.valid) {
        return;
    }
    // Read-only view: const find() does not lock the buffer. Entries are invalidated by
    // update(), so each one is looked up again right before it is used.
    const CameraMetadata& ro = *chars;

    camera_metadata_ro_entry videoConfigs = ro.find(sTags.availableVideoConfigurations);
    if (videoConfigs.count == 0 || videoConfigs.count % kVideoConfigStride != 0) {
        return;
    }

    // Collect the fixed high frame rates and the sizes supporting them.
    std::set<int32_t> highFps;
    std::vector<std::pair<std::pair<int32_t, int32_t>, int32_t>> sizes;  // ((w, h), fps)
    for (size_t i = 0; i < videoConfigs.count; i += kVideoConfigStride) {
        const int32_t* c = &videoConfigs.data.i32[i];
        if (c[2] == c[3] && c[3] >= kMinHighFps && c[3] <= kMaxHighFps) {
            highFps.insert(c[3]);
            sizes.push_back({{c[0], c[1]}, c[3]});
        }
    }
    if (highFps.empty()) {
        return;
    }

    // 1. Advertise the fixed high fps AE target ranges.
    camera_metadata_ro_entry ranges = ro.find(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
    std::vector<int32_t> newRanges(ranges.data.i32, ranges.data.i32 + ranges.count);
    for (int32_t fps : highFps) {
        if (!hasPair(ranges, fps, fps)) {
            newRanges.push_back(fps);
            newRanges.push_back(fps);
        }
    }
    if (newRanges.size() != ranges.count) {
        chars->update(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, newRanges.data(),
                      newRanges.size());
    }

    // 2. Lower the minimum frame durations of the video capable sizes accordingly.
    camera_metadata_ro_entry durations = ro.find(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
    if (durations.count > 0 && durations.count % 4 == 0) {
        std::vector<int64_t> newDurations(durations.data.i64, durations.data.i64 + durations.count);
        bool changed = false;
        for (size_t i = 0; i < newDurations.size(); i += 4) {
            int64_t format = newDurations[i];
            if (format != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
                format != HAL_PIXEL_FORMAT_YCBCR_420_888) {
                continue;
            }
            for (const auto& [size, fps] : sizes) {
                if (newDurations[i + 1] == size.first && newDurations[i + 2] == size.second) {
                    int64_t minDuration = kNsPerSec / fps;
                    if (newDurations[i + 3] > minDuration) {
                        newDurations[i + 3] = minDuration;
                        changed = true;
                    }
                }
            }
        }
        if (changed) {
            chars->update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, newDurations.data(),
                          newDurations.size());
        }
    }

    // 3. Make the AE target fps range a session key, so it reaches configure_streams.
    camera_metadata_ro_entry sessionKeys = ro.find(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS);
    std::vector<int32_t> newSessionKeys(sessionKeys.data.i32,
                                        sessionKeys.data.i32 + sessionKeys.count);
    if (std::find(newSessionKeys.begin(), newSessionKeys.end(),
                  ANDROID_CONTROL_AE_TARGET_FPS_RANGE) == newSessionKeys.end()) {
        newSessionKeys.push_back(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        chars->update(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, newSessionKeys.data(),
                      newSessionKeys.size());

        camera_metadata_ro_entry charKeys = ro.find(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS);
        if (charKeys.count > 0) {
            std::vector<int32_t> newCharKeys(charKeys.data.i32, charKeys.data.i32 + charKeys.count);
            if (std::find(newCharKeys.begin(), newCharKeys.end(),
                          ANDROID_REQUEST_AVAILABLE_SESSION_KEYS) == newCharKeys.end()) {
                newCharKeys.push_back(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS);
                chars->update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, newCharKeys.data(),
                              newCharKeys.size());
            }
        }
    }
}

bool SamsungHighFpsVideo::getHighFpsRange(const camera_metadata_t* params, int32_t* minFps,
                                          int32_t* maxFps) {
    if (params == nullptr || !sTags.valid) {
        return false;
    }

    camera_metadata_ro_entry_t entry;
    if (find_camera_metadata_ro_entry(params, ANDROID_CONTROL_AE_TARGET_FPS_RANGE, &entry) != 0 ||
        entry.count != 2) {
        return false;
    }

    int32_t lo = entry.data.i32[0];
    int32_t hi = entry.data.i32[1];
    if (hi < kMinHighFps || hi > kMaxHighFps || lo <= 0 || lo > hi) {
        return false;
    }

    if (minFps != nullptr) *minFps = lo;
    if (maxFps != nullptr) *maxFps = hi;
    return true;
}

bool SamsungHighFpsVideo::patchSessionParams(CameraMetadata* params) {
    if (params == nullptr || params->isEmpty()) {
        return false;
    }

    int32_t minFps, maxFps;
    {
        const camera_metadata_t* raw = params->getAndLock();
        bool isHighFps = getHighFpsRange(raw, &minFps, &maxFps);
        params->unlock(raw);
        if (!isHighFps) {
            return false;
        }
    }

    // Respect values explicitly set by the client.
    if (!params->exists(sTags.cameraClient)) {
        int32_t client = CAMERA_HIGH_FPS_VIDEO_CLIENT;
        params->update(sTags.cameraClient, &client, 1);
    }
    if (!params->exists(sTags.recordingMaxFps)) {
        params->update(sTags.recordingMaxFps, &maxFps, 1);
    }
    if (!params->exists(sTags.recordingMinFps)) {
        params->update(sTags.recordingMinFps, &minFps, 1);
    }

    ALOGI("%s: high fps session [%d, %d], cameraClient %d", __FUNCTION__, minFps, maxFps,
          CAMERA_HIGH_FPS_VIDEO_CLIENT);
    return true;
}

bool SamsungHighFpsVideo::isReconfigurationRequired(const camera_metadata_t* oldParams,
                                                    const camera_metadata_t* newParams) {
    if (otherParamsDiffer(oldParams, newParams) || otherParamsDiffer(newParams, oldParams)) {
        return true;
    }

    int32_t oldMin = 0, oldMax = 0, newMin = 0, newMax = 0;
    bool oldHigh = getHighFpsRange(oldParams, &oldMin, &oldMax);
    bool newHigh = getHighFpsRange(newParams, &newMin, &newMax);
    if (oldHigh != newHigh) {
        return true;
    }
    return oldHigh && (oldMin != newMin || oldMax != newMax);
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
