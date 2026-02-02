/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/unique_fd.h>
#include <set>

#pragma once

using android::base::unique_fd;

class TspCmdHandler {
  public:
    TspCmdHandler();

    bool isCommandSupported(std::string command) { return mSupportedCommands.contains(command); }

    bool sendCommand(const std::string& command, const std::vector<std::string>& parameters);
    template <typename... T>
    inline bool sendCommand(const std::string& command, T... parameters) {
        std::vector<std::string> newParameters;
        for (const auto& in : {parameters...}) {
            newParameters.push_back(std::to_string(in));
        }
        return sendCommand(command, newParameters);
    }

  private:
    unique_fd mFd;
    std::set<std::string> mSupportedCommands;
};
