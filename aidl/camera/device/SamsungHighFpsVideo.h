/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <CameraMetadata.h>
#include <SamsungCameraModule.h>
#include <system/camera_metadata.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

/*
 * Samsung Exynos camera HALs only accept a per-request CONTROL_AE_TARGET_FPS_RANGE with a
 * maximum between 31 and 119 fps (e.g. [60, 60]) when it is part of the sensor's public fps
 * range table (which tops out at 30 fps) or when the session was configured by a "Samsung"
 * client, signalled through the samsung.android.control.cameraClient session parameter.
 * Otherwise the HAL silently keeps the previous range, so 60 fps video records at 30 fps.
 *
 * The HAL does advertise its real video modes in samsung.android.scaler.availableVideoConfigurations
 * (width, height, minFps, maxFps, ...). This helper:
 *  - exposes the fixed high frame rates from that table as standard AE target fps ranges and
 *    fixes the matching minimum frame durations,
 *  - makes CONTROL_AE_TARGET_FPS_RANGE a session key so the framework hands it to
 *    configure_streams (and reconfigures when an app switches between 30 and 60 fps),
 *  - translates a high fps session range into the Samsung session parameters the HAL expects.
 */
class SamsungHighFpsVideo {
  public:
    // Resolves the Samsung vendor tags from the camera module. Safe to call multiple times.
    static void init(const sp<common::helper::SamsungCameraModule>& module);

    // Adds the high fps video modes of the HAL to the static characteristics.
    static void patchCharacteristics(common::helper::CameraMetadata* chars);

    // Returns true and fills |minFps|/|maxFps| if |params| requests a high fps AE range.
    static bool getHighFpsRange(const camera_metadata_t* params, int32_t* minFps, int32_t* maxFps);

    // Adds the Samsung session parameters matching the requested high fps range to |params|.
    // Returns false if nothing had to be changed.
    static bool patchSessionParams(common::helper::CameraMetadata* params);

    // Returns true if a change between two sets of session parameters needs a stream
    // reconfiguration: any change other than the AE target fps range, or a change of the
    // effective high fps range.
    static bool isReconfigurationRequired(const camera_metadata_t* oldParams,
                                          const camera_metadata_t* newParams);
};

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
