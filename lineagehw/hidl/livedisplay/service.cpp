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

#define LOG_TAG "vendor.lineage.livedisplay@2.0-service.samsung"

#include <android-base/logging.h>
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

#include "AdaptiveBacklight.h"
#include "DisplayColorCalibration.h"
#include "DisplayColorCalibrationExynos.h"
#include "DisplayModes.h"
#include "ReadingEnhancement.h"
#include "ReadingEnhancementExynos.h"
#include "SunlightEnhancement.h"
#include "SunlightEnhancementExynos.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;
using android::status_t;
using android::OK;

using vendor::lineage::livedisplay::V2_0::samsung::AdaptiveBacklight;
using vendor::lineage::livedisplay::V2_0::samsung::DisplayColorCalibration;
using vendor::lineage::livedisplay::V2_0::samsung::DisplayColorCalibrationExynos;
using vendor::lineage::livedisplay::V2_0::samsung::DisplayModes;
using vendor::lineage::livedisplay::V2_0::samsung::ReadingEnhancement;
using vendor::lineage::livedisplay::V2_0::samsung::ReadingEnhancementExynos;
using vendor::lineage::livedisplay::V2_0::samsung::SunlightEnhancement;
using vendor::lineage::livedisplay::V2_0::samsung::SunlightEnhancementExynos;

int main() {
    sp<AdaptiveBacklight> adaptiveBacklight;
    sp<DisplayColorCalibration> displayColorCalibration;
    sp<DisplayColorCalibrationExynos> displayColorCalibrationExynos;
    sp<DisplayModes> displayModes;
    sp<ReadingEnhancement> readingEnhancement;
    sp<ReadingEnhancementExynos> readingEnhancementExynos;
    sp<SunlightEnhancement> sunlightEnhancement;
    sp<SunlightEnhancementExynos> sunlightEnhancementExynos;
    status_t status;
    uint8_t services = 0;

    LOG(INFO) << "LiveDisplay HAL service is starting.";

    adaptiveBacklight = new AdaptiveBacklight();
    if (adaptiveBacklight == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL AdaptiveBacklight Iface, exiting.";
        goto shutdown;
    }
    if (adaptiveBacklight->isSupported()) {
        services++;
    }

    displayColorCalibration = new DisplayColorCalibration();
    if (displayColorCalibration == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayColorCalibration Iface, exiting.";
        goto shutdown;
    }
    if (displayColorCalibration->isSupported()) {
        services++;
    }

    displayColorCalibrationExynos = new DisplayColorCalibrationExynos();
    if (displayColorCalibrationExynos == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayColorCalibration Iface, exiting.";
        goto shutdown;
    }
    if (displayColorCalibrationExynos->isSupported()) {
        services++;
    }

    displayModes = new DisplayModes();
    if (displayModes == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL DisplayModes Iface, exiting.";
        goto shutdown;
    }
    if (displayModes->isSupported()) {
        services++;
    }

    readingEnhancement = new ReadingEnhancement();
    if (readingEnhancement == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL ReadingEnhancement Iface, exiting.";
        goto shutdown;
    }
    if (readingEnhancement->isSupported()) {
        services++;
    }

    readingEnhancementExynos = new ReadingEnhancementExynos();
    if (readingEnhancementExynos == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL ReadingEnhancement Iface, exiting.";
        goto shutdown;
    }
    if (readingEnhancementExynos->isSupported()) {
        services++;
    }

    sunlightEnhancement = new SunlightEnhancement();
    if (sunlightEnhancement == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL SunlightEnhancement Iface, exiting.";
        goto shutdown;
    }
    if (sunlightEnhancement->isSupported()) {
        services++;
    }

    sunlightEnhancementExynos = new SunlightEnhancementExynos();
    if (sunlightEnhancementExynos == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL SunlightEnhancement Iface, exiting.";
        goto shutdown;
    }
    if (sunlightEnhancementExynos->isSupported()) {
        services++;
    }

    configureRpcThreadpool(services, true /*callerWillJoin*/);

    if (adaptiveBacklight->isSupported()) {
        status = adaptiveBacklight->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL AdaptiveBacklight Iface ("
                << status << ")";
            goto shutdown;
        }
    }

    if (displayColorCalibration->isSupported()) {
        status = displayColorCalibration->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL DisplayColorCalibration Iface ("
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
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL DisplayModes Iface ("
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

    if (readingEnhancementExynos->isSupported()) {
        status = readingEnhancementExynos->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL ReadingEnhancement Iface ("
                << status << ")";
            goto shutdown;
        }
    }

    if (sunlightEnhancement->isSupported()) {
        status = sunlightEnhancement->registerAsService();
        if (status != OK) {
            LOG(ERROR)
                << "Could not register service for LiveDisplay HAL SunlightEnhancement Iface ("
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
