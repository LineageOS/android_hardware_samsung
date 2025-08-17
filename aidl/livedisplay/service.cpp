/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay-service.samsung-qcom"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <livedisplay/samsung/AdaptiveBacklight.h>
#include <livedisplay/samsung/DisplayColorCalibration.h>
#include <livedisplay/samsung/DisplayModes.h>
#include <livedisplay/samsung/SunlightEnhancement.h>

using ::aidl::vendor::lineage::livedisplay::samsung::AdaptiveBacklight;
using ::aidl::vendor::lineage::livedisplay::samsung::DisplayColorCalibration;
using ::aidl::vendor::lineage::livedisplay::samsung::DisplayModes;
using ::aidl::vendor::lineage::livedisplay::samsung::SunlightEnhancement;

int main() {
    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::ProcessState::self()->startThreadPool();

    std::shared_ptr<AdaptiveBacklight> adaptiveBacklight =
            ndk::SharedRefBase::make<AdaptiveBacklight>();
    std::shared_ptr<DisplayColorCalibration> displayColorCalibration =
            ndk::SharedRefBase::make<DisplayColorCalibration>();
    std::shared_ptr<DisplayModes> displayModes = ndk::SharedRefBase::make<DisplayModes>();
    std::shared_ptr<SunlightEnhancement> sunlightEnhancement =
            ndk::SharedRefBase::make<SunlightEnhancement>();
    binder_status_t status;

    LOG(INFO) << "LiveDisplay HAL service is starting.";

    if (adaptiveBacklight == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL AdaptiveBacklight Iface, "
                      "exiting.";
        goto shutdown;
    }

    if (displayColorCalibration == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayColorCalibration "
                      "Iface, exiting.";
        goto shutdown;
    }

    if (displayModes == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayModes Iface, exiting.";
        goto shutdown;
    }

    if (sunlightEnhancement == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL SunlightEnhancement Iface, "
                      "exiting.";
        goto shutdown;
    }

    if (adaptiveBacklight->isSupported()) {
        std::string instance = std::string(AdaptiveBacklight::descriptor) + "/default";
        status = AServiceManager_addService(adaptiveBacklight->asBinder().get(), instance.c_str());
        if (status != STATUS_OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL AdaptiveBacklight Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (displayColorCalibration->isSupported()) {
        std::string instance = std::string(DisplayColorCalibration::descriptor) + "/default";
        status = AServiceManager_addService(displayColorCalibration->asBinder().get(),
                                            instance.c_str());
        if (status != STATUS_OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL DisplayColorCalibration "
                          "Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (displayModes->isSupported()) {
        std::string instance = std::string(DisplayModes::descriptor) + "/default";
        status = AServiceManager_addService(displayModes->asBinder().get(), instance.c_str());
        if (status != STATUS_OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL DisplayModes Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (sunlightEnhancement->isSupported()) {
        std::string instance = std::string(SunlightEnhancement::descriptor) + "/default";
        status =
                AServiceManager_addService(sunlightEnhancement->asBinder().get(), instance.c_str());
        if (status != STATUS_OK) {
            LOG(ERROR)
                    << "Could not register service for LiveDisplay HAL SunlightEnhancement Iface ("
                    << status << ")";
            goto shutdown;
        }
    }

    LOG(INFO) << "LiveDisplay HAL service is ready.";
    ABinderProcess_joinThreadPool();

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "LiveDisplay HAL service is shutting down.";
    return EXIT_FAILURE;
}
