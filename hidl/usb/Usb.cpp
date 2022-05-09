/*
 * Copyright (C) 2020 The Android Open Source Project
 * Copyright (C) 2022 The LineageOS Project
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

#define LOG_TAG "android.hardware.usb@1.3-service.samsung"

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <regex>
#include <thread>
#include <unordered_map>

#include <cutils/uevent.h>
#include <sys/epoll.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

#include "Usb.h"

using android::base::GetProperty;

namespace android {
namespace hardware {
namespace usb {
namespace V1_3 {
namespace implementation {

Return<bool> Usb::enableUsbDataSignal(bool enable) {
    bool result = true;

    ALOGI("Userspace turn %s USB data signaling", enable ? "on" : "off");

    if(enable) {
        if (!WriteStringToFile("1", USB_DATA_PATH)) {
            ALOGE("Not able to turn on usb connection notification");
            result = false;
        }
    }
    else {
        if (!WriteStringToFile("0", USB_DATA_PATH)) {
            ALOGE("Not able to turn off usb connection notification");
            result = false;
        }
    }

    return result;
}

// Set by the signal handler to destroy the thread
volatile bool destroyThread;

bool moistureDetectionEnabled = true;
constexpr char kContaminantDetectionPath[] = "/sys/devices/virtual/sec/ccic/water";
constexpr char kTypecPath[] = "/sys/class/typec";
constexpr char kDisableContaminantDetection[] = "vendor.usb.contaminantdisable";

int32_t readFile(const std::string &filename, std::string *contents) {
    FILE *fp;
    ssize_t read = 0;
    char *line = NULL;
    size_t len = 0;

    fp = fopen(filename.c_str(), "r");
    if (fp != NULL) {
        if ((read = getline(&line, &len, fp)) != -1) {
            char *pos;
            if ((pos = strchr(line, '\n')) != NULL)
                *pos = '\0';
            *contents = line;
        }
        free(line);
        fclose(fp);
        return 0;
    } else {
        ALOGE("fopen failed");
    }

    return -1;
}

int32_t writeFile(const std::string &filename, const std::string &contents) {
    FILE *fp;
    std::string written;

    fp = fopen(filename.c_str(), "w");
    if (fp != NULL) {
        // FAILURE RETRY
        int ret = fputs(contents.c_str(), fp);
        fclose(fp);
        if ((ret != EOF) && !readFile(filename, &written) && written == contents)
            return 0;
    }
    return -1;
}

Status queryMoistureDetectionStatus(hidl_vec<PortStatus> *currentPortStatus_1_2) {
    std::string enabled, status, path, DetectedPath;

    if (currentPortStatus_1_2 == NULL || currentPortStatus_1_2->size() == 0) {
        ALOGE("currentPortStatus_1_2 is not available");
        return Status::ERROR;
    }

    (*currentPortStatus_1_2)[0].supportedContaminantProtectionModes = 0;
    (*currentPortStatus_1_2)[0].supportedContaminantProtectionModes |=
        V1_2::ContaminantProtectionMode::FORCE_SINK;
    (*currentPortStatus_1_2)[0].contaminantProtectionStatus = V1_2::ContaminantProtectionStatus::NONE;
    (*currentPortStatus_1_2)[0].contaminantDetectionStatus = V1_2::ContaminantDetectionStatus::DISABLED;
    (*currentPortStatus_1_2)[0].supportsEnableContaminantPresenceDetection = true;
    (*currentPortStatus_1_2)[0].supportsEnableContaminantPresenceProtection = false;

    if (moistureDetectionEnabled) {
        if (readFile(kContaminantDetectionPath, &status)) {
            ALOGE("Failed to open moisture_detected");
            return Status::ERROR;
        }
        if (status == "1") {
            (*currentPortStatus_1_2)[0].contaminantDetectionStatus =
                V1_2::ContaminantDetectionStatus::DETECTED;
            (*currentPortStatus_1_2)[0].contaminantProtectionStatus =
                V1_2::ContaminantProtectionStatus::FORCE_SINK;
        } else
            (*currentPortStatus_1_2)[0].contaminantDetectionStatus =
                V1_2::ContaminantDetectionStatus::NOT_DETECTED;
    }

     ALOGI("ContaminantDetectionStatus:%d ContaminantProtectionStatus:%d",
           (*currentPortStatus_1_2)[0].contaminantDetectionStatus,
           (*currentPortStatus_1_2)[0].contaminantProtectionStatus);

    return Status::SUCCESS;
}

std::string appendRoleNodeHelper(const std::string &portName, PortRoleType type) {
    std::string node("/sys/class/typec/" + portName);

    switch (type) {
        case PortRoleType::DATA_ROLE:
            return node + "/data_role";
        case PortRoleType::POWER_ROLE:
            return node + "/power_role";
        case PortRoleType::MODE:
            return node + "/port_type";
        default:
            return "";
    }
}

std::string convertRoletoString(PortRole role) {
    if (role.type == PortRoleType::POWER_ROLE) {
        if (role.role == static_cast<uint32_t>(PortPowerRole::SOURCE))
            return "source";
        else if (role.role == static_cast<uint32_t>(PortPowerRole::SINK))
            return "sink";
    } else if (role.type == PortRoleType::DATA_ROLE) {
        if (role.role == static_cast<uint32_t>(PortDataRole::HOST))
            return "host";
        if (role.role == static_cast<uint32_t>(PortDataRole::DEVICE))
            return "device";
    } else if (role.type == PortRoleType::MODE) {
        if (role.role == static_cast<uint32_t>(PortMode_1_1::UFP))
            return "sink";
        if (role.role == static_cast<uint32_t>(PortMode_1_1::DFP))
            return "source";
    }
    return "none";
}

void extractRole(std::string *roleName) {
    std::size_t first, last;

    first = roleName->find("[");
    last = roleName->find("]");

    if (first != std::string::npos && last != std::string::npos) {
        *roleName = roleName->substr(first + 1, last - first - 1);
    }
}

void switchToDrp(const std::string &portName) {
    std::string filename = appendRoleNodeHelper(std::string(portName.c_str()), PortRoleType::MODE);
    FILE *fp;

    if (filename != "") {
        fp = fopen(filename.c_str(), "w");
        if (fp != NULL) {
            int ret = fputs("dual", fp);
            fclose(fp);
            if (ret == EOF)
                ALOGE("Fatal: Error while switching back to drp");
        } else {
            ALOGE("Fatal: Cannot open file to switch back to drp");
        }
    } else {
        ALOGE("Fatal: invalid node type");
    }
}

bool switchMode(const hidl_string &portName, const PortRole &newRole, struct Usb *usb) {
    std::string filename = appendRoleNodeHelper(std::string(portName.c_str()), newRole.type);
    std::string written;
    FILE *fp;
    bool roleSwitch = false;

    if (filename == "") {
        ALOGE("Fatal: invalid node type");
        return false;
    }

    fp = fopen(filename.c_str(), "w");
    if (fp != NULL) {
        // Hold the lock here to prevent loosing connected signals
        // as once the file is written the partner added signal
        // can arrive anytime.
        pthread_mutex_lock(&usb->mPartnerLock);
        usb->mPartnerUp = false;
        int ret = fputs(convertRoletoString(newRole).c_str(), fp);
        fclose(fp);

        if (ret != EOF) {
            struct timespec to;
            struct timespec now;

        wait_again:
            clock_gettime(CLOCK_MONOTONIC, &now);
            to.tv_sec = now.tv_sec + PORT_TYPE_TIMEOUT;
            to.tv_nsec = now.tv_nsec;

            int err = pthread_cond_timedwait(&usb->mPartnerCV, &usb->mPartnerLock, &to);
            // There are no uevent signals which implies role swap timed out.
            if (err == ETIMEDOUT) {
                ALOGI("uevents wait timedout");
                // Validity check.
            } else if (!usb->mPartnerUp) {
                goto wait_again;
                // Role switch succeeded since usb->mPartnerUp is true.
            } else {
                roleSwitch = true;
            }
        } else {
            ALOGI("Role switch failed while wrting to file");
        }
        pthread_mutex_unlock(&usb->mPartnerLock);
    }

    if (!roleSwitch)
        switchToDrp(std::string(portName.c_str()));

    return roleSwitch;
}

Usb::Usb()
    : mLock(PTHREAD_MUTEX_INITIALIZER),
      mRoleSwitchLock(PTHREAD_MUTEX_INITIALIZER),
      mPartnerLock(PTHREAD_MUTEX_INITIALIZER),
      mPartnerUp(false) {
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) {
        ALOGE("pthread_condattr_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
        ALOGE("pthread_condattr_setclock failed: %s", strerror(errno));
        abort();
    }
    if (pthread_cond_init(&mPartnerCV, &attr)) {
        ALOGE("pthread_cond_init failed: %s", strerror(errno));
        abort();
    }
    if (pthread_condattr_destroy(&attr)) {
        ALOGE("pthread_condattr_destroy failed: %s", strerror(errno));
        abort();
    }
}

Return<void> Usb::switchRole(const hidl_string &portName, const V1_0::PortRole &newRole) {
    std::string filename = appendRoleNodeHelper(std::string(portName.c_str()), newRole.type);
    std::string written;
    FILE *fp;
    bool roleSwitch = false;

    if (filename == "") {
        ALOGE("Fatal: invalid node type");
        return Void();
    }

    pthread_mutex_lock(&mRoleSwitchLock);

    ALOGI("filename write: %s role:%s", filename.c_str(), convertRoletoString(newRole).c_str());

    if (newRole.type == PortRoleType::MODE) {
        roleSwitch = switchMode(portName, newRole, this);
    } else {
        fp = fopen(filename.c_str(), "w");
        if (fp != NULL) {
            int ret = fputs(convertRoletoString(newRole).c_str(), fp);
            fclose(fp);
            if ((ret != EOF) && !readFile(filename, &written)) {
                extractRole(&written);
                ALOGI("written: %s", written.c_str());
                if (written == convertRoletoString(newRole)) {
                    roleSwitch = true;
                } else {
                    ALOGE("Role switch failed");
                }
            } else {
                ALOGE("failed to update the new role");
            }
        } else {
            ALOGE("fopen failed");
        }
    }

    pthread_mutex_lock(&mLock);
    if (mCallback_1_0 != NULL) {
        Return<void> ret = mCallback_1_0->notifyRoleSwitchStatus(
            portName, newRole, roleSwitch ? Status::SUCCESS : Status::ERROR);
        if (!ret.isOk())
            ALOGE("RoleSwitchStatus error %s", ret.description().c_str());
    } else {
        ALOGE("Not notifying the userspace. Callback is not set");
    }
    pthread_mutex_unlock(&mLock);
    pthread_mutex_unlock(&mRoleSwitchLock);

    return Void();
}

Status getAccessoryConnected(const std::string &portName, std::string *accessory) {
    std::string filename = "/sys/class/typec/" + portName + "-partner/accessory_mode";

    if (readFile(filename, accessory)) {
        ALOGE("getAccessoryConnected: Failed to open filesystem node: %s", filename.c_str());
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

Status getCurrentRoleHelper(const std::string &portName, bool connected, PortRoleType type,
                            uint32_t *currentRole) {
    std::string filename;
    std::string roleName;
    std::string accessory;

    // Mode

    if (type == PortRoleType::POWER_ROLE) {
        filename = "/sys/class/typec/" + portName + "/power_role";
        *currentRole = static_cast<uint32_t>(PortPowerRole::NONE);
    } else if (type == PortRoleType::DATA_ROLE) {
        filename = "/sys/class/typec/" + portName + "/data_role";
        *currentRole = static_cast<uint32_t>(PortDataRole::NONE);
    } else if (type == PortRoleType::MODE) {
        filename = "/sys/class/typec/" + portName + "/data_role";
        *currentRole = static_cast<uint32_t>(PortMode_1_1::NONE);
    } else {
        return Status::ERROR;
    }

    if (!connected)
        return Status::SUCCESS;

    if (type == PortRoleType::MODE) {
        if (getAccessoryConnected(portName, &accessory) != Status::SUCCESS) {
            return Status::ERROR;
        }
        if (accessory == "analog_audio") {
            *currentRole = static_cast<uint32_t>(PortMode_1_1::AUDIO_ACCESSORY);
            return Status::SUCCESS;
        } else if (accessory == "debug") {
            *currentRole = static_cast<uint32_t>(PortMode_1_1::DEBUG_ACCESSORY);
            return Status::SUCCESS;
        }
    }

    if (readFile(filename, &roleName)) {
        ALOGE("getCurrentRole: Failed to open filesystem node: %s", filename.c_str());
        return Status::ERROR;
    }

    extractRole(&roleName);

    if (roleName == "source") {
        *currentRole = static_cast<uint32_t>(PortPowerRole::SOURCE);
    } else if (roleName == "sink") {
        *currentRole = static_cast<uint32_t>(PortPowerRole::SINK);
    } else if (roleName == "host") {
        if (type == PortRoleType::DATA_ROLE)
            *currentRole = static_cast<uint32_t>(PortDataRole::HOST);
        else
            *currentRole = static_cast<uint32_t>(PortMode_1_1::DFP);
    } else if (roleName == "device") {
        if (type == PortRoleType::DATA_ROLE)
            *currentRole = static_cast<uint32_t>(PortDataRole::DEVICE);
        else
            *currentRole = static_cast<uint32_t>(PortMode_1_1::UFP);
    } else if (roleName != "none") {
        /* case for none has already been addressed.
         * so we check if the role isn't none.
         */
        return Status::UNRECOGNIZED_ROLE;
    }

    return Status::SUCCESS;
}

