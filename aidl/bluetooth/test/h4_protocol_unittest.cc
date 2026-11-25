//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define LOG_TAG "bt_h4_unittest"

#include "h4_protocol.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include <log/log.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

using hci::H4Protocol;
using ::testing::Eq;

static char sample_data1[100] = "A point is that which has no part.";
static char sample_data2[100] = "A line is breadthless length.";
static char sample_data3[100] = "The ends of a line are points.";
static char sample_data4[100] =
        "A plane surface is a surface which lies evenly with the straight ...";
static char acl_data[100] =
        "A straight line is a line which lies evenly with the points on itself.";
static char sco_data[100] = "A surface is that which has length and breadth only.";
static char event_data[100] = "The edges of a surface are lines.";
static char iso_data[100] = "A plane angle is the inclination to one another of two lines in a ...";

MATCHER_P3(HidlVecMatches, preamble, preamble_length, payload, "") {
    size_t length = strlen(payload) + preamble_length;
    if (length != arg.size()) {
        return false;
    }

    if (memcmp(preamble, arg.data(), preamble_length) != 0) {
        return false;
    }

    return memcmp(payload, arg.data() + preamble_length, length - preamble_length) == 0;
};

ACTION_P2(Notify, mutex, condition) {
    ALOGD("%s", __func__);
    std::unique_lock<std::mutex> lock(*mutex);
    condition->notify_one();
}

class H4ProtocolTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ALOGD("%s", __func__);

        int sockfd[2];
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);
        H4Protocol* h4_hci =
                new H4Protocol(sockfd[0], event_cb_.AsStdFunction(), acl_cb_.AsStdFunction(),
                               sco_cb_.AsStdFunction(), iso_cb_.AsStdFunction());
        fd_watcher_.WatchFdForNonBlockingReads(sockfd[0],
                                               [h4_hci](int fd) { h4_hci->OnDataReady(fd); });
        protocol_ = h4_hci;

        fake_uart_ = sockfd[1];
    }

    void TearDown() override { fd_watcher_.StopWatchingFileDescriptors(); }

    void SendAndReadUartOutbound(uint8_t type, char* data) {
        ALOGD("%s sending", __func__);
        int data_length = strlen(data);
        protocol_->Send(type, (uint8_t*)data, data_length);

        int uart_length = data_length + 1;  // + 1 for data type code
        int i;

        ALOGD("%s reading", __func__);
        for (i = 0; i < uart_length; i++) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(fake_uart_, &read_fds);
            TEMP_FAILURE_RETRY(select(fake_uart_ + 1, &read_fds, NULL, NULL, NULL));

            char byte;
            TEMP_FAILURE_RETRY(read(fake_uart_, &byte, 1));

            EXPECT_EQ(i == 0 ? type : data[i - 1], byte);
        }

        EXPECT_EQ(i, uart_length);
    }

    void WriteAndExpectInboundAclData(char* payload) {
        // h4 type[1] + handle[2] + size[2]
        char preamble[5] = {HCI_PACKET_TYPE_ACL_DATA, 19, 92, 0, 0};
        int length = strlen(payload);
        preamble[3] = length & 0xFF;
        preamble[4] = (length >> 8) & 0xFF;

        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_, preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_, payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(acl_cb_, Call(HidlVecMatches(preamble + 1, sizeof(preamble) - 1, payload)))
                .WillOnce(Notify(&mutex, &done));

        // Fail if it takes longer than 100 ms.
        auto timeout_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        {
            std::unique_lock<std::mutex> lock(mutex);
            done.wait_until(lock, timeout_time);
        }
    }

    void WriteAndExpectInboundScoData(char* payload) {
        // h4 type[1] + handle[2] + size[1]
        char preamble[4] = {HCI_PACKET_TYPE_SCO_DATA, 20, 17, 0};
        preamble[3] = strlen(payload) & 0xFF;

        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_, preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_, payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(sco_cb_, Call(HidlVecMatches(preamble + 1, sizeof(preamble) - 1, payload)))
                .WillOnce(Notify(&mutex, &done));

        // Fail if it takes longer than 100 ms.
        auto timeout_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        {
            std::unique_lock<std::mutex> lock(mutex);
            done.wait_until(lock, timeout_time);
        }
    }

    void WriteAndExpectInboundEvent(char* payload) {
        // h4 type[1] + event_code[1] + size[1]
        char preamble[3] = {HCI_PACKET_TYPE_EVENT, 9, 0};
        preamble[2] = strlen(payload) & 0xFF;
        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_, preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_, payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(event_cb_, Call(HidlVecMatches(preamble + 1, sizeof(preamble) - 1, payload)))
                .WillOnce(Notify(&mutex, &done));

        {
            std::unique_lock<std::mutex> lock(mutex);
            done.wait(lock);
        }
    }

    void WriteAndExpectInboundIsoData(char* payload) {
        // h4 type[1] + handle[2] + size[1]
        char preamble[5] = {HCI_PACKET_TYPE_ISO_DATA, 19, 92, 0, 0};
        int length = strlen(payload);
        preamble[3] = length & 0xFF;
        preamble[4] = (length >> 8) & 0x3F;

        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_, preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_, payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(iso_cb_, Call(HidlVecMatches(preamble + 1, sizeof(preamble) - 1, payload)))
                .WillOnce(Notify(&mutex, &done));

        // Fail if it takes longer than 100 ms.
        auto timeout_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        {
            std::unique_lock<std::mutex> lock(mutex);
            done.wait_until(lock, timeout_time);
        }
    }

    testing::MockFunction<void(const hidl_vec<uint8_t>&)> event_cb_;
    testing::MockFunction<void(const hidl_vec<uint8_t>&)> acl_cb_;
    testing::MockFunction<void(const hidl_vec<uint8_t>&)> sco_cb_;
    testing::MockFunction<void(const hidl_vec<uint8_t>&)> iso_cb_;
    async::AsyncFdWatcher fd_watcher_;
    H4Protocol* protocol_;
    int fake_uart_;
};

// Test sending data sends correct data onto the UART
TEST_F(H4ProtocolTest, TestSends) {
    SendAndReadUartOutbound(HCI_PACKET_TYPE_COMMAND, sample_data1);
    SendAndReadUartOutbound(HCI_PACKET_TYPE_ACL_DATA, sample_data2);
    SendAndReadUartOutbound(HCI_PACKET_TYPE_SCO_DATA, sample_data3);
    SendAndReadUartOutbound(HCI_PACKET_TYPE_ISO_DATA, sample_data4);
}

// Ensure we properly parse data coming from the UART
TEST_F(H4ProtocolTest, TestReads) {
    WriteAndExpectInboundAclData(acl_data);
    WriteAndExpectInboundScoData(sco_data);
    WriteAndExpectInboundEvent(event_data);
    WriteAndExpectInboundIsoData(iso_data);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
