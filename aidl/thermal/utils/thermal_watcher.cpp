/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define ATRACE_TAG (ATRACE_TAG_THERMAL | ATRACE_TAG_HAL)

#include "thermal_watcher.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cutils/uevent.h>
#include <dirent.h>
#include <linux/netlink.h>
#include <linux/thermal.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <utils/Trace.h>

#include <chrono>
#include <fstream>

#include "../thermal-helper.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

namespace {

static int nlErrorHandle(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
    int *ret = reinterpret_cast<int *>(arg);
    *ret = err->error;
    LOG(ERROR) << __func__ << "nl_groups: " << nla->nl_groups << ", nl_pid: " << nla->nl_pid;

    return NL_STOP;
}

static int nlFinishHandle(struct nl_msg *msg, void *arg) {
    int *ret = reinterpret_cast<int *>(arg);
    *ret = 1;
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    LOG(VERBOSE) << __func__ << ": nlmsg type: " << nlh->nlmsg_type;

    return NL_OK;
}

static int nlAckHandle(struct nl_msg *msg, void *arg) {
    int *ret = reinterpret_cast<int *>(arg);
    *ret = 1;
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    LOG(VERBOSE) << __func__ << ": nlmsg type: " << nlh->nlmsg_type;

    return NL_OK;
}

static int nlSeqCheckHandle(struct nl_msg *msg, void *arg) {
    int *ret = reinterpret_cast<int *>(arg);
    *ret = 1;
    struct nlmsghdr *nlh = nlmsg_hdr(msg);

    LOG(VERBOSE) << __func__ << ": nlmsg type: " << nlh->nlmsg_type;

    return NL_OK;
}

struct HandlerArgs {
    const char *group;
    int id;
};

static int nlSendMsg(struct nl_sock *sock, struct nl_msg *msg,
                     int (*rx_handler)(struct nl_msg *, void *), void *data) {
    int err, done = 0;

    std::unique_ptr<nl_cb, decltype(&nl_cb_put)> cb(nl_cb_alloc(NL_CB_DEFAULT), nl_cb_put);

    err = nl_send_auto_complete(sock, msg);
    if (err < 0)
        return err;

    err = 0;
    nl_cb_err(cb.get(), NL_CB_CUSTOM, nlErrorHandle, &err);
    nl_cb_set(cb.get(), NL_CB_FINISH, NL_CB_CUSTOM, nlFinishHandle, &done);
    nl_cb_set(cb.get(), NL_CB_ACK, NL_CB_CUSTOM, nlAckHandle, &done);

    if (rx_handler != NULL)
        nl_cb_set(cb.get(), NL_CB_VALID, NL_CB_CUSTOM, rx_handler, data);

    while (err == 0 && done == 0) nl_recvmsgs(sock, cb.get());

    return err;
}

static int nlFamilyHandle(struct nl_msg *msg, void *arg) {
    struct HandlerArgs *grp = reinterpret_cast<struct HandlerArgs *>(arg);
    struct nlattr *tb[CTRL_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *mcgrp;
    int rem_mcgrp;

    nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb[CTRL_ATTR_MCAST_GROUPS]) {
        LOG(ERROR) << __func__ << "Multicast group not found";
        return -1;
    }

    nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], rem_mcgrp) {
        struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

        nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX, reinterpret_cast<nlattr *>(nla_data(mcgrp)),
                  nla_len(mcgrp), NULL);

        if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] || !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
            continue;

        if (strncmp(reinterpret_cast<char *>(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])),
                    grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])) != 0)
            continue;

        grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);

        break;
    }

    return 0;
}

