/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <CameraModule.h>

namespace android {
namespace hardware {
namespace camera {
namespace common {
namespace helper {

class SamsungCameraModule : public CameraModule {
  public:
    explicit SamsungCameraModule(camera_module_t* module);
    virtual ~SamsungCameraModule();

    int getCameraDeviceVersion(int cameraId, uint32_t* version);
    int sehGetDeviceVersion(int cameraId);
    int getConcurrentStreamingCameraIds(uint32_t *pConcCamArrayLength, concurrent_camera_combination_t  **ppConcCamArray);
    int isConcurrentStreamCombinationSupported(const std::vector<cameraid_stream_combination_t>& rCameraIdStreamComboVec);
    bool isSetTorchModeStrengthSupported();
    int setTorchModeStrength(const char* camera_id, bool enable, int strength);

  private:
    camera_module_t* mModule;
    KeyedVector<int, int> mDeviceVersionMap;
};

}  // namespace helper
}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
