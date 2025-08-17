/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay-service.samsung-exynos"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>
#include <livedisplay/samsung/AdaptiveBacklight.h>
#include <livedisplay/samsung/DisplayColorCalibrationExynos.h>
#include <livedisplay/samsung/DisplayModes.h>
#include <livedisplay/samsung/ReadingEnhancement.h>
#include <livedisplay/samsung/SunlightEnhancementExynos.h>

using ::aidl::vendor::lineage::livedisplay::samsung::AdaptiveBacklight;
using ::aidl::vendor::lineage::livedisplay::samsung::DisplayColorCalibrationExynos;
using ::aidl::vendor::lineage::livedisplay::samsung::DisplayModes;
using ::aidl::vendor::lineage::livedisplay::samsung::ReadingEnhancement;
using ::aidl::vendor::lineage::livedisplay::samsung::SunlightEnhancementExynos;

int main() {
    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::ProcessState::self()->startThreadPool();

    std::shared_ptr<AdaptiveBacklight> adaptiveBacklight =
            ndk::SharedRefBase::make<AdaptiveBacklight>();
    std::shared_ptr<DisplayColorCalibrationExynos> displayColorCalibration =
            ndk::SharedRefBase::make<DisplayColorCalibrationExynos>();
    std::shared_ptr<DisplayModes> displayModes = ndk::SharedRefBase::make<DisplayModes>();
    std::shared_ptr<ReadingEnhancement> readingEnhancement =
            ndk::SharedRefBase::make<ReadingEnhancement>();
    std::shared_ptr<SunlightEnhancementExynos> sunlightEnhancement =
            ndk::SharedRefBase::make<SunlightEnhancementExynos>();
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

    if (readingEnhancement == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL ReadingEnhancement Iface, "
                      "exiting.";
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
        std::string instance = std::string(DisplayColorCalibrationExynos::descriptor) + "/default";
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

    if (readingEnhancement->isSupported()) {
        std::string instance = std::string(ReadingEnhancement::descriptor) + "/default";
        status = AServiceManager_addService(readingEnhancement->asBinder().get(), instance.c_str());
        if (status != STATUS_OK) {
            LOG(ERROR)
                    << "Could not register service for LiveDisplay HAL ReadingEnhancement Iface ("
                    << status << ")";
            goto shutdown;
        }
    }

    if (sunlightEnhancement->isSupported()) {
        std::string instance = std::string(SunlightEnhancementExynos::descriptor) + "/default";
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
