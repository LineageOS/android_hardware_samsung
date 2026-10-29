/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/ril.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace android::hardware::radio::samsung {

class VendorRil {
  public:
    using ResponseCallback = std::function<void(RIL_Errno error, const void* data, size_t length)>;
    using UnsolicitedCallback = std::function<void(int id, const void* data, size_t length)>;

    static VendorRil& get();

    void setUnsolicitedCallback(RIL_SOCKET_ID socket, UnsolicitedCallback callback);
    bool load(const std::string& path, const std::vector<std::string>& args);

    void request(RIL_SOCKET_ID socket, int id, const void* data, size_t length,
                 ResponseCallback callback);

    // SIM Access Profile, through the blob's RIL_SAP_Init. Ids are sap-api.proto MsgIds and
    // payloads are nanopb encoded.
    bool hasSap() const { return mSapFunctions != nullptr; }
    void setSapUnsolicitedCallback(RIL_SOCKET_ID socket, UnsolicitedCallback callback);
    void sapRequest(RIL_SOCKET_ID socket, int id, const void* data, size_t length,
                    ResponseCallback callback);
    RIL_RadioState radioState(RIL_SOCKET_ID socket);
    // Runs a task on the timer thread, where requests can be sent.
    void post(std::function<void()> task);
    const char* version();

    void setClientHalVersion(RIL_SOCKET_ID socket, int version);
    int clientHalVersion(RIL_SOCKET_ID socket);

  private:
    struct Request {
        int id;
        RIL_SOCKET_ID socket;
        ResponseCallback callback;
    };

    struct Task {
        std::chrono::steady_clock::time_point when;
        std::function<void()> run;

        bool operator>(const Task& other) const { return when > other.when; }
    };

    VendorRil() = default;

    void onRequestComplete(RIL_Token token, RIL_Errno error, void* response, size_t length);
    void onUnsolicitedResponse(int id, const void* data, size_t length, RIL_SOCKET_ID socket);
    void onSapUnsolicitedResponse(int id, const void* data, size_t length, RIL_SOCKET_ID socket);
    void deliver(const UnsolicitedCallback (&callbacks)[RIL_SOCKET_NUM], int id, const void* data,
                 size_t length, RIL_SOCKET_ID socket);
    void requestTimedCallback(RIL_TimedCallback callback, void* param,
                              const struct timeval* relativeTime);
    void onRequestAck(RIL_Token token);

    void dispatch(const SecRilFunctions* functions, RIL_SOCKET_ID socket, int id, const void* data,
                  size_t length, ResponseCallback callback);
    Request* takeRequest(RIL_Token token);
    void schedule(Task task);
    void timerLoop();

    const SecRilFunctions* mFunctions = nullptr;
    const SecRilFunctions* mSapFunctions = nullptr;
    std::atomic<int> mClientHalVersion[RIL_SOCKET_NUM] = {};

    std::mutex mRequestsMutex;
    std::unordered_set<Request*> mRequests;

    std::mutex mUnsolicitedMutex;
    UnsolicitedCallback mUnsolicited[RIL_SOCKET_NUM];
    UnsolicitedCallback mSapUnsolicited[RIL_SOCKET_NUM];

    std::mutex mTimerMutex;
    std::condition_variable mTimerCondition;
    std::priority_queue<Task, std::vector<Task>, std::greater<>> mTimers;
};

}  // namespace android::hardware::radio::samsung
