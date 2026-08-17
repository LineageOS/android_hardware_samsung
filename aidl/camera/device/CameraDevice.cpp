/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "CamDev-impl"

#include "CameraDevice.h"
#include "convert.h"

#include <cutils/trace.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace implementation {

std::string CameraDevice::kDeviceVersion = "1.1";

CameraDevice::CameraDevice(
        sp<SamsungCameraModule> module, const std::string& cameraId,
        const SortedVector<std::pair<std::string, std::string>>& cameraDeviceNames)
    : mModule(module),
      mCameraId(cameraId),
      mDisconnected(false),
      mCameraDeviceNames(cameraDeviceNames) {
    mCameraIdInt = atoi(mCameraId.c_str());
    // Should not reach here as provider also validate ID
    if (mCameraIdInt < 0) {
        ALOGE("%s: Invalid camera id: %s", __FUNCTION__, mCameraId.c_str());
        mInitFail = true;
    } else if (mCameraIdInt >= mModule->getNumberOfCameras()) {
        ALOGI("%s: Adding a new camera id: %s", __FUNCTION__, mCameraId.c_str());
    }

    mDeviceVersion = mModule->getDeviceVersion(mCameraIdInt);
    if (mDeviceVersion < CAMERA_DEVICE_API_VERSION_3_2) {
        ALOGE("%s: Camera id %s does not support HAL3.2+", __FUNCTION__, mCameraId.c_str());
        mInitFail = true;
    }
}

CameraDevice::~CameraDevice() {}

Status CameraDevice::initStatus() const {
    Mutex::Autolock _l(mLock);
    Status status = Status::OK;
    if (mInitFail) {
        status = Status::INTERNAL_ERROR;
    } else if (mDisconnected) {
        status = Status::CAMERA_DISCONNECTED;
    }
    return status;
}

ndk::ScopedAStatus CameraDevice::getCameraCharacteristics(CameraMetadata* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    Status status = initStatus();
    if (status == Status::OK) {
        // Module 2.1+ codepath.
        struct camera_info info;
        int ret = mModule->getCameraInfo(mCameraIdInt, &info);
        if (ret == OK && info.static_camera_characteristics != NULL) {
            common::helper::CameraMetadata metadata =
                    (camera_metadata_t*)info.static_camera_characteristics;
            camera_metadata_entry_t entry = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);
            if (entry.count > 0 && *entry.data.u8 != 0 &&
                mModule->isSetTorchModeStrengthSupported()) {
                // Samsung always has 5 supported torch strength levels
                int32_t defaultTorchStrength = 1;
                int32_t torchStrengthLevels = 5;
                metadata.update(ANDROID_FLASH_INFO_STRENGTH_DEFAULT_LEVEL, &defaultTorchStrength,
                                1);
                metadata.update(ANDROID_FLASH_INFO_STRENGTH_MAXIMUM_LEVEL, &torchStrengthLevels, 1);
            }

            convertToAidl(metadata.release(), _aidl_return);
        } else {
            ALOGE("%s: get camera info failed!", __FUNCTION__);
            status = Status::INTERNAL_ERROR;
        }
    }

    return fromStatus(status);
}