Status getTypeCPortNamesHelper(std::unordered_map<std::string, bool> *names) {
    DIR *dp;

    dp = opendir(kTypecPath);
    if (dp != NULL) {
        struct dirent *ep;

        while ((ep = readdir(dp))) {
            if (ep->d_type == DT_LNK) {
                if (std::string::npos == std::string(ep->d_name).find("-partner")) {
                    std::unordered_map<std::string, bool>::const_iterator portName =
                        names->find(ep->d_name);
                    if (portName == names->end()) {
                        names->insert({ep->d_name, false});
                    }
                } else {
                    (*names)[std::strtok(ep->d_name, "-")] = true;
                }
            }
        }
        closedir(dp);
        return Status::SUCCESS;
    }

    ALOGE("Failed to open /sys/class/typec");
    return Status::ERROR;
}

bool canSwitchRoleHelper(const std::string &portName, PortRoleType /*type*/) {
    std::string filename = "/sys/class/typec/" + portName + "-partner/supports_usb_power_delivery";
    std::string supportsPD;

    if (!readFile(filename, &supportsPD)) {
        if (supportsPD == "yes") {
            return true;
        }
    }

    return false;
}

/*
 * Reuse the same method for both V1_0 and V1_1 callback objects.
 * The caller of this method would reconstruct the V1_0::PortStatus
 * object if required.
 */
