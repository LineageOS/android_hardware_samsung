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

int main(int argc, char *argv[]) {
    std::string prop = FACTORY_PROP;

    if (argc > 1 && std::string(argv[1]) == "NetworkConfig")
        prop = TELEPHONY_PROP;

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
    } else if (android::base::EndsWith(prop, FACTORY_PROP)) {
        LOG(WARNING) << "Could not read " << prop << ", setting defaults!";
        LoadProperties("ro.vendor.multisim.simslotcount=1");
    } else {
        LOG(WARNING) << "Could not read " << prop << "!";
    }
}
