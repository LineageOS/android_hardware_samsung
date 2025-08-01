/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "CamPrvdr"

#include "CameraProvider.h"

#include <CameraDevice.h>
#include <convert.h>
#include <utils/Trace.h>

#include <regex>

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace implementation {

using ::aidl::android::hardware::camera::common::CameraMetadataType;
using ::aidl::android::hardware::camera::common::Status;
using ::aidl::android::hardware::camera::common::TorchModeStatus;
using ::aidl::android::hardware::camera::common::VendorTag;
using ::android::hardware::camera::common::helper::VendorTagDescriptor;
using ::android::hardware::camera::device::implementation::CameraDevice;
using ::android::hardware::camera::device::implementation::fromStatus;

namespace {
// "device@<version>/internal/<id>"
const std::regex kDeviceNameRE("device@([0-9]+\\.[0-9]+)/internal/(.+)");
const int kMaxCameraDeviceNameLen = 128;
const int kMaxCameraIdLen = 16;

bool matchDeviceName(const std::string& deviceName, std::string* deviceVersion,
                     std::string* cameraId) {
    std::string deviceNameStd(deviceName.c_str());
    std::smatch sm;
    if (std::regex_match(deviceNameStd, sm, kDeviceNameRE)) {
        if (deviceVersion != nullptr) {
            *deviceVersion = sm[1];
        }
        if (cameraId != nullptr) {
            *cameraId = sm[2];
        }
        return true;
    }
    return false;
}

}  // anonymous namespace

void CameraProvider::addDeviceNames(int camera_id, CameraDeviceStatus status, bool cam_new) {
    char cameraId[kMaxCameraIdLen];
    snprintf(cameraId, sizeof(cameraId), "%d", camera_id);
    std::string cameraIdStr(cameraId);

    mCameraIds.add(cameraIdStr);

    // initialize mCameraDeviceNames
    int deviceVersion = mModule->sehGetDeviceVersion(camera_id);
    auto deviceNamePair = std::make_pair(cameraIdStr, getAidlDeviceName(cameraIdStr));
    mCameraDeviceNames.add(deviceNamePair);
    if (cam_new) {
        mCallbacks->cameraDeviceStatusChange(deviceNamePair.second, status);
    }
}

void CameraProvider::removeDeviceNames(int camera_id) {
    std::string cameraIdStr = std::to_string(camera_id);

    mCameraIds.remove(cameraIdStr);

    int deviceVersion = mModule->sehGetDeviceVersion(camera_id);
    auto deviceNamePair = std::make_pair(cameraIdStr, getAidlDeviceName(cameraIdStr));
    mCameraDeviceNames.remove(deviceNamePair);
    mCallbacks->cameraDeviceStatusChange(deviceNamePair.second, CameraDeviceStatus::NOT_PRESENT);
    mModule->removeCamera(camera_id);
}

/**
 * static callback forwarding methods from HAL to instance
 */
void CameraProvider::sCameraDeviceStatusChange(const struct camera_module_callbacks* callbacks,
                                               int camera_id, int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(static_cast<const CameraProvider*>(callbacks));
    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    char cameraId[kMaxCameraIdLen];
    snprintf(cameraId, sizeof(cameraId), "%d", camera_id);
    std::string cameraIdStr(cameraId);
    cp->mCameraStatusMap[cameraIdStr] = (camera_device_status_t)new_status;

    if (cp->mCallbacks == nullptr) {
        // For camera connected before mCallbacks is set, the corresponding
        // addDeviceNames() would be called later in setCallbacks().
        return;
    }

    bool found = false;
    CameraDeviceStatus status = (CameraDeviceStatus)new_status;
    for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
        if (cameraIdStr.compare(deviceNamePair.first) == 0) {
            cp->mCallbacks->cameraDeviceStatusChange(deviceNamePair.second, status);
            found = true;
        }
    }

    switch (status) {
        case CameraDeviceStatus::PRESENT:
        case CameraDeviceStatus::ENUMERATING:
            if (!found) {
                cp->addDeviceNames(camera_id, status, true);
            }
            break;
        case CameraDeviceStatus::NOT_PRESENT:
            if (found) {
                cp->removeDeviceNames(camera_id);
            }
    }
}

