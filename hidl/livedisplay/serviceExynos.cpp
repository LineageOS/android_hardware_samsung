/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef LIVES_IN_SYSTEM
#define LOG_TAG "lineage.livedisplay@2.0-service.samsung-exynos"
#else
#define LOG_TAG "vendor.lineage.livedisplay@2.0-service.samsung-exynos"
#endif

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

#include "AdaptiveBacklight.h"
#include "DisplayColorCalibrationExynos.h"
#include "DisplayModes.h"
#include "ReadingEnhancement.h"
#include "SunlightEnhancementExynos.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;
using android::status_t;
using android::OK;

using vendor::lineage::livedisplay::V2_0::samsung::AdaptiveBacklight;
using vendor::lineage::livedisplay::V2_0::samsung::DisplayColorCalibrationExynos;
using vendor::lineage::livedisplay::V2_0::samsung::DisplayModes;
using vendor::lineage::livedisplay::V2_0::samsung::ReadingEnhancement;
using vendor::lineage::livedisplay::V2_0::samsung::SunlightEnhancementExynos;

int main() {
    sp<AdaptiveBacklight> adaptiveBacklight;
    sp<DisplayColorCalibrationExynos> displayColorCalibrationExynos;
    sp<DisplayModes> displayModes;
    sp<ReadingEnhancement> readingEnhancement;
    sp<SunlightEnhancementExynos> sunlightEnhancementExynos;
    status_t status;

    LOG(INFO) << "LiveDisplay HAL service is starting.";

    adaptiveBacklight = new AdaptiveBacklight();
    if (adaptiveBacklight == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL AdaptiveBacklight Iface, exiting.";
        goto shutdown;
    }

    displayColorCalibrationExynos = new DisplayColorCalibrationExynos();
    if (displayColorCalibrationExynos == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayColorCalibration "
                      "Iface, exiting.";
        goto shutdown;
    }

    displayModes = new DisplayModes();
    if (displayModes == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayModes Iface, exiting.";
        goto shutdown;
    }

    readingEnhancement = new ReadingEnhancement();
    if (readingEnhancement == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL ReadingEnhancement Iface, exiting.";
        goto shutdown;
    }

    sunlightEnhancementExynos = new SunlightEnhancementExynos();
    if (sunlightEnhancementExynos == nullptr) {
        LOG(ERROR)
            << "Can not create an instance of LiveDisplay HAL SunlightEnhancement Iface, exiting.";
        goto shutdown;
    }

    configureRpcThreadpool(1, true /*callerWillJoin*/);

    if (adaptiveBacklight->isSupported()) {
        status = adaptiveBacklight->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL AdaptiveBacklight Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (displayColorCalibrationExynos->isSupported()) {
        status = displayColorCalibrationExynos->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL DisplayColorCalibration Iface ("
                << status << ")";
            goto shutdown;
        }
    }

    if (displayModes->isSupported()) {
        status = displayModes->registerAsService();
        if (status != OK) {
            LOG(ERROR) << "Could not register service for LiveDisplay HAL DisplayModes Iface ("
                       << status << ")";
            goto shutdown;
        }
    }

    if (readingEnhancement->isSupported()) {
        status = readingEnhancement->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL ReadingEnhancement Iface ("
                << status << ")";
            goto shutdown;
        }
    }

    if (sunlightEnhancementExynos->isSupported()) {
        status = sunlightEnhancementExynos->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL SunlightEnhancement Iface ("
                << status << ")";
            goto shutdown;
        }
    }

    LOG(INFO) << "LiveDisplay HAL service is ready.";
    joinRpcThreadpool();
// Should not pass this line

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "LiveDisplay HAL service is shutting down.";
    return 1;
}
