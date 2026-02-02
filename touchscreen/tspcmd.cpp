/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "libsamsungtspcmd"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <fstream>
#include <utility>

#include "tspcmd.h"

using android::base::WriteStringToFd;

TspCmdHandler::TspCmdHandler() {
    std::ifstream stream("/sys/class/sec/tsp/cmd_list");

    std::string temp;
    while (std::getline(stream, temp)) {
        if (temp.starts_with("++")) continue;
        if (!temp.empty()) mSupportedCommands.emplace(temp);
    }
    mFd = unique_fd(open("/sys/class/sec/tsp/cmd", O_WRONLY));
    if (!mFd.ok()) {
        LOG(ERROR) << "Failed to open /sys/class/sec/tsp/cmd: " << strerror(errno);
    }
}

bool TspCmdHandler::sendCommand(const std::string& command,
                                const std::vector<std::string>& parameters) {
    if (!isCommandSupported(command)) return false;

    std::string cmd = command;
    for (auto& param : parameters) {
        cmd.append(",").append(param);
    }

    if (!WriteStringToFd(cmd, mFd)) {
        LOG(ERROR) << "Failed to send command " << cmd << ": " << strerror(errno);
        return false;
    }
    return true;
}
