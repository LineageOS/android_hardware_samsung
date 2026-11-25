/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __BT_VENDOR_H__
#define __BT_VENDOR_H__

#include "bt_vendor_lib.h"

class BtVendor {
  public:
    static BtVendor* getInstance() {
        if (!mInstance) {
            mInstance = new BtVendor;
        }
        return mInstance;
    }

    void setVendorCback(bt_vendor_callbacks_t* cb, bt_vendor_opcode_t opcode) {
        mCbacks = cb;
        mOpcode = opcode;
    }

    int32_t* queryFdList() { return fdList; }
    size_t queryFdCount() { return fdCount; }
    void callRemainingCbacks();
    void populateFdList(int32_t list[], size_t count);

  private:
    BtVendor() = default;

    ~BtVendor() {
        if (mInstance) {
            delete mInstance;
            mInstance = nullptr;
        }
        mCbacks = nullptr;
    }

    static BtVendor* mInstance;
    bt_vendor_callbacks_t* mCbacks = nullptr;
    bt_vendor_opcode_t mOpcode;
    int32_t fdCount;
    int32_t fdList[CH_MAX] = {0};
};

BtVendor* BtVendor::mInstance = nullptr;
#endif  // __BT_VENDOR_H__