static int nlGetMulticastId(struct nl_sock *sock, const char *family, const char *group) {
    int err = 0, ctrlid;
    struct HandlerArgs grp = {
            .group = group,
            .id = -ENOENT,
    };

    std::unique_ptr<nl_msg, decltype(&nlmsg_free)> msg(nlmsg_alloc(), nlmsg_free);

    ctrlid = genl_ctrl_resolve(sock, "nlctrl");

    genlmsg_put(msg.get(), 0, 0, ctrlid, 0, 0, CTRL_CMD_GETFAMILY, 0);

    nla_put_string(msg.get(), CTRL_ATTR_FAMILY_NAME, family);

    err = nlSendMsg(sock, msg.get(), nlFamilyHandle, &grp);
    if (err)
        return err;

    err = grp.id;
    LOG(INFO) << group << " multicast_id: " << grp.id;

    return err;
}

static bool socketAddMembership(struct nl_sock *sock, const char *group) {
    int mcid = nlGetMulticastId(sock, THERMAL_GENL_FAMILY_NAME, group);
    if (mcid < 0) {
        LOG(ERROR) << "Failed to get multicast id: " << group;
        return false;
    }

    if (nl_socket_add_membership(sock, mcid)) {
        LOG(ERROR) << "Failed to add netlink socket membership: " << group;
        return false;
    }

    LOG(INFO) << "Added netlink socket membership: " << group;
    return true;
}

static int handleEvent(struct nl_msg *n, void *arg) {
    struct nlmsghdr *nlh = nlmsg_hdr(n);
    struct genlmsghdr *glh = genlmsg_hdr(nlh);
    struct nlattr *attrs[THERMAL_GENL_ATTR_MAX + 1];
    int *tz_id = reinterpret_cast<int *>(arg);

    genlmsg_parse(nlh, 0, attrs, THERMAL_GENL_ATTR_MAX, NULL);

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_TRIP_UP) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_TRIP_UP";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID])
            LOG(INFO) << "Thermal zone trip id: "
                      << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_TRIP_DOWN) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_TRIP_DOWN";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID])
            LOG(INFO) << "Thermal zone trip id: "
                      << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_GOV_CHANGE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_GOV_CHANGE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_GOV_NAME])
            LOG(INFO) << "Governor name: " << nla_get_string(attrs[THERMAL_GENL_ATTR_GOV_NAME]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_CREATE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_CREATE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_NAME])
            LOG(INFO) << "Thermal zone name: " << nla_get_string(attrs[THERMAL_GENL_ATTR_TZ_NAME]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_DELETE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_DELETE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_DISABLE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_DISABLE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_ENABLE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_ENABLE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_TRIP_CHANGE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_TRIP_CHANGE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID])
            LOG(INFO) << "Trip id:: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_TYPE])
            LOG(INFO) << "Trip type: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_TYPE]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_TEMP])
            LOG(INFO) << "Trip temp: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_TEMP]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_HYST])
            LOG(INFO) << "Trip hyst: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_HYST]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_TRIP_ADD) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_TRIP_ADD";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID])
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID])
            LOG(INFO) << "Trip id:: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_TYPE])
            LOG(INFO) << "Trip type: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_TYPE]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_TEMP])
            LOG(INFO) << "Trip temp: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_TEMP]);
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_HYST])
            LOG(INFO) << "Trip hyst: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_HYST]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_TZ_TRIP_DELETE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_TZ_TRIP_DELETE";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID])
            LOG(INFO) << "Trip id:: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TRIP_ID]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_CDEV_STATE_UPDATE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_CDEV_STATE_UPDATE";
        if (attrs[THERMAL_GENL_ATTR_CDEV_ID])
            LOG(INFO) << "Cooling device id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_ID]);
        if (attrs[THERMAL_GENL_ATTR_CDEV_CUR_STATE])
            LOG(INFO) << "Cooling device current state: "
                      << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_CUR_STATE]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_CDEV_ADD) {
        LOG(INFO) << "THERMAL_GENL_EVENT_CDEV_ADD";
        if (attrs[THERMAL_GENL_ATTR_CDEV_NAME])
            LOG(INFO) << "Cooling device name: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_NAME]);
        if (attrs[THERMAL_GENL_ATTR_CDEV_ID])
            LOG(INFO) << "Cooling device id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_ID]);
        if (attrs[THERMAL_GENL_ATTR_CDEV_MAX_STATE])
            LOG(INFO) << "Cooling device max state: "
                      << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_MAX_STATE]);
    }

    if (glh->cmd == THERMAL_GENL_EVENT_CDEV_DELETE) {
        LOG(INFO) << "THERMAL_GENL_EVENT_CDEV_DELETE";
        if (attrs[THERMAL_GENL_ATTR_CDEV_ID])
            LOG(INFO) << "Cooling device id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_CDEV_ID]);
    }

    if (glh->cmd == THERMAL_GENL_SAMPLING_TEMP) {
        LOG(INFO) << "THERMAL_GENL_SAMPLING_TEMP";
        if (attrs[THERMAL_GENL_ATTR_TZ_ID]) {
            LOG(INFO) << "Thermal zone id: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
            *tz_id = nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_ID]);
        }
        if (attrs[THERMAL_GENL_ATTR_TZ_TEMP])
            LOG(INFO) << "Thermal zone temp: " << nla_get_u32(attrs[THERMAL_GENL_ATTR_TZ_TEMP]);
    }

    return 0;
}

}  // namespace