Status getPortStatusHelper(hidl_vec<PortStatus> *currentPortStatus_1_2, HALVersion version) {
    std::unordered_map<std::string, bool> names;
    Status result = getTypeCPortNamesHelper(&names);
    int i = -1;

    if (result == Status::SUCCESS) {
        currentPortStatus_1_2->resize(names.size());
        for (std::pair<std::string, bool> port : names) {
            i++;
            ALOGI("%s", port.first.c_str());
            (*currentPortStatus_1_2)[i].status_1_1.status.portName = port.first;

            uint32_t currentRole;
            if (getCurrentRoleHelper(port.first, port.second, PortRoleType::POWER_ROLE,
                                     &currentRole) == Status::SUCCESS) {
                (*currentPortStatus_1_2)[i].status_1_1.status.currentPowerRole =
                    static_cast<PortPowerRole>(currentRole);
            } else {
                ALOGE("Error while retrieving portNames");
                goto done;
            }

            if (getCurrentRoleHelper(port.first, port.second, PortRoleType::DATA_ROLE,
                                     &currentRole) == Status::SUCCESS) {
                (*currentPortStatus_1_2)[i].status_1_1.status.currentDataRole =
                    static_cast<PortDataRole>(currentRole);
            } else {
                ALOGE("Error while retrieving current port role");
                goto done;
            }

            if (getCurrentRoleHelper(port.first, port.second, PortRoleType::MODE, &currentRole) ==
                Status::SUCCESS) {
                (*currentPortStatus_1_2)[i].status_1_1.currentMode =
                    static_cast<PortMode_1_1>(currentRole);
                (*currentPortStatus_1_2)[i].status_1_1.status.currentMode =
                    static_cast<V1_0::PortMode>(currentRole);
            } else {
                ALOGE("Error while retrieving current data role");
                goto done;
            }

            (*currentPortStatus_1_2)[i].status_1_1.status.canChangeMode = true;
            (*currentPortStatus_1_2)[i].status_1_1.status.canChangeDataRole =
                port.second ? canSwitchRoleHelper(port.first, PortRoleType::DATA_ROLE) : false;
            (*currentPortStatus_1_2)[i].status_1_1.status.canChangePowerRole =
                port.second ? canSwitchRoleHelper(port.first, PortRoleType::POWER_ROLE) : false;

            if (version == HALVersion::V1_0) {
                ALOGI("HAL version V1_0");
                (*currentPortStatus_1_2)[i].status_1_1.status.supportedModes = V1_0::PortMode::DFP;
            } else {
                if (version == HALVersion::V1_1)
                    ALOGI("HAL version V1_1");
                else
                    ALOGI("HAL version V1_2");
                (*currentPortStatus_1_2)[i].status_1_1.supportedModes = PortMode_1_1::UFP | PortMode_1_1::DFP;
                (*currentPortStatus_1_2)[i].status_1_1.status.supportedModes = V1_0::PortMode::NONE;
                (*currentPortStatus_1_2)[i].status_1_1.status.currentMode = V1_0::PortMode::NONE;
            }

            ALOGI(
                "%d:%s connected:%d canChangeMode:%d canChagedata:%d canChangePower:%d "
                "supportedModes:%d",
                i, port.first.c_str(), port.second,
                (*currentPortStatus_1_2)[i].status_1_1.status.canChangeMode,
                (*currentPortStatus_1_2)[i].status_1_1.status.canChangeDataRole,
                (*currentPortStatus_1_2)[i].status_1_1.status.canChangePowerRole,
                (*currentPortStatus_1_2)[i].status_1_1.supportedModes);
        }
        return Status::SUCCESS;
    }
done:
    return Status::ERROR;
}