ndk::ScopedAStatus CameraDevice::getPhysicalCameraCharacteristics(
        const std::string& in_physicalCameraId, CameraMetadata* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    Status status = initStatus();
    if (status == Status::OK) {
        // Require module 2.5+ version.
        if (mModule->getModuleApiVersion() < CAMERA_MODULE_API_VERSION_2_5) {
            ALOGE("%s: get_physical_camera_info must be called on camera module 2.5 or newer",
                  __FUNCTION__);
            status = Status::INTERNAL_ERROR;
        } else {
            char* end;
            errno = 0;
            long id = strtol(in_physicalCameraId.c_str(), &end, 0);
            if (id > INT_MAX || (errno == ERANGE && id == LONG_MAX) || id < INT_MIN ||
                (errno == ERANGE && id == LONG_MIN) || *end != '\0') {
                ALOGE("%s: Invalid physicalCameraId %s", __FUNCTION__, in_physicalCameraId.c_str());
                status = Status::ILLEGAL_ARGUMENT;
            } else {
                camera_metadata_t* physicalInfo = nullptr;
                int ret = mModule->getPhysicalCameraInfo((int)id, &physicalInfo);
                if (ret == OK) {
                    convertToAidl(physicalInfo, _aidl_return);
                } else if (ret == -EINVAL) {
                    ALOGE("%s: %s is not a valid physical camera Id outside of getCameraIdList()",
                          __FUNCTION__, in_physicalCameraId.c_str());
                    status = Status::ILLEGAL_ARGUMENT;
                } else {
                    ALOGE("%s: Failed to get physical camera %s info: %s (%d)!", __FUNCTION__,
                          in_physicalCameraId.c_str(), strerror(-ret), ret);
                    status = Status::INTERNAL_ERROR;
                }
            }
        }
    }

    return fromStatus(status);
}

void CameraDevice::setConnectionStatus(bool connected) {
    Mutex::Autolock _l(mLock);
    mDisconnected = !connected;
    std::shared_ptr<CameraDeviceSession> session = mSession.lock();
    if (session == nullptr) {
        return;
    }
    // Only notify active session disconnect events.
    // Users will need to re-open camera after disconnect event
    if (!connected) {
        session->disconnect();
    }
    return;
}

Status CameraDevice::getAidlStatus(int status) {
    switch (status) {
        case 0:
            return Status::OK;
        case -ENOSYS:
            return Status::OPERATION_NOT_SUPPORTED;
        case -EBUSY:
            return Status::CAMERA_IN_USE;
        case -EUSERS:
            return Status::MAX_CAMERAS_IN_USE;
        case -ENODEV:
            return Status::INTERNAL_ERROR;
        case -EINVAL:
            return Status::ILLEGAL_ARGUMENT;
        default:
            ALOGE("%s: unknown HAL status code %d", __FUNCTION__, status);
            return Status::INTERNAL_ERROR;
    }
}

ndk::ScopedAStatus CameraDevice::getResourceCost(CameraResourceCost* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    Status status = initStatus();
    if (status == Status::OK) {
        int cost = 100;
        std::vector<std::string> conflicting_devices;
        struct camera_info info;

        // If using post-2.4 module version, query the cost + conflicting devices from the HAL
        if (mModule->getModuleApiVersion() >= CAMERA_MODULE_API_VERSION_2_4) {
            int ret = mModule->getCameraInfo(mCameraIdInt, &info);
            if (ret == OK) {
                cost = info.resource_cost;
                for (size_t i = 0; i < info.conflicting_devices_length; i++) {
                    std::string cameraId(info.conflicting_devices[i]);
                    for (const auto& pair : mCameraDeviceNames) {
                        if (cameraId == pair.first) {
                            conflicting_devices.push_back(pair.second);
                        }
                    }
                }
            } else {
                status = Status::INTERNAL_ERROR;
            }
        }

        if (status == Status::OK) {
            _aidl_return->resourceCost = cost;
            _aidl_return->conflictingDevices.resize(conflicting_devices.size());
            for (size_t i = 0; i < conflicting_devices.size(); i++) {
                _aidl_return->conflictingDevices[i] = conflicting_devices[i];
                ALOGV("CamDevice %s is conflicting with camDevice %s", mCameraId.c_str(),
                      _aidl_return->conflictingDevices[i].c_str());
            }
        }
    }

    return fromStatus(status);
}

