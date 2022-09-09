#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "Memtrack.h"

#undef LOG_TAG
#define LOG_TAG "memtrack-service"

using aidl::android::hardware::memtrack::Memtrack;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Memtrack> memtrack = ndk::SharedRefBase::make<Memtrack>();

    const std::string instance = std::string() + Memtrack::descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(memtrack->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE; // Unreachable
}