void queryVersionHelper(android::hardware::usb::V1_3::implementation::Usb *usb,
                        hidl_vec<PortStatus> *currentPortStatus_1_2) {
    hidl_vec<V1_1::PortStatus_1_1> currentPortStatus_1_1;
    hidl_vec<V1_0::PortStatus> currentPortStatus;
    Status status;
    sp<V1_1::IUsbCallback> callback_V1_1 = V1_1::IUsbCallback::castFrom(usb->mCallback_1_0);
    sp<IUsbCallback> callback_V1_2 = IUsbCallback::castFrom(usb->mCallback_1_0);

    pthread_mutex_lock(&usb->mLock);
    if (usb->mCallback_1_0 != NULL) {
        if (callback_V1_2 != NULL) {
            status = getPortStatusHelper(currentPortStatus_1_2, HALVersion::V1_2);
            if (status == Status::SUCCESS)
                queryMoistureDetectionStatus(currentPortStatus_1_2);
        } else if (callback_V1_1 != NULL) {
            status = getPortStatusHelper(currentPortStatus_1_2, HALVersion::V1_1);
            currentPortStatus_1_1.resize(currentPortStatus_1_2->size());
            for (unsigned long i = 0; i < currentPortStatus_1_2->size(); i++)
                currentPortStatus_1_1[i] = (*currentPortStatus_1_2)[i].status_1_1;
        } else {
            status = getPortStatusHelper(currentPortStatus_1_2, HALVersion::V1_0);
            currentPortStatus.resize(currentPortStatus_1_2->size());
            for (unsigned long i = 0; i < currentPortStatus_1_2->size(); i++)
                currentPortStatus[i] = (*currentPortStatus_1_2)[i].status_1_1.status;
        }

        Return<void> ret;

        if (callback_V1_2 != NULL)
            ret = callback_V1_2->notifyPortStatusChange_1_2(*currentPortStatus_1_2, status);
        else if (callback_V1_1 != NULL)
            ret = callback_V1_1->notifyPortStatusChange_1_1(currentPortStatus_1_1, status);
        else
            ret = usb->mCallback_1_0->notifyPortStatusChange(currentPortStatus, status);

        if (!ret.isOk())
            ALOGE("queryPortStatus_1_2 error %s", ret.description().c_str());
    } else {
        ALOGI("Notifying userspace skipped. Callback is NULL");
    }
    pthread_mutex_unlock(&usb->mLock);
}

