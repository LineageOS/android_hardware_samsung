/*
 * Copyright (C) 2021 The Android Open Source Project
 * Copyright (C) 2026 j0sh1x<aljoshua.hell@gmail.com>
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

#include <aidl/android/hardware/ir/BnConsumerIr.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <dlfcn.h>
#include <log/log.h>

#include <samsung_ir.h>

#include <string>

using ::aidl::android::hardware::ir::ConsumerIrFreqRange;
using ::android::base::WriteStringToFile;

namespace aidl::android::hardware::ir {

class ConsumerIr : public BnConsumerIr {
  public:
    ConsumerIr();
    ~ConsumerIr();

  private:
    ::ndk::ScopedAStatus getCarrierFreqs(std::vector<ConsumerIrFreqRange>* _aidl_return) override;

    ::ndk::ScopedAStatus transmit(int32_t in_carrierFreqHz,
                                  const std::vector<int32_t>& in_pattern) override;
};

ConsumerIr::ConsumerIr() {
    ALOGI("ConsumerIr AIDL service initializing");
}

ConsumerIr::~ConsumerIr() {}

::ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(
        std::vector<::aidl::android::hardware::ir::ConsumerIrFreqRange>* out_ranges) {
    *out_ranges = kCarrierFreqRanges;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ConsumerIr::transmit(int32_t in_carrierFreqHz,
                                          const std::vector<int32_t>& in_pattern) {
    float factor;
    std::vector<int32_t> buffer{in_carrierFreqHz};

#ifndef MS_IR_SIGNAL
    factor = 1000000.0f / static_cast<float>(in_carrierFreqHz);
#else
    factor = 1.0f;
#endif

    for (const int32_t& number : in_pattern) {
        buffer.emplace_back(static_cast<int32_t>(number / factor));
    }

    std::string out;
    for (size_t i = 0; i < buffer.size(); ++i) {
        if (i > 0) out += ',';
        out += std::to_string(buffer[i]);
    }

    if (!WriteStringToFile(out, IR_PATH, true)) {
        return ::ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ::ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::ir

using aidl::android::hardware::ir::ConsumerIr;

int main() {
    auto binder = ::ndk::SharedRefBase::make<ConsumerIr>();
    const std::string name = std::string() + ConsumerIr::descriptor + "/default";

    CHECK_EQ(STATUS_OK, AServiceManager_addService(binder->asBinder().get(), name.c_str()))
            << "Failed to register " << name;

    ABinderProcess_setThreadPoolMaxThreadCount(0);
    ABinderProcess_joinThreadPool();

    return EXIT_FAILURE;  // should not be reached
}
