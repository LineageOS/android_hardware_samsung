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

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include <teecl.h>
}

#define TA_PATH "/vendor/tee/00000000-0000-0000-0000-474154454b45"
#define TA_UUID "\0\0\0\0\0\0\0\0\0\0GATEKE"
#define TEEC_VALUE_INPUT 1
#define TEEC_VALUE_OUTPUT 2
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
    TeegrisGateKeeper() {
        int fd;
        size_t size;
        struct stat st;
        void* data;

        TEEC_InitializeContext(NULL, &context);

        fd = open(TA_PATH, O_RDONLY);
        fstat(fd, &st);
        size = st.st_size;
        data = malloc(size);
        read(fd, data, size);
        close(fd);

        TEECS_OpenSession(&context, &session, TA_UUID, data, size, 0, NULL, NULL, NULL);
        free(data);

        TEEC_AllocateSharedMemory(&context, &shmem_sz_cmd);
        TEEC_AllocateSharedMemory(&context, &shmem_sz_pwd_handle);
        TEEC_AllocateSharedMemory(&context, &shmem_sz_pwd);

        enroll_payload.inout.buffer = &enroll_mem;
        verify_payload.inout.buffer = &shmem_sz_cmd.buffer;
        verify_payload.enrolled_password_handle.buffer = &shmem_sz_pwd_handle.buffer;
        verify_payload.provided_password.buffer = &shmem_sz_pwd.buffer;

        verify_inf = (struct verify_info*)(shmem_sz_cmd.buffer + 0x400);
    }

    ~TeegrisGateKeeper() {
        TEEC_ReleaseSharedMemory(&shmem_sz_cmd);
        TEEC_ReleaseSharedMemory(&shmem_sz_pwd_handle);
        TEEC_ReleaseSharedMemory(&shmem_sz_pwd);
        TEEC_CloseSession(&session);
        TEEC_FinalizeContext(&context);
    }

    void Enroll(const EnrollRequest& request, EnrollResponse* response) {
        uint8_t* enrolled_password_handle;

        enroll_mem.password_handle_length = sizeof(enroll_mem.password_handle_buffer);
        enroll_mem.user_id = request.user_id;

        enroll_payload.current_password_handle.buffer = request.password_handle.Data<uint8_t>();
        enroll_payload.current_password_handle.size = request.password_handle.size();
        enroll_payload.current_password.buffer = request.enrolled_password.Data<uint8_t>();
        enroll_payload.current_password.size = request.enrolled_password.size();
        enroll_payload.desired_password.buffer = request.provided_password.Data<uint8_t>();
        enroll_payload.desired_password.size = request.provided_password.size();

        TEEC_InvokeCommand(&session, TZ_ENROLL_CMD, &enroll_payload, NULL);
        response->error = enroll_mem.error;

        if (response->error == ERROR_NONE) {
            enrolled_password_handle = (uint8_t*)malloc(enroll_mem.password_handle_length);
            memcpy(enrolled_password_handle, enroll_mem.password_handle_buffer,
                   enroll_mem.password_handle_length);
            response->SetEnrolledPasswordHandle(
                    SizedBuffer(enrolled_password_handle, enroll_mem.password_handle_length));
        } else if (response->error == ERROR_RETRY) {
            response->retry_timeout = enroll_mem.retry_timeout;
        }
    }

    void Verify(const VerifyRequest& request, VerifyResponse* response) {
        uint8_t* auth_token;

        verify_inf->challenge = request.challenge;
        verify_inf->auth_token_length = 0x400;
        verify_inf->user_id = request.user_id;

        verify_payload.enrolled_password_handle.size = request.password_handle.size();
        verify_payload.provided_password.size = request.provided_password.size();
        memcpy(shmem_sz_pwd_handle.buffer, request.password_handle.Data<uint8_t>(),
               verify_payload.enrolled_password_handle.size);
        memcpy(shmem_sz_pwd.buffer, request.provided_password.Data<uint8_t>(),
               verify_payload.provided_password.size);

        TEEC_InvokeCommand(&session, TZ_VERIFY_CMD, &verify_payload, NULL);
        response->error = verify_inf->error;

        if (response->error == ERROR_NONE) {
            auth_token = (uint8_t*)malloc(verify_inf->auth_token_length);
            memcpy(auth_token, shmem_sz_cmd.buffer, verify_inf->auth_token_length);
            response->SetVerificationToken(SizedBuffer(auth_token, verify_inf->auth_token_length));
            response->request_reenroll = verify_inf->request_reenroll;
        } else if (response->error == ERROR_RETRY) {
            response->retry_timeout = verify_inf->retry_timeout;
        }
    }

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