Return<void> Usb::queryPortStatus() {
    hidl_vec<PortStatus> currentPortStatus_1_2;

    queryVersionHelper(this, &currentPortStatus_1_2);
    return Void();
}

Return<void> Usb::enableContaminantPresenceDetection(const hidl_string & /*portName*/,
                                                     bool enable) {
    std::string disable = GetProperty(kDisableContaminantDetection, "");

    if (disable != "true")
        moistureDetectionEnabled = enable;

    hidl_vec<PortStatus> currentPortStatus_1_2;

    queryVersionHelper(this, &currentPortStatus_1_2);
    return Void();
}

Return<void> Usb::enableContaminantPresenceProtection(const hidl_string & /*portName*/,
                                                      bool /*enable*/) {
    hidl_vec<PortStatus> currentPortStatus_1_2;

    queryVersionHelper(this, &currentPortStatus_1_2);
    return Void();
}

struct data {
    int uevent_fd;
    android::hardware::usb::V1_3::implementation::Usb *usb;
};

static void uevent_event(uint32_t /*epevents*/, struct data *payload) {
    char msg[UEVENT_MSG_LEN + 2];
    char *cp;
    int n;

    n = uevent_kernel_multicast_recv(payload->uevent_fd, msg, UEVENT_MSG_LEN);
    if (n <= 0)
        return;
    if (n >= UEVENT_MSG_LEN) /* overflow -- discard */
        return;

    msg[n] = '\0';
    msg[n + 1] = '\0';
    cp = msg;

    while (*cp) {
        if (std::regex_match(cp, std::regex("(add)(.*)(-partner)"))) {
            ALOGI("partner added");
            pthread_mutex_lock(&payload->usb->mPartnerLock);
            payload->usb->mPartnerUp = true;
            pthread_cond_signal(&payload->usb->mPartnerCV);
            pthread_mutex_unlock(&payload->usb->mPartnerLock);
        } else if (!strncmp(cp, "DEVTYPE=typec_", strlen("DEVTYPE=typec_")) ||
                   !strncmp(cp, "CCIC=WATER", strlen("CCIC=WATER")) ||
                   !strncmp(cp, "CCIC=DRY", strlen("CCIC=DRY"))) {
            hidl_vec<PortStatus> currentPortStatus_1_2;
            queryVersionHelper(payload->usb, &currentPortStatus_1_2);

            // Role switch is not in progress and port is in disconnected state
            if (!pthread_mutex_trylock(&payload->usb->mRoleSwitchLock)) {
                for (unsigned long i = 0; i < currentPortStatus_1_2.size(); i++) {
                    DIR *dp =
                        opendir(std::string("/sys/class/typec/" +
                                            std::string(currentPortStatus_1_2[i]
                                                            .status_1_1.status.portName.c_str()) +
                                            "-partner")
                                    .c_str());
                    if (dp == NULL) {
                        // PortRole role = {.role = static_cast<uint32_t>(PortMode::UFP)};
                        switchToDrp(currentPortStatus_1_2[i].status_1_1.status.portName);
                    } else {
                        closedir(dp);
                    }
                }
                pthread_mutex_unlock(&payload->usb->mRoleSwitchLock);
            }
            break;
        }
        /* advance to after the next \0 */
        while (*cp++) {
        }
    }
}

