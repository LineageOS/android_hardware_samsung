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

#define TA_PATH "/vendor/tee/00000000-0000-0000-0000-474154454b45"
#define TA_UUID "\0\0\0\0\0\0\0\0\0\0GATEKE"
#define TEEC_VALUE_INPUT 1
#define TEEC_VALUE_OUTPUT 2
#define TZ_ENROLL_CMD 0x3f
#define TZ_ENROLL_PAYLOAD_TYPE 0x5557
#define TZ_VERIFY_CMD 0x7e
#define TZ_VERIFY_PAYLOAD_TYPE 0xddf

extern void (*TEEC_CloseSession)(void**);
extern void (*TEEC_FinalizeContext)(void**);
extern void (*TEEC_InitializeContext)(void*, void**);
extern void (*TEEC_InvokeCommand)(void**, int, void*, int*);
extern void (*TEEC_ReleaseSharedMemory)(void*);
extern void (*TEEC_RegisterSharedMemory)(void**, void*);
extern void (*TEECS_OpenSession)(void**, void**, const char[], void*, size_t, int, void*, void*,
                                 int*);

namespace gatekeeper {

template <typename T>
struct shared_mem {
    T* buffer;
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

        enroll_payload.inout.buffer = &enroll_mem;
        shmem_sz_cmd.buffer = &verify_mem;
        TEEC_RegisterSharedMemory(&context, &shmem_sz_cmd);
        verify_payload.inout.buffer = &shmem_sz_cmd.buffer;
        verify_payload.enrolled_password_handle.buffer = &shmem_sz_pwd_handle.buffer;
        verify_payload.provided_password.buffer = &shmem_sz_pwd.buffer;
    }

    ~TeegrisGateKeeper() {
        TEEC_ReleaseSharedMemory(&shmem_sz_cmd);
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

        verify_mem.challenge = request.challenge;
        verify_mem.auth_token_length = sizeof(verify_mem.auth_token_buffer);
        verify_mem.user_id = request.user_id;

        shmem_sz_pwd_handle.buffer = request.password_handle.Data<uint8_t>();
        shmem_sz_pwd_handle.size = request.password_handle.size();
        verify_payload.enrolled_password_handle.size = shmem_sz_pwd_handle.size;
        shmem_sz_pwd.buffer = request.provided_password.Data<uint8_t>();
        shmem_sz_pwd.size = request.provided_password.size();
        verify_payload.provided_password.size = shmem_sz_pwd.size;

        TEEC_RegisterSharedMemory(&context, &shmem_sz_pwd_handle);
        TEEC_RegisterSharedMemory(&context, &shmem_sz_pwd);
        TEEC_InvokeCommand(&session, TZ_VERIFY_CMD, &verify_payload, NULL);
        TEEC_ReleaseSharedMemory(&shmem_sz_pwd_handle);
        TEEC_ReleaseSharedMemory(&shmem_sz_pwd);
        response->error = verify_mem.error;

        if (response->error == ERROR_NONE) {
            auth_token = (uint8_t*)malloc(verify_mem.auth_token_length);
            memcpy(auth_token, shmem_sz_cmd.buffer, verify_mem.auth_token_length);
            response->SetVerificationToken(SizedBuffer(auth_token, verify_mem.auth_token_length));
            response->request_reenroll = verify_mem.request_reenroll;
        } else if (response->error == ERROR_RETRY) {
            response->retry_timeout = verify_mem.retry_timeout;
        }
    }

  private:
    void *context, *session;
    shared_mem<void> shmem_sz_cmd = {.size = sizeof(verify_mem),
                                     .flags = TEEC_VALUE_INPUT | TEEC_VALUE_OUTPUT};
    shared_mem<const uint8_t> shmem_sz_pwd_handle = {.flags = TEEC_VALUE_INPUT};
    shared_mem<const uint8_t> shmem_sz_pwd = {.flags = TEEC_VALUE_INPUT};

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

    struct __attribute__((packed)) {
        uint8_t auth_token_buffer[0x400];
        uint64_t challenge;
        uint32_t auth_token_length;
        bool request_reenroll;
        gatekeeper_error_t error;
        uint32_t retry_timeout;
        uint32_t user_id;
    } verify_mem;

    struct {
        uint32_t started;
        uint32_t paramTypes = TZ_VERIFY_PAYLOAD_TYPE;
        sized_buffer<void*> inout = {.size = sizeof(verify_mem)};
        sized_buffer<const uint8_t*> enrolled_password_handle;
        sized_buffer<const uint8_t*> provided_password;
    } verify_payload;
};

}  // namespace gatekeeper
