/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.touch-service.samsung"

#include "GloveMode.h"
#include "KeyDisabler.h"
#include "StylusMode.h"
#include "TouchscreenGesture.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::vendor::lineage::touch::GloveMode;
using aidl::vendor::lineage::touch::KeyDisabler;
using aidl::vendor::lineage::touch::StylusMode;
using aidl::vendor::lineage::touch::TouchscreenGesture;

int main() {
    binder_status_t status = STATUS_OK;

    ABinderProcess_setThreadPoolMaxThreadCount(0);

    std::shared_ptr<GloveMode> gm = ndk::SharedRefBase::make<GloveMode>();
    if (gm->isSupported()) {
        const std::string gm_instance = std::string(GloveMode::descriptor) + "/default";
        status = AServiceManager_addService(gm->asBinder().get(), gm_instance.c_str());
        CHECK_EQ(status, STATUS_OK) << "Failed to add service " << gm_instance << " " << status;
    }

    std::shared_ptr<KeyDisabler> kd = ndk::SharedRefBase::make<KeyDisabler>();
    if (kd->isSupported()) {
        const std::string kd_instance = std::string(KeyDisabler::descriptor) + "/default";
        status = AServiceManager_addService(kd->asBinder().get(), kd_instance.c_str());
        CHECK_EQ(status, STATUS_OK) << "Failed to add service " << kd_instance << " " << status;
    }

    std::shared_ptr<StylusMode> sm = ndk::SharedRefBase::make<StylusMode>();
    if (sm->isSupported()) {
        const std::string sm_instance = std::string(StylusMode::descriptor) + "/default";
        status = AServiceManager_addService(sm->asBinder().get(), sm_instance.c_str());
        CHECK_EQ(status, STATUS_OK) << "Failed to add service " << sm_instance << " " << status;
    }

    std::shared_ptr<TouchscreenGesture> tg = ndk::SharedRefBase::make<TouchscreenGesture>();
    if (tg->isSupported()) {
        const std::string tg_instance = std::string(TouchscreenGesture::descriptor) + "/default";
        status = AServiceManager_addService(tg->asBinder().get(), tg_instance.c_str());
        CHECK_EQ(status, STATUS_OK) << "Failed to add service " << tg_instance << " " << status;
    }

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
