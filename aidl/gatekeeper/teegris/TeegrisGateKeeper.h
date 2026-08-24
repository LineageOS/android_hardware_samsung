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

#define TRUSTLET_PATH "/vendor/tee/00000000-0000-0000-0000-474154454b45"
#define TRUSTLET_ID "\0\0\0\0\0\0\0\0\0\0GATEKE"

extern void (*TEEC_AllocateSharedMemory)(void**, void*);
extern void (*TEEC_CloseSession)(void**);
extern void (*TEEC_FinalizeContext)(void**);
extern void (*TEEC_InitializeContext)(void*, void**);
extern void (*TEEC_InvokeCommand)(void**, int, void*, int*);
extern void (*TEEC_ReleaseSharedMemory)(void*);
extern void (*TEECS_OpenSession)(void**, void**, const char[], void*, size_t, int, void*, void*,
                                 int*);

namespace gatekeeper {

struct shared_mem {
    uint8_t* ptr;
    size_t size;
    uint32_t mode;
    uint32_t reserved[3];
};

template <typename T>
struct sized_buffer {
    T* ptr;
    uint32_t size;
    uint32_t reserved[3];
};

struct __attribute__((packed)) info_verify {
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

        fd = open(TRUSTLET_PATH, O_RDONLY);
        fstat(fd, &st);
        size = st.st_size;
        data = malloc(size);
        read(fd, data, size);
        close(fd);

        TEECS_OpenSession(&context, &session, TRUSTLET_ID, data, size, 0, NULL, NULL, NULL);
        free(data);

        TEEC_AllocateSharedMemory(&context, &shmem_sz_cmd);
        TEEC_AllocateSharedMemory(&context, &shmem_sz_pwd_handle);
        TEEC_AllocateSharedMemory(&context, &shmem_sz_pwd);

        msg_enroll.buffer.ptr = &mem_enroll;
        msg_verify.buffer.ptr = &shmem_sz_cmd.ptr;
        msg_verify.enrolled_password_handle.ptr = &shmem_sz_pwd_handle.ptr;
        msg_verify.provided_password.ptr = &shmem_sz_pwd.ptr;

        inf_verify = (struct info_verify*)(shmem_sz_cmd.ptr + 0x400);
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

        mem_enroll.password_handle_length = 0x400;
        mem_enroll.user_id = request.user_id;

        msg_enroll.current_password_handle.ptr = request.password_handle.Data<uint8_t>();
        msg_enroll.current_password_handle.size = request.password_handle.size();
        msg_enroll.current_password.ptr = request.enrolled_password.Data<uint8_t>();
        msg_enroll.current_password.size = request.enrolled_password.size();
        msg_enroll.desired_password.ptr = request.provided_password.Data<uint8_t>();
        msg_enroll.desired_password.size = request.provided_password.size();

        TEEC_InvokeCommand(&session, 0x3f, &msg_enroll, NULL);
        response->error = mem_enroll.error;

        if (response->error == ERROR_NONE) {
            enrolled_password_handle = (uint8_t*)malloc(mem_enroll.password_handle_length);
            memcpy(enrolled_password_handle, mem_enroll.password_handle_buffer,
                   mem_enroll.password_handle_length);
            response->SetEnrolledPasswordHandle(
                    SizedBuffer(enrolled_password_handle, mem_enroll.password_handle_length));
        } else if (response->error == ERROR_RETRY) {
            response->retry_timeout = mem_enroll.retry_timeout;
        }
    }

    void Verify(const VerifyRequest& request, VerifyResponse* response) {
        uint8_t* auth_token;

        inf_verify->challenge = request.challenge;
        inf_verify->auth_token_length = 0x400;
        inf_verify->user_id = request.user_id;

        msg_verify.enrolled_password_handle.size = request.password_handle.size();
        msg_verify.provided_password.size = request.provided_password.size();
        memcpy(shmem_sz_pwd_handle.ptr, request.password_handle.Data<uint8_t>(),
               msg_verify.enrolled_password_handle.size);
        memcpy(shmem_sz_pwd.ptr, request.provided_password.Data<uint8_t>(),
               msg_verify.provided_password.size);

        TEEC_InvokeCommand(&session, 0x7e, &msg_verify, NULL);
        response->error = inf_verify->error;

        if (response->error == ERROR_NONE) {
            auth_token = (uint8_t*)malloc(inf_verify->auth_token_length);
            memcpy(auth_token, shmem_sz_cmd.ptr, inf_verify->auth_token_length);
            response->SetVerificationToken(SizedBuffer(auth_token, inf_verify->auth_token_length));
            response->request_reenroll = inf_verify->request_reenroll;
        } else if (response->error == ERROR_RETRY) {
            response->retry_timeout = inf_verify->retry_timeout;
        }
    }

  private:
    void *context, *session;
    shared_mem shmem_sz_cmd = {.size = 0x419, .mode = 3};
    shared_mem shmem_sz_pwd_handle = {.size = 0x40, .mode = 1};
    shared_mem shmem_sz_pwd = {.size = 0x80, .mode = 1};
    info_verify* inf_verify;

    struct {
        uint8_t password_handle_buffer[0x400];
        uint32_t password_handle_length;
        gatekeeper_error_t error;
        uint32_t retry_timeout;
        uint32_t user_id;
    } mem_enroll;

    struct {
        uint64_t type = 0x555700000000;
        sized_buffer<void> buffer = {.size = sizeof(mem_enroll)};
        sized_buffer<const uint8_t> current_password_handle;
        sized_buffer<const uint8_t> current_password;
        sized_buffer<const uint8_t> desired_password;
        uint64_t reserved;
    } msg_enroll;

    struct {
        uint64_t type = 0xddf00000000;
        sized_buffer<uint8_t*> buffer = {.size = 0x419};
        sized_buffer<uint8_t*> enrolled_password_handle;
        sized_buffer<uint8_t*> provided_password;
    } msg_verify;
};

}  // namespace gatekeeper