void *work(void *param) {
    int epoll_fd, uevent_fd;
    struct epoll_event ev;
    int nevents = 0;
    struct data payload;

    ALOGE("creating thread");

    uevent_fd = uevent_open_socket(64 * 1024, true);

    if (uevent_fd < 0) {
        ALOGE("uevent_init: uevent_open_socket failed\n");
        return NULL;
    }

    payload.uevent_fd = uevent_fd;
    payload.usb = (android::hardware::usb::V1_3::implementation::Usb *)param;

    fcntl(uevent_fd, F_SETFL, O_NONBLOCK);

    ev.events = EPOLLIN;
    ev.data.ptr = (void *)uevent_event;

    epoll_fd = epoll_create(64);
    if (epoll_fd == -1) {
        ALOGE("epoll_create failed; errno=%d", errno);
        goto error;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, uevent_fd, &ev) == -1) {
        ALOGE("epoll_ctl failed; errno=%d", errno);
        goto error;
    }

    while (!destroyThread) {
        struct epoll_event events[64];

        nevents = epoll_wait(epoll_fd, events, 64, -1);
        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("usb epoll_wait failed; errno=%d", errno);
            break;
        }

        for (int n = 0; n < nevents; ++n) {
            if (events[n].data.ptr)
                (*(void (*)(int, struct data *payload))events[n].data.ptr)(events[n].events,
                                                                           &payload);
        }
    }

    ALOGI("exiting worker thread");