ndk::ScopedAStatus CameraDevice::isStreamCombinationSupported(const StreamConfiguration& in_streams,
                                                              bool* _aidl_return) {
    Status status;
    *_aidl_return = false;

    // Require module 2.5+ version.
    if (mModule->getModuleApiVersion() < CAMERA_MODULE_API_VERSION_2_5) {
        ALOGE("%s: is_stream_combination_supported must be called on camera module 2.5 or "
              "newer",
              __FUNCTION__);
        status = Status::INTERNAL_ERROR;
    } else {
        camera_stream_combination_t streamComb{};
        streamComb.operation_mode = static_cast<uint32_t>(in_streams.operationMode);
        streamComb.num_streams = in_streams.streams.size();
        camera_stream_t* streamBuffer = new camera_stream_t[streamComb.num_streams];

        size_t i = 0;
        for (const auto& it : in_streams.streams) {
            streamBuffer[i].stream_type = static_cast<int>(it.streamType);
            streamBuffer[i].width = it.width;
            streamBuffer[i].height = it.height;
            streamBuffer[i].format = static_cast<int>(it.format);
            streamBuffer[i].data_space = static_cast<android_dataspace_t>(it.dataSpace);
            streamBuffer[i].usage = static_cast<uint32_t>(it.usage);
            streamBuffer[i].physical_camera_id = it.physicalCameraId.c_str();
            streamBuffer[i++].rotation = static_cast<int>(it.rotation);
        }
        streamComb.streams = streamBuffer;
        auto res = mModule->isStreamCombinationSupported(mCameraIdInt, &streamComb);
        switch (res) {
            case NO_ERROR:
                *_aidl_return = true;
                status = Status::OK;
                break;
            case BAD_VALUE:
                status = Status::OK;
                break;
            case INVALID_OPERATION:
                status = Status::OPERATION_NOT_SUPPORTED;
                break;
            default:
                ALOGE("%s: Unexpected error: %d", __FUNCTION__, res);
                status = Status::INTERNAL_ERROR;
        };
        delete[] streamBuffer;
    }

    return fromStatus(status);
}