void CameraProvider::sTorchModeStatusChange(const struct camera_module_callbacks* callbacks,
                                            const char* camera_id, int new_status) {
    CameraProvider* cp = const_cast<CameraProvider*>(static_cast<const CameraProvider*>(callbacks));

    if (cp == nullptr) {
        ALOGE("%s: callback ops is null", __FUNCTION__);
        return;
    }

    Mutex::Autolock _l(cp->mCbLock);
    if (cp->mCallbacks != nullptr) {
        std::string cameraIdStr(camera_id);
        TorchModeStatus status = (TorchModeStatus)new_status;
        for (auto const& deviceNamePair : cp->mCameraDeviceNames) {
            if (cameraIdStr.compare(deviceNamePair.first) == 0) {
                cp->mCallbacks->torchModeStatusChange(deviceNamePair.second, status);
            }
        }
    }
}

std::string CameraProvider::getAidlDeviceName(std::string cameraId) {
    return "device@" + CameraDevice::kDeviceVersion + "/internal/" + cameraId;
}

CameraProvider::CameraProvider()
    : camera_module_callbacks_t({sCameraDeviceStatusChange, sTorchModeStatusChange}) {
    mInitFailed = initialize();
}

CameraProvider::~CameraProvider() {}

bool CameraProvider::initCamera(int id) {
    struct camera_info info;
    auto rc = mModule->getCameraInfo(id, &info);
    if (rc != NO_ERROR) {
        ALOGE("%s: Camera info query failed!", __func__);
        return true;
    }

    if (checkCameraVersion(id, info) != OK) {
        ALOGE("%s: Camera version check failed!", __func__);
        return true;
    }

    char cameraId[kMaxCameraIdLen];
    snprintf(cameraId, sizeof(cameraId), "%d", id);
    std::string cameraIdStr(cameraId);
    mCameraStatusMap[cameraIdStr] = CAMERA_DEVICE_STATUS_PRESENT;

    addDeviceNames(id);

    return false;
}

