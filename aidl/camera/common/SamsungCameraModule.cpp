/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "CamComm1.0-SamsungCamModule"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include <SamsungCameraModule.h>
#include <utils/Trace.h>

namespace android {
namespace hardware {
namespace camera {
namespace common {
namespace helper {

SamsungCameraModule::SamsungCameraModule(camera_module_t* module)
    : CameraModule(module), mModule(module) {}

SamsungCameraModule::~SamsungCameraModule() {}

int SamsungCameraModule::getCameraDeviceVersion(int cameraId, uint32_t* version) {
    ATRACE_CALL();
    int ret;
    if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_5 &&
            mModule->get_camera_device_version != NULL) {
        ret = mModule->get_camera_device_version(cameraId, version);
    } else {
        struct camera_info info;
        ret = getCameraInfo(cameraId, &info);
        if (ret == OK) {
            *version = info.device_version;
        }
    }
    return ret;
}

int SamsungCameraModule::sehGetDeviceVersion(int cameraId) {
    ssize_t index = mDeviceVersionMap.indexOfKey(cameraId);
    if (index == NAME_NOT_FOUND) {
        uint32_t deviceVersion;
        if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_0) {
            getCameraDeviceVersion(cameraId, &deviceVersion);
        } else {
            deviceVersion = CAMERA_DEVICE_API_VERSION_1_0;
        }
        index = mDeviceVersionMap.add(cameraId, deviceVersion);
    }
    assert(index != NAME_NOT_FOUND);
    return mDeviceVersionMap[index];
}

int SamsungCameraModule::getConcurrentStreamingCameraIds(uint32_t *pConcCamArrayLength,
                                                  concurrent_camera_combination_t**  ppConcCamArray) {
    int res = INVALID_OPERATION;
    if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_5 &&
        mModule->get_concurrent_streaming_camera_ids != NULL) {
        ATRACE_BEGIN("camera_module->get_concurrent_streaming_camera_ids");
        res = mModule->get_concurrent_streaming_camera_ids(pConcCamArrayLength, ppConcCamArray);
        ATRACE_END();
    }
    return res;
}

int SamsungCameraModule::isConcurrentStreamCombinationSupported(const std::vector<cameraid_stream_combination_t>& rCameraIdStreamComboVec)
{
    int res = INVALID_OPERATION;
    if (getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_5 &&
        mModule->is_concurrent_stream_combination_supported != NULL) {
        ATRACE_BEGIN("camera_module->is_concurrent_stream_combination_supported");
        res = mModule->is_concurrent_stream_combination_supported(rCameraIdStreamComboVec.size(), rCameraIdStreamComboVec.data());
        ATRACE_END();
    }
    return res;
}

bool SamsungCameraModule::isSetTorchModeStrengthSupported() {
    return isSetTorchModeSupported() && mModule->set_torch_mode_strength != NULL;
}

int SamsungCameraModule::setTorchModeStrength(const char* camera_id, bool enable, int strength) {
    int res = INVALID_OPERATION;
    if (mModule->set_torch_mode_strength != NULL) {
        ATRACE_BEGIN("camera_module->set_torch_mode_strength");
        res = mModule->set_torch_mode_strength(camera_id, enable, strength);
        ATRACE_END();
    }
    return res;
}

}  // namespace helper
}  // namespace common
}  // namespace camera
}  // namespace hardware
}  // namespace android
