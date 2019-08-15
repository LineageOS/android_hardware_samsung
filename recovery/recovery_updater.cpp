/*
 * Copyright (C) 2019, The LineageOS Project
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

#include <android-base/properties.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "edify/expr.h"
#include "otautil/error_code.h"
#include "updater/install.h"

/* convert bootloader version string to int */
int blToInt (const char *bootloader) {
    int ret = 0;

    for (size_t i = 0; i < strlen(bootloader)-2; i++) {
        ret += (int)bootloader[i];
    }

  return ret;
}

/* compare_bootloader("BL_VERSION", "BL_VERSION", ...) */
Value * CompareBootloaderFn(const char *name, State *state,
                          const std::vector<std::unique_ptr<Expr>>& argv) {
    std::string bootloader = android::base::GetProperty("ro.bootloader", "");

    if (bootloader.empty()) {
        return ErrorAbort(state, kFreadFailure, "%s() failed to read current Bootloader version",
                name);
    }

    std::vector<std::string> args;
    if (!ReadArgs(state, argv, &args)) {
        return ErrorAbort(state, kArgsParsingFailure, "%s() error parsing arguments", name);
    }

    for (auto& bl_version : args) {
        uiPrintf(state, "Comparing bootloader version %s to %s",
                bl_version.c_str(), bootloader.c_str());

        if (blToInt(bl_version.c_str()) >= blToInt(bootloader.c_str())) {
                return StringValue(strdup("1"));
        }
    }

    return StringValue(strdup("0"));
}

void Register_librecovery_updater_exynos() {
    RegisterFunction("samsung.compare_bootloader", CompareBootloaderFn);
}