error:
    close(uevent_fd);

    if (epoll_fd >= 0)
        close(epoll_fd);

    return NULL;
}

void sighandler(int sig) {
    if (sig == SIGUSR1) {
        destroyThread = true;
        ALOGI("destroy set");
        return;
    }
    signal(SIGUSR1, sighandler);
}

Return<void> Usb::setCallback(const sp<V1_0::IUsbCallback> &callback) {
    sp<V1_1::IUsbCallback> callback_V1_1 = V1_1::IUsbCallback::castFrom(callback);
    sp<IUsbCallback> callback_V1_2 = IUsbCallback::castFrom(callback);

    if (callback != NULL) {
        if (callback_V1_2 != NULL)
            ALOGI("Registering 1.2 callback");
        else if (callback_V1_1 != NULL)
            ALOGI("Registering 1.1 callback");
    }

    pthread_mutex_lock(&mLock);
    /*
     * When both the old callback and new callback values are NULL,
     * there is no need to spin off the worker thread.
     * When both the values are not NULL, we would already have a
     * worker thread running, so updating the callback object would
     * be suffice.
     */
    if ((mCallback_1_0 == NULL && callback == NULL) ||
        (mCallback_1_0 != NULL && callback != NULL)) {
        /*
         * Always store as V1_0 callback object. Type cast to V1_1
         * when the callback is actually invoked.
         */
        mCallback_1_0 = callback;
        pthread_mutex_unlock(&mLock);
        return Void();
    }

    mCallback_1_0 = callback;
    ALOGI("registering callback");

    // Kill the worker thread if the new callback is NULL.
    if (mCallback_1_0 == NULL) {
        pthread_mutex_unlock(&mLock);
        if (!pthread_kill(mPoll, SIGUSR1)) {
            pthread_join(mPoll, NULL);
            ALOGI("pthread destroyed");
        }
        return Void();
    }

    destroyThread = false;
    signal(SIGUSR1, sighandler);

    /*
     * Create a background thread if the old callback value is NULL
     * and being updated with a new value.
     */
    if (pthread_create(&mPoll, NULL, work, this)) {
        ALOGE("pthread creation failed %d", errno);
        mCallback_1_0 = NULL;
    }

    pthread_mutex_unlock(&mLock);
    return Void();
}

}  // namespace implementation
}  // namespace V1_3
}  // namespace usb
}  // namespace hardware
}  // namespace android
