/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <android-base/strings.h>
#include <android/hardware/usb/gadget/1.2/IUsbGadget.h>
#include <android/hardware/usb/gadget/1.2/types.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <pixelusb/UsbGadgetCommon.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <utils/Log.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace android {
namespace hardware {
namespace usb {
namespace gadget {
namespace V1_2 {
namespace implementation {

using ::android::sp;
using ::android::base::GetProperty;
using ::android::base::SetProperty;
using ::android::base::unique_fd;
using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::google::pixel::usb::addAdb;
using ::android::hardware::google::pixel::usb::addEpollFd;
using ::android::hardware::google::pixel::usb::getVendorFunctions;
using ::android::hardware::google::pixel::usb::kDebug;
using ::android::hardware::google::pixel::usb::kDisconnectWaitUs;
using ::android::hardware::google::pixel::usb::linkFunction;
using ::android::hardware::google::pixel::usb::MonitorFfs;
using ::android::hardware::google::pixel::usb::resetGadget;
using ::android::hardware::google::pixel::usb::setVidPid;
using ::android::hardware::google::pixel::usb::unlinkFunctions;
using ::android::hardware::usb::gadget::V1_0::Status;
using ::android::hardware::usb::gadget::V1_0::IUsbGadgetCallback;
using ::android::hardware::usb::gadget::V1_2::IUsbGadget;
using ::android::hardware::usb::gadget::V1_2::GadgetFunction;
using ::std::string;

constexpr char kGadgetName[] = "11110000.dwc3";
#ifndef UDC_PATH
#define UDC_PATH "/sys/class/udc/11110000.dwc3/"
#endif
static MonitorFfs monitorFfs(kGadgetName);

#define SPEED_PATH UDC_PATH "current_speed"

struct UsbGadget : public IUsbGadget {
    UsbGadget();

    // Makes sure that only one request is processed at a time.
    std::mutex mLockSetCurrentFunction;
    uint64_t mCurrentUsbFunctions;
    bool mCurrentUsbFunctionsApplied;
    UsbSpeed mUsbSpeed;

    Return<void> setCurrentUsbFunctions(uint64_t functions,
                                        const sp<V1_0::IUsbGadgetCallback> &callback,
                                        uint64_t timeout) override;

    Return<void> getCurrentUsbFunctions(const sp<V1_0::IUsbGadgetCallback> &callback) override;

    Return<Status> reset() override;

    Return<void> getUsbSpeed(const sp<V1_2::IUsbGadgetCallback> &callback) override;

  private:
    Status tearDownGadget();
    Status setupFunctions(uint64_t functions, const sp<V1_0::IUsbGadgetCallback> &callback,
                          uint64_t timeout);
};

}  // namespace implementation
}  // namespace V1_2
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
