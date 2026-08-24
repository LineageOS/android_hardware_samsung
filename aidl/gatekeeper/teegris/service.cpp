/*
 * Copyright (C) 2016 The Android Open Source Project
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
 */

#define LOG_TAG "android.hardware.gatekeeper-service.teegris"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <dlfcn.h>

#include "GateKeeper.h"

void (*TEEC_AllocateSharedMemory)(void**, void*);
void (*TEEC_CloseSession)(void**);
void (*TEEC_FinalizeContext)(void**);
void (*TEEC_InitializeContext)(void*, void**);
void (*TEEC_InvokeCommand)(void**, int, void*, int*);
void (*TEEC_ReleaseSharedMemory)(void*);
void (*TEECS_OpenSession)(void**, void**, const char[], void*, size_t, int, void*, void*, int*);

using aidl::android::hardware::gatekeeper::TeegrisGateKeeperDevice;

int main() {
    void* libteecl_handle = dlopen("/vendor/lib64/libteecl.so", RTLD_NOW);

    TEEC_AllocateSharedMemory =
            (typeof(TEEC_AllocateSharedMemory))dlsym(libteecl_handle, "TEEC_AllocateSharedMemory");
    TEEC_CloseSession = (typeof(TEEC_CloseSession))dlsym(libteecl_handle, "TEEC_CloseSession");
    TEEC_FinalizeContext =
            (typeof(TEEC_FinalizeContext))dlsym(libteecl_handle, "TEEC_FinalizeContext");
    TEEC_InitializeContext =
            (typeof(TEEC_InitializeContext))dlsym(libteecl_handle, "TEEC_InitializeContext");
    TEEC_InvokeCommand = (typeof(TEEC_InvokeCommand))dlsym(libteecl_handle, "TEEC_InvokeCommand");
    TEEC_ReleaseSharedMemory =
            (typeof(TEEC_ReleaseSharedMemory))dlsym(libteecl_handle, "TEEC_ReleaseSharedMemory");
    TEECS_OpenSession = (typeof(TEECS_OpenSession))dlsym(libteecl_handle, "TEECS_OpenSession");

    ABinderProcess_setThreadPoolMaxThreadCount(0);

    ::gatekeeper::TeegrisGateKeeper implementation;
    auto gatekeeper = ndk::SharedRefBase::make<TeegrisGateKeeperDevice>(implementation);
    const std::string instance = TeegrisGateKeeperDevice::descriptor + std::string("/default");
    auto status = AServiceManager_addService(gatekeeper->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    ABinderProcess_joinThreadPool();
    dlclose(libteecl_handle);
    return -1;  // Should never get here.
}