bool CameraProvider::initialize() {
    camera_module_t* rawModule;
    int err = hw_get_module(SEC_CAMERA_HARDWARE_MODULE_ID, (const hw_module_t**)&rawModule);
    if (err < 0) {
        err = hw_get_module(CAMERA_HARDWARE_MODULE_ID, (const hw_module_t**)&rawModule);
        if (err < 0) {
            ALOGE("Could not load camera HAL module: %d (%s)", err, strerror(-err));
            return true;
        }
    }

    mModule = new SamsungCameraModule(rawModule);
    err = mModule->init();
    if (err != OK) {
        ALOGE("Could not initialize camera HAL module: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }
    ALOGI("Loaded \"%s\" camera module", mModule->getModuleName());

    // Setup vendor tags here so HAL can setup vendor keys in camera characteristics
    VendorTagDescriptor::clearGlobalVendorTagDescriptor();
    if (!setUpVendorTags()) {
        ALOGE("%s: Vendor tag setup failed, will not be available.", __FUNCTION__);
    }

    // Setup callback now because we are going to try openLegacy next
    err = mModule->setCallbacks(this);
    if (err != OK) {
        ALOGE("Could not set camera module callback: %d (%s)", err, strerror(-err));
        mModule.clear();
        return true;
    }

    mNumberOfLegacyCameras = mModule->getNumberOfCameras();
    for (int i = 0; i < mNumberOfLegacyCameras; i++) {
        if (initCamera(i)) {
            mModule.clear();
            return true;
        }
    }
    std::vector<int> extraIDs = {
#ifdef EXTRA_IDS
        EXTRA_IDS
#endif
    };
    for (int i : extraIDs) {
        if (initCamera(i)) {
            mModule.clear();
            return true;
        } else {
            mNumberOfLegacyCameras++;
        }
    }

    return false;  // mInitFailed
}

/**
 * Check that the device HAL version is still in supported.
 */
int CameraProvider::checkCameraVersion(int id, camera_info info) {
    if (mModule == nullptr) {
        return NO_INIT;
    }

    // device_version undefined in CAMERA_MODULE_API_VERSION_1_0,
    // All CAMERA_MODULE_API_VERSION_1_0 devices are backward-compatible
    uint16_t moduleVersion = mModule->getModuleApiVersion();
    if (moduleVersion >= CAMERA_MODULE_API_VERSION_2_0) {
        // Verify the device version is in the supported range
        switch (info.device_version) {
            case CAMERA_DEVICE_API_VERSION_1_0:
            case CAMERA_DEVICE_API_VERSION_3_2:
            case CAMERA_DEVICE_API_VERSION_3_3:
            case CAMERA_DEVICE_API_VERSION_3_4:
            case CAMERA_DEVICE_API_VERSION_3_5:
                // in support
                break;
            case CAMERA_DEVICE_API_VERSION_3_6:
                /**
                 * ICameraDevice@3.5 contains APIs from both
                 * CAMERA_DEVICE_API_VERSION_3_6 and CAMERA_MODULE_API_VERSION_2_5
                 * so we require HALs to uprev both for simplified supported combinations.
                 * HAL can still opt in individual new APIs indepedently.
                 */
                if (moduleVersion < CAMERA_MODULE_API_VERSION_2_5) {
                    ALOGE("%s: Device %d has unsupported version combination:"
                          "HAL version %x and module version %x",
                          __FUNCTION__, id, info.device_version, moduleVersion);
                    return NO_INIT;
                }
                break;
            case CAMERA_DEVICE_API_VERSION_2_0:
            case CAMERA_DEVICE_API_VERSION_2_1:
            case CAMERA_DEVICE_API_VERSION_3_0:
            case CAMERA_DEVICE_API_VERSION_3_1:
                // no longer supported
            default:
                ALOGE("%s: Device %d has HAL version %x, which is not supported", __FUNCTION__, id,
                      info.device_version);
                return NO_INIT;
        }
    }

    return OK;
}

bool CameraProvider::setUpVendorTags() {
    ATRACE_CALL();
    vendor_tag_ops_t vOps = vendor_tag_ops_t();

    // Check if vendor operations have been implemented
    if (!mModule->isVendorTagDefined()) {
        ALOGI("%s: No vendor tags defined for this device.", __FUNCTION__);
        return true;
    }

    mModule->getVendorTagOps(&vOps);

    // Ensure all vendor operations are present
    if (vOps.get_tag_count == nullptr || vOps.get_all_tags == nullptr ||
        vOps.get_section_name == nullptr || vOps.get_tag_name == nullptr ||
        vOps.get_tag_type == nullptr) {
        ALOGE("%s: Vendor tag operations not fully defined. Ignoring definitions.", __FUNCTION__);
        return false;
    }

    // Read all vendor tag definitions into a descriptor
    sp<VendorTagDescriptor> desc;
    status_t res;
    if ((res = VendorTagDescriptor::createDescriptorFromOps(&vOps, /*out*/ desc)) != OK) {
        ALOGE("%s: Could not generate descriptor from vendor tag operations,"
              "received error %s (%d). Camera clients will not be able to use"
              "vendor tags",
              __FUNCTION__, strerror(res), res);
        return false;
    }

    // Set the global descriptor to use with camera metadata
    VendorTagDescriptor::setAsGlobalVendorTagDescriptor(desc);
    const SortedVector<String8>* sectionNames = desc->getAllSectionNames();
    size_t numSections = sectionNames->size();
    std::vector<std::vector<VendorTag>> tagsBySection(numSections);
    int tagCount = desc->getTagCount();
    std::vector<uint32_t> tags(tagCount);
    desc->getTagArray(tags.data());
    for (int i = 0; i < tagCount; i++) {
        VendorTag vt;
        vt.tagId = tags[i];
        vt.tagName = desc->getTagName(tags[i]);
        vt.tagType = (CameraMetadataType)desc->getTagType(tags[i]);
        ssize_t sectionIdx = desc->getSectionIndex(tags[i]);
        tagsBySection[sectionIdx].push_back(vt);
    }
    mVendorTagSections.resize(numSections);
    for (size_t s = 0; s < numSections; s++) {
        mVendorTagSections[s].sectionName = (*sectionNames)[s].c_str();
        mVendorTagSections[s].tags = tagsBySection[s];
    }
    return true;
}

ndk::ScopedAStatus CameraProvider::setCallback(
        const std::shared_ptr<ICameraProviderCallback>& callback) {
    Mutex::Autolock _l(mCbLock);
    mCallbacks = callback;
    if (callback == nullptr) {
        return fromStatus(Status::OK);
    }
    // Add and report all presenting external cameras.
    for (auto const& statusPair : mCameraStatusMap) {
        int id = std::stoi(statusPair.first);
        auto status = static_cast<CameraDeviceStatus>(statusPair.second);
        if (id >= mNumberOfLegacyCameras && status != CameraDeviceStatus::NOT_PRESENT) {
            addDeviceNames(id, status, true);
        }
    }

    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraProvider::getVendorTags(std::vector<VendorTagSection>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    _aidl_return->insert(_aidl_return->end(), mVendorTagSections.begin(), mVendorTagSections.end());
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraProvider::getCameraIdList(std::vector<std::string>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    for (auto const& deviceNamePair : mCameraDeviceNames) {
        if (std::stoi(deviceNamePair.first) >= mNumberOfLegacyCameras) {
            // External camera devices must be reported through the device status change callback,
            // not in this list.
            continue;
        }
        if (mCameraStatusMap[deviceNamePair.first] == CAMERA_DEVICE_STATUS_PRESENT) {
            _aidl_return->push_back(deviceNamePair.second);
        }
    }
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraProvider::getCameraDeviceInterface(
        const std::string& in_cameraDeviceName, std::shared_ptr<ICameraDevice>* _aidl_return) {
    if (_aidl_return == nullptr) {
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    std::string cameraId, deviceVersion;
    bool match = matchDeviceName(in_cameraDeviceName, &deviceVersion, &cameraId);

    if (!match) {
        *_aidl_return = nullptr;
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }

    ssize_t index = mCameraDeviceNames.indexOf(std::make_pair(cameraId, in_cameraDeviceName));
    if (index == NAME_NOT_FOUND) {  // Either an illegal name or a device version mismatch
        Status status = Status::OK;
        ssize_t idx = mCameraIds.indexOf(cameraId);
        if (idx == NAME_NOT_FOUND) {
            ALOGE("%s: cannot find camera %s!", __FUNCTION__, cameraId.c_str());
            status = Status::ILLEGAL_ARGUMENT;
        } else {  // invalid version
            ALOGE("%s: camera device %s does not support version %s!", __FUNCTION__,
                  cameraId.c_str(), deviceVersion.c_str());
            status = Status::OPERATION_NOT_SUPPORTED;
        }
        return fromStatus(status);
    }

    ALOGV("Constructing camera device");
    std::shared_ptr<CameraDevice> deviceImpl =
            ndk::SharedRefBase::make<CameraDevice>(mModule, cameraId, mCameraDeviceNames);
    if (deviceImpl == nullptr || deviceImpl->isInitFailed()) {
        ALOGE("%s: camera device %s init failed!", __FUNCTION__, cameraId.c_str());
        *_aidl_return = nullptr;
        return fromStatus(Status::INTERNAL_ERROR);
    }

    IF_ALOGV() {
        int interfaceVersion;
        deviceImpl->getInterfaceVersion(&interfaceVersion);
        ALOGV("%s: device interface version: %d", __FUNCTION__, interfaceVersion);
    }

    *_aidl_return = deviceImpl;
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraProvider::notifyDeviceStateChange(int64_t newState) {
    ALOGD("%s: New device state: 0x%" PRIx64, __FUNCTION__, newState);
    uint64_t state = static_cast<uint64_t>(newState);
    mModule->notifyDeviceStateChange(state);
    return fromStatus(Status::OK);
}

ndk::ScopedAStatus CameraProvider::getConcurrentCameraIds(
        std::vector<ConcurrentCameraIdCombination>* concurrent_camera_ids) {
    if (concurrent_camera_ids == nullptr) {
        ALOGE("%s: concurrent_camera_ids is nullptr. ", __FUNCTION__);
        return fromStatus(Status::ILLEGAL_ARGUMENT);
    }
    concurrent_camera_ids->clear();

    concurrent_camera_combination_t *pConcCamArray = NULL;
    uint32_t concCamArrayLength = 0;
    Status status = Status::OPERATION_NOT_SUPPORTED;
    int res = mModule->getConcurrentStreamingCameraIds(&concCamArrayLength, &pConcCamArray);
    if ((OK == res) && (0 != concCamArrayLength) && (NULL != pConcCamArray))
    {
        (*concurrent_camera_ids).resize(concCamArrayLength);
        int camIdComboIndex = 0;
        for (int camIdComboIndex = 0; camIdComboIndex < concCamArrayLength; ++camIdComboIndex)
        {
            std::vector<std::string> aidlCombination(pConcCamArray[camIdComboIndex].numCameras);
            for (int camIndex = 0; camIndex < pConcCamArray[camIdComboIndex].numCameras; ++camIndex)
            {
                aidlCombination[camIndex] = std::to_string(pConcCamArray[camIdComboIndex].concurrentCamArray[camIndex]);
            }
            (*concurrent_camera_ids)[camIdComboIndex].combination = aidlCombination;
        }

        // Memory allocated by caller needs to be freed
        delete [] pConcCamArray;
        status = Status::OK;
    }

    ALOGI("%s called, number of concurrent combinations supported is %d", __FUNCTION__, concCamArrayLength);
    return fromStatus(status);
}

ndk::ScopedAStatus CameraProvider::isConcurrentStreamCombinationSupported(
        const std::vector<CameraIdAndStreamCombination>& in_configs,
        bool* support)
{
    Status status = Status::OK;
    bool queryStatus = false;
    size_t numEntries = in_configs.size();
    int res = NO_ERROR;

    std::vector<cameraid_stream_combination_t> cameraIdStreamComboVec;
    for (const auto &camIdStreamCombo : in_configs)
    {
        cameraid_stream_combination_t cameraIdStreamComboEntry = {};
        cameraIdStreamComboEntry.streamConfig = new camera_stream_combination_t;
        camera_stream_combination_t* pStreamComb = cameraIdStreamComboEntry.streamConfig;
        if (NULL == pStreamComb)
        {
            res = NO_MEMORY;
            break;
        }

        pStreamComb->operation_mode = static_cast<uint32_t> (camIdStreamCombo.streamConfiguration.operationMode);
        pStreamComb->num_streams = camIdStreamCombo.streamConfiguration.streams.size();
        pStreamComb->streams = new camera_stream_t[pStreamComb->num_streams];
        if (NULL == pStreamComb->streams)
        {
            delete pStreamComb;
            res = NO_MEMORY;
            break;
        }

        camera_stream_t* pStreamBuffer = pStreamComb->streams;
        size_t strmIndex = 0;
        for (const auto &it : camIdStreamCombo.streamConfiguration.streams)
        {
            pStreamBuffer[strmIndex].stream_type = static_cast<int> (it.streamType);
            pStreamBuffer[strmIndex].width = it.width;
            pStreamBuffer[strmIndex].height = it.height;
            pStreamBuffer[strmIndex].format = static_cast<int> (it.format);
            pStreamBuffer[strmIndex].data_space = static_cast<android_dataspace_t> (it.dataSpace);
            pStreamBuffer[strmIndex].usage = static_cast<uint32_t> (it.usage);
            pStreamBuffer[strmIndex].physical_camera_id = it.physicalCameraId.c_str();
            pStreamBuffer[strmIndex++].rotation = static_cast<int> (it.rotation);
        }
        cameraIdStreamComboVec.push_back(cameraIdStreamComboEntry);
    }

    if (NO_ERROR == res)
    {
        res = mModule->isConcurrentStreamCombinationSupported(cameraIdStreamComboVec);
    }

    switch (res)
    {
        case NO_ERROR:
            queryStatus = true;
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

    for (auto &camIdStreamCombo : cameraIdStreamComboVec)
    {
        delete [] camIdStreamCombo.streamConfig->streams;
        delete camIdStreamCombo.streamConfig;
    }

    *support = queryStatus;
    ALOGI("%s called, queryStatus %d, status %d, number of concurrent Cameras %zu", __FUNCTION__, queryStatus, status, numEntries);
    return fromStatus(status);
}

}  // namespace implementation
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android
