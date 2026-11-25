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

#include "h4_protocol.h"

#define LOG_TAG "android.hardware.bluetooth-hci-h4"

#include <errno.h>
#include <fcntl.h>
#include <log/log.h>
#include <sys/uio.h>
#include <unistd.h>

namespace android {
namespace hardware {
namespace bluetooth {
namespace hci {

size_t H4Protocol::Send(uint8_t type, const uint8_t* data, size_t length) {
    struct iovec iov_array[] = {{&type, sizeof(type)}, {const_cast<uint8_t*>(data), length}};
    struct iovec* iov = iov_array;
    int iovcnt = sizeof(iov_array) / sizeof(iov_array[0]);
    size_t total_bytes = 0;
    for (int i = 0; i < iovcnt; i++) {
        total_bytes += iov_array[i].iov_len;
    }
    size_t bytes_written = 0;
    size_t remaining_bytes = total_bytes;

    while (remaining_bytes > 0) {
        ssize_t ret = TEMP_FAILURE_RETRY(writev(uart_fd_, iov, iovcnt));
        if (ret == -1) {
            if (errno == EAGAIN) continue;
            ALOGE("%s error writing to UART (%s)", __func__, strerror(errno));
            break;
        } else if (ret == 0) {
            // Nothing written
            ALOGE("%s zero bytes written - something went wrong...", __func__);
            break;
        } else if (ret == remaining_bytes) {
            // Everything written
            bytes_written += ret;
            break;
        }

        bytes_written += ret;
        remaining_bytes -= ret;
        ALOGW("%s: %d/%d bytes written - retrying remaining %d bytes", __func__,
              static_cast<int>(bytes_written), static_cast<int>(total_bytes),
              static_cast<int>(remaining_bytes));

        // Remove iovs which are written from the list
        while (ret >= iov->iov_len) {
            ret -= iov->iov_len;
            ++iov;
            --iovcnt;
        }
        // Adjust the iov to point to the remaining data which needs to be written
        if (ret) {
            iov->iov_base = static_cast<uint8_t*>(iov->iov_base) + ret;
            iov->iov_len -= ret;
        }
    }
    return bytes_written;
}

void H4Protocol::OnPacketReady() {
    switch (hci_packet_type_) {
        case HCI_PACKET_TYPE_EVENT:
            event_cb_(hci_packetizer_.GetPacket());
            break;
        case HCI_PACKET_TYPE_ACL_DATA:
            acl_cb_(hci_packetizer_.GetPacket());
            break;
        case HCI_PACKET_TYPE_SCO_DATA:
            sco_cb_(hci_packetizer_.GetPacket());
            break;
        case HCI_PACKET_TYPE_ISO_DATA:
            iso_cb_(hci_packetizer_.GetPacket());
            break;
        case HCI_PACKET_TYPE_UNKNOWN:
            ALOGE("%s: Unknown packet sent", __func__);
            break;
        default:
            LOG_ALWAYS_FATAL("%s: Unimplemented packet type %d", __func__,
                             static_cast<int>(hci_packet_type_));
    }
    // Get ready for the next type byte.
    hci_packet_type_ = HCI_PACKET_TYPE_UNKNOWN;
}

void H4Protocol::OnDataReady(int fd) {
    if (hci_packet_type_ == HCI_PACKET_TYPE_UNKNOWN) {
        uint8_t buffer[1] = {0};
        ssize_t bytes_read = TEMP_FAILURE_RETRY(read(fd, buffer, 1));
        if (bytes_read != 1) {
            if (bytes_read == 0) {
                // This is only expected if the UART got closed when shutting down.
                ALOGE("%s: Unexpected EOF reading the packet type!", __func__);
                sleep(5);  // Expect to be shut down within 5 seconds.
                return;
            } else if (bytes_read < 0) {
                LOG_ALWAYS_FATAL("%s: Read packet type error: %s", __func__, strerror(errno));
            } else {
                LOG_ALWAYS_FATAL("%s: More bytes read than expected (%u)!", __func__,
                                 static_cast<unsigned int>(bytes_read));
            }
        }
        hci_packet_type_ = static_cast<HciPacketType>(buffer[0]);
        if (hci_packet_type_ == HCI_PACKET_TYPE_UNKNOWN) {
            ALOGE("%s: Unknown packet sent", __func__);
        } else if (hci_packet_type_ != HCI_PACKET_TYPE_ACL_DATA &&
            hci_packet_type_ != HCI_PACKET_TYPE_SCO_DATA &&
            hci_packet_type_ != HCI_PACKET_TYPE_ISO_DATA &&
            hci_packet_type_ != HCI_PACKET_TYPE_EVENT) {
            LOG_ALWAYS_FATAL("%s: Unimplemented packet type %d", __func__,
                             static_cast<int>(hci_packet_type_));
        }
    } else {
        hci_packetizer_.OnDataReady(fd, hci_packet_type_);
    }
}

}  // namespace hci
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
