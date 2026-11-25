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

#include "mct_protocol.h"
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

using hci::MctProtocol;
using ::testing::Eq;

static char sample_data1[100] = "A point is that which has no part.";
static char sample_data2[100] = "A line is breadthless length.";
static char acl_data[100] =
        "A straight line is a line which lies evenly with the points on itself.";
static char event_data[100] = "The edges of a surface are lines.";

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

class MctProtocolTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ALOGD("%s", __func__);

        int mct_fds[CH_MAX];
        MakeFakeUartFd(CH_CMD, mct_fds, fake_uart_);
        MakeFakeUartFd(CH_EVT, mct_fds, fake_uart_);
        MakeFakeUartFd(CH_ACL_IN, mct_fds, fake_uart_);
        MakeFakeUartFd(CH_ACL_OUT, mct_fds, fake_uart_);

        MctProtocol* mct_hci =
                new MctProtocol(mct_fds, event_cb_.AsStdFunction(), acl_cb_.AsStdFunction());
        fd_watcher_.WatchFdForNonBlockingReads(
                mct_fds[CH_EVT], [mct_hci](int fd) { mct_hci->OnEventDataReady(fd); });
        fd_watcher_.WatchFdForNonBlockingReads(mct_fds[CH_ACL_IN],
                                               [mct_hci](int fd) { mct_hci->OnAclDataReady(fd); });
        protocol_ = mct_hci;
    }

    void MakeFakeUartFd(int index, int* host_side, int* controller_side) {
        int sockfd[2];
        socketpair(AF_LOCAL, SOCK_STREAM, 0, sockfd);
        host_side[index] = sockfd[0];
        controller_side[index] = sockfd[1];
    }

    void TearDown() override { fd_watcher_.StopWatchingFileDescriptors(); }

    void SendAndReadUartOutbound(uint8_t type, char* data, int outbound_fd) {
        ALOGD("%s sending", __func__);
        int data_length = strlen(data);
        protocol_->Send(type, (uint8_t*)data, data_length);

        ALOGD("%s reading", __func__);
        int i;
        for (i = 0; i < data_length; i++) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(outbound_fd, &read_fds);
            TEMP_FAILURE_RETRY(select(outbound_fd + 1, &read_fds, NULL, NULL, NULL));

            char byte;
            TEMP_FAILURE_RETRY(read(outbound_fd, &byte, 1));

            EXPECT_EQ(data[i], byte);
        }

        EXPECT_EQ(i, data_length);
    }

    void WriteAndExpectInboundAclData(char* payload) {
        // handle[2] + size[2]
        char preamble[4] = {19, 92, 0, 0};
        int length = strlen(payload);
        preamble[2] = length & 0xFF;
        preamble[3] = (length >> 8) & 0xFF;

        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_[CH_ACL_IN], preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_[CH_ACL_IN], payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(acl_cb_, Call(HidlVecMatches(preamble, sizeof(preamble), payload)))
                .WillOnce(Notify(&mutex, &done));

        // Fail if it takes longer than 100 ms.
        auto timeout_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        {
            std::unique_lock<std::mutex> lock(mutex);
            done.wait_until(lock, timeout_time);
        }
    }

    void WriteAndExpectInboundEvent(char* payload) {
        // event_code[1] + size[1]
        char preamble[2] = {9, 0};
        preamble[1] = strlen(payload) & 0xFF;

        ALOGD("%s writing", __func__);
        TEMP_FAILURE_RETRY(write(fake_uart_[CH_EVT], preamble, sizeof(preamble)));
        TEMP_FAILURE_RETRY(write(fake_uart_[CH_EVT], payload, strlen(payload)));

        ALOGD("%s waiting", __func__);
        std::mutex mutex;
        std::condition_variable done;
        EXPECT_CALL(event_cb_, Call(HidlVecMatches(preamble, sizeof(preamble), payload)))
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
    async::AsyncFdWatcher fd_watcher_;
    MctProtocol* protocol_;
    int fake_uart_[CH_MAX];
};

// Test sending data sends correct data onto the UART
TEST_F(MctProtocolTest, TestSends) {
    SendAndReadUartOutbound(HCI_PACKET_TYPE_COMMAND, sample_data1, fake_uart_[CH_CMD]);
    SendAndReadUartOutbound(HCI_PACKET_TYPE_ACL_DATA, sample_data2, fake_uart_[CH_ACL_OUT]);
}

// Ensure we properly parse data coming from the UART
TEST_F(MctProtocolTest, TestReads) {
    WriteAndExpectInboundAclData(acl_data);
    WriteAndExpectInboundEvent(event_data);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