ndk::ScopedAStatus CameraDevice::open(const std::shared_ptr<ICameraDeviceCallback>& in_callback,
                                      std::shared_ptr<ICameraDeviceSession>* _aidl_return) {
    if (_aidl_return == nullptr) {
        ALOGE("%s: cannot open camera %s. return session ptr is null!", __FUNCTION__,
              mCameraId.c_str());
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    Status status = initStatus();
    std::shared_ptr<CameraDeviceSession> session = nullptr;

    if (in_callback == nullptr) {
        ALOGE("%s: cannot open camera %s. callback is null!", __FUNCTION__, mCameraId.c_str());
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    if (status != Status::OK) {
        // Provider will never pass initFailed device to client, so
        // this must be a disconnected camera
        ALOGE("%s: cannot open camera %s. camera is disconnected!", __FUNCTION__,
              mCameraId.c_str());
        return fromStatus(Status::CAMERA_DISCONNECTED);
    } else {
        mLock.lock();

        ALOGV("%s: Initializing device for camera %d", __FUNCTION__, mCameraIdInt);
        session = mSession.lock();
        if (session != nullptr && !session->isClosed()) {
            ALOGE("%s: cannot open an already opened camera!", __FUNCTION__);
            mLock.unlock();
            return fromStatus(Status::CAMERA_IN_USE);
        }

        /** Open HAL device */
        status_t res;
        camera3_device_t* device;

        ATRACE_BEGIN("camera3->open");
        res = mModule->open(mCameraId.c_str(), reinterpret_cast<hw_device_t**>(&device));
        ATRACE_END();

        if (res != OK) {
            ALOGE("%s: cannot open camera %s!", __FUNCTION__, mCameraId.c_str());
            mLock.unlock();
            return fromStatus(getAidlStatus(res));
        }

        /** Cross-check device version */
        if (device->common.version < CAMERA_DEVICE_API_VERSION_3_2) {
            ALOGE("%s: Could not open camera: "
                  "Camera device should be at least %x, reports %x instead",
                  __FUNCTION__, CAMERA_DEVICE_API_VERSION_3_2, device->common.version);
            device->common.close(&device->common);
            mLock.unlock();
            return fromStatus(Status::ILLEGAL_ARGUMENT);
        }

        struct camera_info info;
        res = mModule->getCameraInfo(mCameraIdInt, &info);
        if (res != OK) {
            ALOGE("%s: Could not open camera: getCameraInfo failed", __FUNCTION__);
            device->common.close(&device->common);
            mLock.unlock();
            return fromStatus(Status::ILLEGAL_ARGUMENT);
        }

        session = createSession(device, info.static_camera_characteristics, in_callback);
        if (session == nullptr) {
            ALOGE("%s: camera device session allocation failed", __FUNCTION__);
            mLock.unlock();
            return fromStatus(Status::INTERNAL_ERROR);
        }
        if (session->isInitFailed()) {
            ALOGE("%s: camera device session init failed", __FUNCTION__);
            session = nullptr;
            mLock.unlock();
            return fromStatus(Status::INTERNAL_ERROR);
        }
        mSession = session;

        mLock.unlock();
    }

    *_aidl_return = session;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraDevice::openInjectionSession(const std::shared_ptr<ICameraDeviceCallback>&,
                                                      std::shared_ptr<ICameraInjectionSession>*) {
    return fromStatus(Status::OPERATION_NOT_SUPPORTED);
}

ndk::ScopedAStatus CameraDevice::setTorchMode(bool in_on) {
    Status status;
    if (in_on) {
        status = static_cast<Status>(
                turnOnTorchWithStrengthLevel(mTorchStrengthLevel).getServiceSpecificError());
        if (status == Status::OPERATION_NOT_SUPPORTED) {
            goto fallback;
        }
    } else {
    fallback:
        if (!mModule->isSetTorchModeSupported()) {
            return fromStatus(Status::OPERATION_NOT_SUPPORTED);
        }

        status = initStatus();
        if (status == Status::OK) {
            status = getAidlStatus(mModule->setTorchMode(mCameraId.c_str(), in_on));
            if (status == Status::OK) {
                mTorchStrengthLevel = 1;
                status = getAidlStatus(mModule->setTorchMode(mCameraId.c_str(), in_on));
            }
        }
    }
    return fromStatus(status);
}

ndk::ScopedAStatus CameraDevice::turnOnTorchWithStrengthLevel(int32_t in_torchStrength) {
    if (!mModule->isSetTorchModeStrengthSupported()) {
        return fromStatus(Status::OPERATION_NOT_SUPPORTED);
    }

    Status status = initStatus();
    if (status == Status::OK) {
        status = getAidlStatus(
                mModule->setTorchModeStrength(mCameraId.c_str(), true, in_torchStrength));
        if (status == Status::OK) {
            mTorchStrengthLevel = in_torchStrength;
        }
    }
    return fromStatus(status);
}

ndk::ScopedAStatus CameraDevice::getTorchStrengthLevel(int32_t* _aidl_return) {
    if (!mModule->isSetTorchModeSupported()) {
        return fromStatus(Status::OPERATION_NOT_SUPPORTED);
    }

    *_aidl_return = mTorchStrengthLevel;
    return fromStatus(Status::OK);
}

std::shared_ptr<CameraDeviceSession> CameraDevice::createSession(
        camera3_device_t* device, const camera_metadata_t* deviceInfo,
        const std::shared_ptr<ICameraDeviceCallback>& callback) {
    return ndk::SharedRefBase::make<CameraDeviceSession>(device, deviceInfo, callback);
}

binder_status_t CameraDevice::dump(int fd, const char** args, uint32_t numArgs) {
    std::shared_ptr<CameraDeviceSession> session = mSession.lock();
    if (session == nullptr) {
        dprintf(fd, "No active camera device session instance\n");
        return STATUS_OK;
    }

    return session->dump(fd, args, numArgs);
}

}  // namespace implementation
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
