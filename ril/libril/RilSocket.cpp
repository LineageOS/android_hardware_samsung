/*
* Copyright (C) 2014 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

extern "C"
void *ril_socket_process_requests_loop(void *arg);

#include "RilSocket.h"
#include <cutils/sockets.h>
#include <utils/Log.h>
#include <assert.h>
#define SOCKET_LISTEN_BACKLOG 0

int RilSocket::socketInit(void) {
    int ret;

    listenCb = &RilSocket::sSocketListener;
    commandCb = &RilSocket::sSocketRequestsHandler;
    listenFd = android_get_control_socket(name);

    //Start listening
    ret = listen(listenFd, SOCKET_LISTEN_BACKLOG);

    if (ret < 0) {
        RLOGE("Failed to listen on %s socket '%d': %s",
        name, listenFd, strerror(errno));
        return ret;
    }
    //Add listen event to the event loop
    ril_event_set(&listenEvent, listenFd, false, listenCb, this);
    rilEventAddWakeup_helper(&listenEvent);
    return ret;
}

void RilSocket::sSocketListener(int fd, short flags, void *param) {
    RilSocket *theSocket = (RilSocket *) param;
    MySocketListenParam listenParam;
    listenParam.socket = theSocket;
    listenParam.sListenParam.type = RIL_SAP_SOCKET;

    listenCallback_helper(fd, flags, (void*)&listenParam);
}

void RilSocket::onNewCommandConnect() {
    pthread_attr_t attr;
    PthreadPtr pptr = ril_socket_process_requests_loop;
    int result;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    //Start socket request processing loop thread
    result = pthread_create(&socketThreadId, &attr, pptr, this);
    if(result < 0) {
        RLOGE("pthread_create failed with result:%d",result);
    }

    RLOGE("New socket command connected and socket request thread started");
}

void RilSocket::sSocketRequestsHandler(int fd, short flags, void *param) {
    socketClient *sc = (socketClient *) param;
    RilSocket *theSocket = sc->socketPtr;
    RecordStream *rs = sc->rs;

    theSocket->socketRequestsHandler(fd, flags, rs);
}

void RilSocket::socketRequestsHandler(int fd, short flags, RecordStream *p_rs) {
    int ret;
    assert(fd == commandFd);
    void *p_record;
    size_t recordlen;

    for (;;) {
        /* loop until EAGAIN/EINTR, end of stream, or other error */
        ret = record_stream_get_next(p_rs, &p_record, &recordlen);

        if (ret == 0 && p_record == NULL) {
            /* end-of-stream */
            break;
        } else if (ret < 0) {
            break;
        } else if (ret == 0) {
            pushRecord(p_record, recordlen);
        }
    }

    if (ret == 0 || !(errno == EAGAIN || errno == EINTR)) {
        /* fatal error or end-of-stream */
        if (ret != 0) {
            RLOGE("error on reading command socket errno:%d\n", errno);
        } else {
            RLOGW("EOS.  Closing command socket.");
        }

        close(commandFd);
        commandFd = -1;

        ril_event_del(&callbackEvent);

        record_stream_free(p_rs);

        /* start listening for new connections again */

        rilEventAddWakeup_helper(&listenEvent);

        onCommandsSocketClosed();
    }
}

void RilSocket::setListenFd(int fd) {
    listenFd = fd;
}

void RilSocket::setCommandFd(int fd) {
    commandFd = fd;
}

int RilSocket::getListenFd(void) {
    return listenFd;
}

int RilSocket::getCommandFd(void) {
    return commandFd;
}

void RilSocket::setListenCb(ril_event_cb cb) {
    listenCb = cb;
}

void RilSocket::setCommandCb(ril_event_cb cb) {
    commandCb = cb;
}

ril_event_cb RilSocket::getListenCb(void) {
    return listenCb;
}

ril_event_cb RilSocket::getCommandCb(void) {
    return commandCb;
}

void RilSocket::setListenEvent(ril_event event) {
    listenEvent = event;
}

void RilSocket::setCallbackEvent(ril_event event) {
    callbackEvent = event;
}

ril_event* RilSocket::getListenEvent(void)  {
    return &listenEvent;
}

ril_event* RilSocket::getCallbackEvent(void) {
    return &callbackEvent;
}

extern "C"
void *ril_socket_process_requests_loop(void *arg) {
    RilSocket *socket = (RilSocket *)arg;
    socket->processRequestsLoop();
    return NULL;
}