void ThermalWatcher::registerFilesToWatch(const std::set<std::string> &sensors_to_watch) {
    LOG(INFO) << "Uevent register file to watch...";
    monitored_sensors_.insert(sensors_to_watch.begin(), sensors_to_watch.end());

    uevent_fd_.reset((TEMP_FAILURE_RETRY(uevent_open_socket(64 * 1024, true))));
    if (uevent_fd_.get() < 0) {
        LOG(ERROR) << "failed to open uevent socket";
        return;
    }

    fcntl(uevent_fd_, F_SETFL, O_NONBLOCK);

    looper_->addFd(uevent_fd_.get(), 0, ::android::Looper::EVENT_INPUT, nullptr, nullptr);
    sleep_ms_ = std::chrono::milliseconds(0);
    last_update_time_ = boot_clock::now();
}

void ThermalWatcher::registerFilesToWatchNl(const std::set<std::string> &sensors_to_watch) {
    LOG(INFO) << "Thermal genl register file to watch...";
    monitored_sensors_.insert(sensors_to_watch.begin(), sensors_to_watch.end());

    sk_thermal = nl_socket_alloc();
    if (!sk_thermal) {
        LOG(ERROR) << "nl_socket_alloc failed";
        return;
    }

    if (genl_connect(sk_thermal)) {
        LOG(ERROR) << "genl_connect failed: sk_thermal";
        return;
    }

    thermal_genl_fd_.reset(nl_socket_get_fd(sk_thermal));
    if (thermal_genl_fd_.get() < 0) {
        LOG(ERROR) << "Failed to create thermal netlink socket";
        return;
    }

    if (!socketAddMembership(sk_thermal, THERMAL_GENL_EVENT_GROUP_NAME)) {
        return;
    }

    /*
     * Currently, only the update_temperature() will send thermal genl samlping events
     * from kernel. To avoid thermal-hal busy because samlping events are sent
     * too frequently, ignore thermal genl samlping events until we figure out how to use it.
     *
    if (!socketAddMembership(sk_thermal, THERMAL_GENL_SAMPLING_GROUP_NAME)) {
        return;
    }
    */

    fcntl(thermal_genl_fd_, F_SETFL, O_NONBLOCK);
    looper_->addFd(thermal_genl_fd_.get(), 0, ::android::Looper::EVENT_INPUT, nullptr, nullptr);
    sleep_ms_ = std::chrono::milliseconds(0);
    last_update_time_ = boot_clock::now();
}

