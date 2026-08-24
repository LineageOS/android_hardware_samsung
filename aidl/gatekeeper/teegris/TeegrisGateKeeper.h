/*
 * Copyright 2015 The Android Open Source Project
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
 *
 */

#include <gatekeeper/gatekeeper.h>

extern "C" {
#include <teecl.h>
}

#define TA_PATH "/vendor/tee/00000000-0000-0000-0000-474154454b45"
#define TA_UUID "\0\0\0\0\0\0\0\0\0\0GATEKE"
#define TZ_ENROLL_CMD 0x3f
#define TZ_ENROLL_PAYLOAD_TYPE 0x5557
#define TZ_VERIFY_CMD 0x7e
#define TZ_VERIFY_PAYLOAD_TYPE 0xddf

namespace gatekeeper {

struct shared_mem {
    uint8_t* buffer;
    size_t size;
    uint32_t flags;
    uint32_t reserved[3];
};

template <typename T>
struct sized_buffer {
    T* buffer;
    uint32_t size;
    uint32_t reserved[3];
};

struct __attribute__((packed)) verify_info {
    uint64_t challenge;
    uint32_t auth_token_length;
    bool request_reenroll;
    gatekeeper_error_t error;
    uint32_t retry_timeout;
    uint32_t user_id;
};

class TeegrisGateKeeper {
  public:
    TeegrisGateKeeper();
    ~TeegrisGateKeeper();
    void Enroll(const EnrollRequest& request, EnrollResponse* response);
    void Verify(const VerifyRequest& request, VerifyResponse* response);

  private:
    void *context, *session;
    shared_mem shmem_sz_cmd = {.size = 0x419, .flags = TEEC_VALUE_INPUT | TEEC_VALUE_OUTPUT};
    shared_mem shmem_sz_pwd_handle = {.size = 0x40, .flags = TEEC_VALUE_INPUT};
    shared_mem shmem_sz_pwd = {.size = 0x80, .flags = TEEC_VALUE_INPUT};
    verify_info* verify_inf;

    struct {
        uint8_t password_handle_buffer[0x400];
        uint32_t password_handle_length;
        gatekeeper_error_t error;
        uint32_t retry_timeout;
        uint32_t user_id;
    } enroll_mem;

    struct {
        uint32_t started;
        uint32_t paramTypes = TZ_ENROLL_PAYLOAD_TYPE;
        sized_buffer<void> inout = {.size = sizeof(enroll_mem)};
        sized_buffer<const uint8_t> current_password_handle;
        sized_buffer<const uint8_t> current_password;
        sized_buffer<const uint8_t> desired_password;
        uint64_t reserved;
    } enroll_payload;

    struct {
        uint32_t started;
        uint32_t paramTypes = TZ_VERIFY_PAYLOAD_TYPE;
        sized_buffer<uint8_t*> inout = {.size = 0x419};
        sized_buffer<uint8_t*> enrolled_password_handle;
        sized_buffer<uint8_t*> provided_password;
    } verify_payload;
};

}  // namespace gatekeeper
