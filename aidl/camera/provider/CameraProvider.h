/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <SamsungCameraModule.h>
#include <VendorTagDescriptor.h>
#include <aidl/android/hardware/camera/device/ICameraDevice.h>
#include <aidl/android/hardware/camera/provider/BnCameraProvider.h>

#include <map>

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {

using ::aidl::android::hardware::camera::common::CameraDeviceStatus;
using ::aidl::android::hardware::camera::common::VendorTagSection;
using ::aidl::android::hardware::camera::device::ICameraDevice;
using ::aidl::android::hardware::camera::provider::BnCameraProvider;
using ::aidl::android::hardware::camera::provider::CameraIdAndStreamCombination;
using ::aidl::android::hardware::camera::provider::ConcurrentCameraIdCombination;
using ::aidl::android::hardware::camera::provider::ICameraProviderCallback;
using ::android::hardware::camera::common::helper::SamsungCameraModule;

class CameraProvider : public BnCameraProvider, protected camera_module_callbacks_t {
  public:
    CameraProvider();
    ~CameraProvider() override;

    // Caller must use this method to check if CameraProvider ctor failed
    bool isInitFailed() { return mInitFailed; }

    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<ICameraProviderCallback>& in_callback) override;
    ndk::ScopedAStatus getVendorTags(std::vector<VendorTagSection>* _aidl_return) override;
    ndk::ScopedAStatus getCameraIdList(std::vector<std::string>* _aidl_return) override;
    ndk::ScopedAStatus getCameraDeviceInterface(
            const std::string& in_cameraDeviceName,
            std::shared_ptr<ICameraDevice>* _aidl_return) override;
    ndk::ScopedAStatus notifyDeviceStateChange(int64_t in_deviceState) override;
    ndk::ScopedAStatus getConcurrentCameraIds(
            std::vector<ConcurrentCameraIdCombination>* concurrent_camera_ids) override;
    ndk::ScopedAStatus isConcurrentStreamCombinationSupported(
            const std::vector<CameraIdAndStreamCombination>& in_configs,
            bool* support) override;

  protected:
    Mutex mCbLock;
    std::shared_ptr<ICameraProviderCallback> mCallbacks = nullptr;

    sp<SamsungCameraModule> mModule;

    int mNumberOfLegacyCameras;
    std::map<std::string, camera_device_status_t> mCameraStatusMap;  // camera id -> status
    SortedVector<std::string> mCameraIds;  // the "0"/"1" legacy camera Ids
    // (cameraId string, hidl device name) pairs
    SortedVector<std::pair<std::string, std::string>> mCameraDeviceNames;

    // Must be queried before using any APIs.
    // APIs will only work when this returns true
    bool mInitFailed;
    bool initCamera(int id);
    bool initialize();

    std::vector<VendorTagSection> mVendorTagSections;
    bool setUpVendorTags();
    int checkCameraVersion(int id, camera_info info);

    // create AIDL device name from camera ID
    std::string getAidlDeviceName(std::string cameraId);

    // static callback forwarding methods
    static void sCameraDeviceStatusChange(const struct camera_module_callbacks* callbacks,
                                          int camera_id, int new_status);
    static void sTorchModeStatusChange(const struct camera_module_callbacks* callbacks,
                                       const char* camera_id, int new_status);

    void addDeviceNames(int camera_id, CameraDeviceStatus status = CameraDeviceStatus::PRESENT,
                        bool cam_new = false);
    void removeDeviceNames(int camera_id);
};

}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
