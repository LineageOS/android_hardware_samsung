/*
 * Copyright (C) 2021, The LineageOS Project
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

#define LOG_TAG "secril_config_svc"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <fstream>

#define EFS_OLD "/efs/"
#define EFS_NEW "/mnt/vendor/efs/"
#define FACTORY_PROP "factory.prop"
#define TELEPHONY_PROP "telephony.prop"

void LoadProperties(std::string data) {
    for (std::string line : android::base::Split(data, "\n")) {
        if (line == "\0") break;

        std::vector<std::string> parts = android::base::Split(line, "=");
        if (parts.size() == 2) {
            LOG(INFO) << "Setting property: " << line;
            android::base::SetProperty(parts.at(0), parts.at(1));
        } else {
            LOG(ERROR) << "Invalid data: " << line;
        }
    }
}

std::string ReadProperty(const std::string& prop_file, const std::string& property) {
    std::ifstream in(prop_file);
    if (!in.is_open()) {
        LOG(INFO) << "Cannot open file: " << prop_file;
        return std::string();
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = android::base::Trim(line.substr(0, pos));
        std::string val = android::base::Trim(line.substr(pos + 1));
        if (key == property) {
            return val;
        }
    }

    return std::string();
}

int main(int argc, char *argv[]) {
    bool isNetworkConfig = (argc > 1 && std::string(argv[1]) == "NetworkConfig");

    std::string prop = isNetworkConfig ? TELEPHONY_PROP : FACTORY_PROP;

    std::ifstream in(EFS_NEW + prop);
    if (in.good()) {
        in.close();
        prop = EFS_NEW + prop;
    } else {
        prop = EFS_OLD + prop;
    }

    LOG(INFO) << "Loading properties from " << prop;

    std::string content;
    if (android::base::ReadFileToString(prop, &content)) {
        LoadProperties(content.c_str());
    } else if (!isNetworkConfig) {
        LOG(WARNING) << "Could not read " << prop << ", setting defaults!";
        android::base::SetProperty("ro.multisim.simslotcount", "1");
        android::base::SetProperty("ro.vendor.multisim.simslotcount", "1");
        android::base::SetProperty("persist.radio.multisim.config", "ss");
    } else {
        LOG(WARNING) << "Could not read " << prop << "!";
    }

    if (isNetworkConfig) {
        content = ReadProperty(prop, "persist.radio.def_network");
        if (!content.empty()) {
            android::base::SetProperty("ro.vendor.radio.default_network", content);
        }
    } else {
        content = ReadProperty(prop, "ro.multisim.simslotcount");
        if (!content.empty()) {
            android::base::SetProperty("ro.telephony.sim_slots.count", content);
        }
    }
}