bool ThermalWatcher::startWatchingDeviceFiles() {
    if (cb_) {
        auto ret = this->run("FileWatcherThread", ::android::PRIORITY_HIGHEST);
        if (ret != ::android::NO_ERROR) {
            LOG(ERROR) << "ThermalWatcherThread start fail";
            return false;
        } else {
            LOG(INFO) << "ThermalWatcherThread started";
            return true;
        }
    }
    return false;
}
void ThermalWatcher::parseUevent(std::set<std::string> *sensors_set) {
    bool thermal_event = false;
    constexpr int kUeventMsgLen = 2048;
    char msg[kUeventMsgLen + 2];
    char *cp;

    while (true) {
        int n = uevent_kernel_multicast_recv(uevent_fd_.get(), msg, kUeventMsgLen);
        if (n <= 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG(ERROR) << "Error reading from Uevent Fd";
            }
            break;
        }

        if (n >= kUeventMsgLen) {
            LOG(ERROR) << "Uevent overflowed buffer, discarding";
            continue;
        }

        msg[n] = '\0';
        msg[n + 1] = '\0';

        cp = msg;
        while (*cp) {
            std::string uevent = cp;
            auto findSubSystemThermal = uevent.find("SUBSYSTEM=thermal");
            if (!thermal_event) {
                if (::android::base::StartsWith(uevent, "SUBSYSTEM=")) {
                    if (findSubSystemThermal != std::string::npos) {
                        thermal_event = true;
                    } else {
                        break;
                    }
                }
            } else {
                auto start_pos = uevent.find("NAME=");
                if (start_pos != std::string::npos) {
                    start_pos += 5;
                    std::string name = uevent.substr(start_pos);
                    if (monitored_sensors_.find(name) != monitored_sensors_.end()) {
                        sensors_set->insert(name);
                    }
                    break;
                }
            }
            while (*cp++) {
            }
        }
    }
}

// TODO(b/175367921): Consider for potentially adding more type of event in the function
// instead of just add the sensors to the list.
void ThermalWatcher::parseGenlink(std::set<std::string> *sensors_set) {
    int err = 0, done = 0, tz_id = -1;

    std::unique_ptr<nl_cb, decltype(&nl_cb_put)> cb(nl_cb_alloc(NL_CB_DEFAULT), nl_cb_put);

    nl_cb_err(cb.get(), NL_CB_CUSTOM, nlErrorHandle, &err);
    nl_cb_set(cb.get(), NL_CB_FINISH, NL_CB_CUSTOM, nlFinishHandle, &done);
    nl_cb_set(cb.get(), NL_CB_ACK, NL_CB_CUSTOM, nlAckHandle, &done);
    nl_cb_set(cb.get(), NL_CB_SEQ_CHECK, NL_CB_CUSTOM, nlSeqCheckHandle, &done);
    nl_cb_set(cb.get(), NL_CB_VALID, NL_CB_CUSTOM, handleEvent, &tz_id);

    while (!done && !err) {
        nl_recvmsgs(sk_thermal, cb.get());

        if (tz_id < 0) {
            break;
        }

        std::string name;
        if (getThermalZoneTypeById(tz_id, &name) &&
            monitored_sensors_.find(name) != monitored_sensors_.end()) {
            sensors_set->insert(name);
        }
    }
}

void ThermalWatcher::wake() {
    looper_->wake();
}

bool ThermalWatcher::threadLoop() {
    LOG(VERBOSE) << "ThermalWatcher polling...";

    int fd;
    std::set<std::string> sensors;

    auto time_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(boot_clock::now() -
                                                                                 last_update_time_);

    if (time_elapsed_ms < sleep_ms_ &&
        looper_->pollOnce(sleep_ms_.count(), &fd, nullptr, nullptr) >= 0) {
        ATRACE_NAME("ThermalWatcher::threadLoop - receive event");
        if (fd != uevent_fd_.get() && fd != thermal_genl_fd_.get()) {
            return true;
        } else if (fd == thermal_genl_fd_.get()) {
            parseGenlink(&sensors);
        } else if (fd == uevent_fd_.get()) {
            parseUevent(&sensors);
        }
        // Ignore cb_ if uevent is not from monitored sensors
        if (sensors.size() == 0) {
            return true;
        }
    }

    sleep_ms_ = cb_(sensors);
    last_update_time_ = boot_clock::now();
    return true;
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
