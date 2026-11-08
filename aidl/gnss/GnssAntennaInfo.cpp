/*
 * Copyright (C) 2021 The Android Open Source Project
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

#define LOG_TAG "GnssAntennaInfoAidl"

#include "GnssAntennaInfo.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <log/log.h>
#include "Utils.h"

namespace aidl::android::hardware::gnss {

using namespace ::android::hardware::gnss;
using Row = IGnssAntennaInfoCallback::Row;
using Coord = IGnssAntennaInfoCallback::Coord;

std::shared_ptr<IGnssAntennaInfoCallback> GnssAntennaInfo::sCallback = nullptr;

GnssAntennaInfo::GnssAntennaInfo() : mMinIntervalMs(1000) {}

GnssAntennaInfo::~GnssAntennaInfo() {
    stop();
}

// Methods from ::android::hardware::gnss::V2_1::IGnssAntennaInfo follow.
ndk::ScopedAStatus GnssAntennaInfo::setCallback(
        const std::shared_ptr<IGnssAntennaInfoCallback>& callback) {
    ALOGD("setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;

    if (mIsActive) {
        ALOGW("GnssAntennaInfo callback already set. Resetting the callback...");
        stop();
    }
    start();

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssAntennaInfo::close() {
    ALOGD("close");
    stop();
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = nullptr;
    return ndk::ScopedAStatus::ok();
}

void GnssAntennaInfo::start() {
    ALOGD("start");
    mIsActive = true;
    mThread = std::thread([this]() {
        while (mIsActive == true) {
            if (sCallback != nullptr) {
                IGnssAntennaInfoCallback::GnssAntennaInfo mockAntennaInfo_1 = {
                        .carrierFrequencyHz = 1575420000,
                        .phaseCenterOffsetCoordinateMillimeters = Coord{.x = 1,
                                                                        .xUncertainty = 0.1,
                                                                        .y = 2,
                                                                        .yUncertainty = 0.1,
                                                                        .z = 3,
                                                                        .zUncertainty = 0.1},
                        .phaseCenterVariationCorrectionMillimeters =
                                {
                                        Row{std::vector<double>{1, -1, 5, -2, 3, -1}},
                                        Row{std::vector<double>{-2, 3, 2, 0, 1, 2}},
                                        Row{std::vector<double>{1, 3, 2, -1, -3, 5}},
                                },
                        .phaseCenterVariationCorrectionUncertaintyMillimeters =
                                {
                                        Row{std::vector<double>{0.1, 0.2, 0.4, 0.1, 0.2, 0.3}},
                                        Row{std::vector<double>{0.3, 0.2, 0.3, 0.6, 0.1, 0.1}},
                                        Row{std::vector<double>{0.1, 0.1, 0.4, 0.2, 0.5, 0.3}},
                                },
                        .signalGainCorrectionDbi =
                                {
                                        Row{std::vector<double>{2, -3, 1, -3, 0, -4}},
                                        Row{std::vector<double>{1, 0, -4, 1, 3, -2}},
                                        Row{std::vector<double>{3, -2, 0, -2, 3, 0}},
                                },
                        .signalGainCorrectionUncertaintyDbi =
                                {
                                        Row{std::vector<double>{0.3, 0.1, 0.2, 0.6, 0.1, 0.3}},
                                        Row{std::vector<double>{0.1, 0.1, 0.5, 0.2, 0.3, 0.1}},
                                        Row{std::vector<double>{0.2, 0.4, 0.2, 0.1, 0.1, 0.2}},
                                },
                };

                IGnssAntennaInfoCallback::GnssAntennaInfo mockAntennaInfo_2 = {
                        .carrierFrequencyHz = 1176450000,
                        .phaseCenterOffsetCoordinateMillimeters = Coord{.x = 5,
                                                                        .xUncertainty = 0.1,
                                                                        .y = 6,
                                                                        .yUncertainty = 0.1,
                                                                        .z = 7,
                                                                        .zUncertainty = 0.1},
                };

                std::vector<IGnssAntennaInfoCallback::GnssAntennaInfo> mockAntennaInfos = {
                        mockAntennaInfo_1,
                        mockAntennaInfo_2,
                };
                this->reportAntennaInfo(mockAntennaInfos);
            }

            /** For mock implementation this is good. On real device, we should only report
                antennaInfo at start and when there is a configuration change. **/
            std::this_thread::sleep_for(std::chrono::milliseconds(mMinIntervalMs));
        }
    });
}

void GnssAntennaInfo::stop() {
    ALOGD("stop");
    mIsActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
}

void GnssAntennaInfo::reportAntennaInfo(
        const std::vector<IGnssAntennaInfoCallback::GnssAntennaInfo>& antennaInfo) const {
    std::unique_lock<std::mutex> lock(mMutex);

    if (sCallback == nullptr) {
        ALOGE("%s: No non-null callback", __func__);
        return;
    }

    auto ret = sCallback->gnssAntennaInfoCb(antennaInfo);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
}

}  // namespace aidl::android::hardware::gnss
