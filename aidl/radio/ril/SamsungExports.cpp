/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungExports"

#include <samsung_ril/VendorRil.h>

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <string>

using android::hardware::radio::samsung::VendorRil;

// Symbols libsec-ril imports from libril_sem. The file, command, settings and APN calls
// are only served by Samsung's framework, so the blob falls back to its defaults.

extern "C" {

int RIL_GetClientHalVersion(int slot) {
    // Offset past the HIDL 1.x version range the blob also checks for.
    return 20 + VendorRil::get().clientHalVersion(static_cast<RIL_SOCKET_ID>(slot));
}

unsigned char IsSecIntfExtention(int slot) {
    return 1;
}

void RIL_RegisterSehBridge(int slot) {
    android::base::SetProperty("ril.halservice.registered.slot" + std::to_string(slot + 1), "true");
}

int RIL_GetFd(int slot, const char* path, int flags, int mode) {
    LOG(DEBUG) << __func__ << "(" << slot << ", " << path << ") unsupported";
    return -1;
}

void RIL_Execute(int slot, const char* command) {
    LOG(DEBUG) << __func__ << "(" << slot << ", " << command << ") unsupported";
}

int RIL_GetSettingValue(int slot, const char* name, const char* table) {
    LOG(DEBUG) << __func__ << "(" << slot << ", " << table << "/" << name << ") unsupported";
    return -1;
}

int RIL_LoadApnProfFromDB(int slot, const char* selection, void* profile) {
    LOG(DEBUG) << __func__ << "(" << slot << ") unsupported";
    return -1;
}

int RIL_ReadPacketBytes(int slot, const char* interface, void* bytes) {
    return -1;
}

}  // extern "C"
