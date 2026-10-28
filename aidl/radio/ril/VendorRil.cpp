/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungRil"

#include <samsung_ril/VendorRil.h>

#include <android-base/logging.h>
#include <dlfcn.h>

#include <memory>

namespace android::hardware::radio::samsung {

namespace {

using RilInit = const RIL_RadioFunctions* (*)(const RIL_Env*, int, char**);

}  // namespace

VendorRil& VendorRil::get() {
    static VendorRil instance;
    return instance;
}

void VendorRil::setUnsolicitedCallback(RIL_SOCKET_ID socket, UnsolicitedCallback callback) {
    std::lock_guard lock(mUnsolicitedMutex);
    mUnsolicited[socket] = std::move(callback);
}

bool VendorRil::load(const std::string& path, const std::vector<std::string>& args) {
    static const RIL_Env env = {
            .OnRequestComplete = [](RIL_Token t, RIL_Errno e, void* r,
                                    size_t l) { get().onRequestComplete(t, e, r, l); },
            .OnUnsolicitedResponse =
                    [](int id, const void* d, size_t l, RIL_SOCKET_ID s) {
                        get().onUnsolicitedResponse(id, d, l, s);
                    },
            .RequestTimedCallback =
                    [](RIL_TimedCallback c, void* p, const struct timeval* t) {
                        get().requestTimedCallback(c, p, t);
                    },
            .OnRequestAck = [](RIL_Token t) { get().onRequestAck(t); },
    };

    void* handle = dlopen(path.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        LOG(ERROR) << "dlopen " << path << " failed: " << dlerror();
        return false;
    }

    auto init = reinterpret_cast<RilInit>(dlsym(handle, "RIL_Init"));
    if (init == nullptr) {
        LOG(ERROR) << "RIL_Init not found in " << path;
        return false;
    }

    std::thread(&VendorRil::timerLoop, this).detach();

    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    // RIL_Init blocks until every slot's modem init thread has finished.
    mFunctions = reinterpret_cast<const SecRilFunctions*>(
            init(&env, static_cast<int>(argv.size() - 1), argv.data()));
    if (mFunctions == nullptr) {
        LOG(ERROR) << "RIL_Init failed";
        return false;
    }

    LOG(INFO) << "Loaded " << path << ", RIL version " << mFunctions->version << ", " << version();
    return true;
}

void VendorRil::request(RIL_SOCKET_ID socket, int id, const void* data, size_t length,
                        ResponseCallback callback) {
    auto* request = new Request{id, socket, std::move(callback)};
    {
        std::lock_guard lock(mRequestsMutex);
        mRequests.insert(request);
    }
    mFunctions->onRequest(id, const_cast<void*>(data), length, request, socket);
}

RIL_RadioState VendorRil::radioState(RIL_SOCKET_ID socket) {
    return mFunctions->onStateRequest(socket);
}

const char* VendorRil::version() {
    return mFunctions->getVersion != nullptr ? mFunctions->getVersion() : "unknown";
}

void VendorRil::setClientHalVersion(RIL_SOCKET_ID socket, int version) {
    mClientHalVersion[socket] = version;
}

int VendorRil::clientHalVersion(RIL_SOCKET_ID socket) {
    return socket >= 0 && socket < RIL_SOCKET_NUM ? mClientHalVersion[socket].load() : 0;
}

VendorRil::Request* VendorRil::takeRequest(RIL_Token token) {
    auto* request = static_cast<Request*>(token);
    std::lock_guard lock(mRequestsMutex);
    if (mRequests.erase(request) == 0) {
        LOG(ERROR) << "Unknown RIL token " << token;
        return nullptr;
    }
    return request;
}

void VendorRil::onRequestComplete(RIL_Token token, RIL_Errno error, void* response, size_t length) {
    std::unique_ptr<Request> request(takeRequest(token));
    if (request == nullptr) {
        return;
    }
    LOG(VERBOSE) << "Request " << request->id << " on socket " << request->socket
                 << " completed: " << error;
    if (request->callback) {
        request->callback(error, response, length);
    }
}

void VendorRil::onUnsolicitedResponse(int id, const void* data, size_t length,
                                      RIL_SOCKET_ID socket) {
    UnsolicitedCallback callback;
    {
        std::lock_guard lock(mUnsolicitedMutex);
        if (socket >= 0 && socket < RIL_SOCKET_NUM) {
            callback = mUnsolicited[socket];
        }
    }
    if (!callback) {
        LOG(WARNING) << "Dropping unsolicited " << id << " for socket " << socket;
        return;
    }
    callback(id, data, length);
}

void VendorRil::requestTimedCallback(RIL_TimedCallback callback, void* param,
                                     const struct timeval* relativeTime) {
    auto when = std::chrono::steady_clock::now();
    if (relativeTime != nullptr) {
        when += std::chrono::seconds(relativeTime->tv_sec) +
                std::chrono::microseconds(relativeTime->tv_usec);
    }
    schedule({when, [callback, param] { callback(param); }});
}

void VendorRil::post(std::function<void()> task) {
    schedule({std::chrono::steady_clock::now(), std::move(task)});
}

void VendorRil::schedule(Task task) {
    {
        std::lock_guard lock(mTimerMutex);
        mTimers.push(std::move(task));
    }
    mTimerCondition.notify_one();
}

void VendorRil::onRequestAck(RIL_Token token) {
    LOG(VERBOSE) << "Request ack for token " << token;
}

void VendorRil::timerLoop() {
    std::unique_lock lock(mTimerMutex);
    while (true) {
        if (mTimers.empty()) {
            mTimerCondition.wait(lock);
            continue;
        }
        if (mTimers.top().when > std::chrono::steady_clock::now()) {
            mTimerCondition.wait_until(lock, mTimers.top().when);
            continue;
        }
        auto next = mTimers.top();
        mTimers.pop();
        lock.unlock();
        next.run();
        lock.lock();
    }
}

}  // namespace android::hardware::radio::samsung
