#ifndef ANDROID_HARDWARE_IR_V1_0_CONSUMERIR_H
#define ANDROID_HARDWARE_IR_V1_0_CONSUMERIR_H

#include <android/hardware/ir/1.0/IConsumerIr.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace ir {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct ConsumerIr : public IConsumerIr {
    // Methods from ::android::hardware::ir::V1_0::IConsumerIr follow.
    Return<bool> transmit(int32_t carrierFreq, const hidl_vec<int32_t>& pattern) override;
    Return<void> getCarrierFreqs(getCarrierFreqs_cb _hidl_cb) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.

};

// FIXME: most likely delete, this is only for passthrough implementations
// extern "C" IConsumerIr* HIDL_FETCH_IConsumerIr(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace ir
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_IR_V1_0_CONSUMERIR_H
