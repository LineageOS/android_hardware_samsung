//
// Copyright 2016 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>
using std::vector;

#include "bluetooth_address.h"

namespace android {
namespace hardware {
namespace bluetooth {
namespace V1_0 {
namespace implementation {

constexpr char kTestAddr1[BluetoothAddress::kStringLength + 1] = "12:34:56:78:9a:bc";
constexpr uint8_t kTestAddr1_bytes[BluetoothAddress::kBytes] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
constexpr char kZeros[BluetoothAddress::kStringLength + 1] = "00:00:00:00:00:00";
constexpr uint8_t kZeros_bytes[BluetoothAddress::kBytes] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

class BluetoothAddressTest : public ::testing::Test {
  public:
    BluetoothAddressTest() {}
    ~BluetoothAddressTest() {}

    void FileWriteString(const char* path, const char* string);
};

void BluetoothAddressTest::FileWriteString(const char* path, const char* string) {
    int fd = open(path, O_CREAT | O_RDWR, 0600);
    EXPECT_TRUE(fd > 0) << "err = " << strerror(errno);

    size_t length = strlen(string);
    size_t bytes_written = write(fd, string, length);

    EXPECT_EQ(length, bytes_written) << strerror(errno);

    close(fd);
}

TEST_F(BluetoothAddressTest, string_to_bytes) {
    uint8_t addr[BluetoothAddress::kBytes];

    // Malformed addresses
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("000000000000", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("00:00:00:00:0000", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("00:00:00:00:00:0", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("00:00:00:00:00:0;", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("aB:cD:eF:Gh:iJ:Kl", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("00:00:000:00:00:0;", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("12:34:56:78:90:12;", addr));
    EXPECT_FALSE(BluetoothAddress::string_to_bytes("12:34:56:78:90:123", addr));

    // Reasonable addresses
    EXPECT_TRUE(BluetoothAddress::string_to_bytes("00:00:00:00:00:00", addr));
    EXPECT_TRUE(BluetoothAddress::string_to_bytes("a5:a5:a5:a5:a5:a5", addr));
    EXPECT_TRUE(BluetoothAddress::string_to_bytes("5A:5A:5A:5A:5A:5A", addr));
    EXPECT_TRUE(BluetoothAddress::string_to_bytes("AA:BB:CC:DD:EE:FF", addr));
    EXPECT_TRUE(BluetoothAddress::string_to_bytes("aa:bb:cc:dd:ee:ff", addr));

    // Compare the output to known bytes
    uint8_t addrA[BluetoothAddress::kBytes];
    uint8_t addrB[BluetoothAddress::kBytes];

    // kTestAddr1
    EXPECT_TRUE(BluetoothAddress::string_to_bytes(kTestAddr1, addrA));
    EXPECT_TRUE(memcmp(addrA, kTestAddr1_bytes, BluetoothAddress::kBytes) == 0);

    // kZeros
    EXPECT_TRUE(BluetoothAddress::string_to_bytes(kZeros, addrB));
    EXPECT_TRUE(memcmp(addrB, kZeros_bytes, BluetoothAddress::kBytes) == 0);

    // kTestAddr1 != kZeros
    EXPECT_FALSE(memcmp(addrA, addrB, BluetoothAddress::kBytes) == 0);
}

TEST_F(BluetoothAddressTest, bytes_to_string) {
    char addrA[BluetoothAddress::kStringLength + 1] = "";
    char addrB[BluetoothAddress::kStringLength + 1] = "";

    // kTestAddr1
    BluetoothAddress::bytes_to_string(kTestAddr1_bytes, addrA);
    EXPECT_TRUE(memcmp(addrA, kTestAddr1, BluetoothAddress::kStringLength) == 0);

    // kZeros
    BluetoothAddress::bytes_to_string(kZeros_bytes, addrB);
    EXPECT_TRUE(memcmp(addrB, kZeros, BluetoothAddress::kStringLength) == 0);

    // kTestAddr1 != kZeros
    EXPECT_FALSE(memcmp(addrA, addrB, BluetoothAddress::kStringLength) == 0);
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
