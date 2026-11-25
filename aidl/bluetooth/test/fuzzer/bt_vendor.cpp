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
#include "bt_vendor.h"

#define UNUSED_PARAM __attribute__((unused))
#define HCI_CMD_PREAMBLE_SIZE 3
#define HCI_RESET 0x0C03
#define HCI_EVT_CMD_CMPL_OPCODE 3
#define HCI_EVT_CMD_CMPL_STATUS_RET_BYTE 5
#define MSG_STACK_TO_HC_HCI_CMD 0x2000
#define BT_HC_HDR_SIZE (sizeof(HC_BT_HDR))
#define STREAM_TO_UINT16(u16, p)                                    \
    {                                                               \
        u16 = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); \
        (p) += 2;                                                   \
    }
#define UINT16_TO_STREAM(p, u16)        \
    {                                   \
        *(p)++ = (uint8_t)(u16);        \
        *(p)++ = (uint8_t)((u16) >> 8); \
    }
bt_vendor_callbacks_t* bt_vendor_cbacks = nullptr;

void hw_epilog_cback(void* p_mem) {
    HC_BT_HDR* p_evt_buf = (HC_BT_HDR*)p_mem;
    uint8_t *p, status;
    uint16_t opcode;

    status = *((uint8_t*)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_STATUS_RET_BYTE);
    p = (uint8_t*)(p_evt_buf + 1) + HCI_EVT_CMD_CMPL_OPCODE;
    STREAM_TO_UINT16(opcode, p);

    if (!bt_vendor_cbacks) {
        return;
    }
    /* Must free the RX event buffer */
    bt_vendor_cbacks->dealloc(p_evt_buf);

    /* Once epilog process is done, must call callback to notify caller */
    bt_vendor_cbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);
    return;
}

static int testInit(const bt_vendor_callbacks_t* cb, unsigned char* bdaddr UNUSED_PARAM) {
    if (cb == nullptr) {
        return -1;
    }
    /*store reference to user callbacks */
    bt_vendor_cbacks = (bt_vendor_callbacks_t*)cb;
    return 0;
}

static int testOperations(bt_vendor_opcode_t opcode, void* param UNUSED_PARAM) {
    BtVendor* btVendor = BtVendor::getInstance();
    if (bt_vendor_cbacks) {
        btVendor->setVendorCback(bt_vendor_cbacks, opcode);
    }
    switch (opcode) {
        case BT_VND_OP_POWER_CTRL: {
            // No callback for this opcode
            break;
        }
        case BT_VND_OP_USERIAL_OPEN: {
            int32_t (*fd_array)[] = (int32_t (*)[])param;
            int32_t fdArray[CH_MAX];
            *fdArray = *(btVendor->queryFdList());
            size_t fdcount = btVendor->queryFdCount();
            for (size_t i = 0; i < fdcount; ++i) {
                (*fd_array)[i] = fdArray[i];
            }
            return fdcount;
            break;
        }
        case BT_VND_OP_FW_CFG: {
            if (bt_vendor_cbacks) {
                bt_vendor_cbacks->fwcfg_cb(BT_VND_OP_RESULT_SUCCESS);
            }
            break;
        }
        case BT_VND_OP_GET_LPM_IDLE_TIMEOUT: {
            // No callback for this opcode
            uint32_t* timeout_ms = (uint32_t*)param;
            *timeout_ms = 0;
            break;
        }
        case BT_VND_OP_LPM_SET_MODE: {
            if (bt_vendor_cbacks) {
                bt_vendor_cbacks->lpm_cb(BT_VND_OP_RESULT_SUCCESS);
            }
            break;
        }
        case BT_VND_OP_USERIAL_CLOSE: {
            // No callback for this opcode
            break;
        }
        case BT_VND_OP_LPM_WAKE_SET_STATE: {
            // No callback for this opcode
            break;
        }
        default:
            break;
    }
    return 0;
}

static void testCleanup(void) {
    bt_vendor_cbacks = nullptr;
}

const bt_vendor_interface_t BLUETOOTH_VENDOR_LIB_INTERFACE = {
        sizeof(bt_vendor_interface_t), testInit, testOperations, testCleanup};

void BtVendor::populateFdList(int32_t list[], size_t count) {
    fdCount = count;
    for (size_t i = 0; i < count; ++i) {
        fdList[i] = list[i];
    }
}

void BtVendor::callRemainingCbacks() {
    if (mCbacks) {
        mCbacks->audio_state_cb(BT_VND_OP_RESULT_SUCCESS);
        mCbacks->scocfg_cb(BT_VND_OP_RESULT_SUCCESS);
        mCbacks->a2dp_offload_cb(BT_VND_OP_RESULT_SUCCESS, mOpcode, 0);
        mCbacks->epilog_cb(BT_VND_OP_RESULT_SUCCESS);

        HC_BT_HDR* p_buf = NULL;
        uint8_t* p;

        /* Sending a HCI_RESET */
        /* Must allocate command buffer via HC's alloc API */
        p_buf = (HC_BT_HDR*)mCbacks->alloc(BT_HC_HDR_SIZE + HCI_CMD_PREAMBLE_SIZE);
        if (p_buf) {
            p_buf->event = MSG_STACK_TO_HC_HCI_CMD;
            p_buf->offset = 0;
            p_buf->layer_specific = 0;
            p_buf->len = HCI_CMD_PREAMBLE_SIZE;

            p = (uint8_t*)(p_buf + 1);
            UINT16_TO_STREAM(p, HCI_RESET);
            *p = 0; /* parameter length */

            /* Send command via HC's xmit_cb API */
            mCbacks->xmit_cb(HCI_RESET, p_buf, hw_epilog_cback);
        } else {
            mCbacks->epilog_cb(BT_VND_OP_RESULT_FAIL);
        }
    }
}
