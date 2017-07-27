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

#ifndef RIL_SOCKET_H_INCLUDED
#define RIL_SOCKET_H_INCLUDED
#include <libril/ril_ex.h>
#include "rilSocketQueue.h"
#include <ril_event.h>

using namespace std;

extern "C" void *ril_socket_process_requests_loop(void *arg);

/**
 * Abstract socket class representing sockets in rild.
 * <p>
 * This class performs the following functions :
 * <ul>
 *     <li> Start socket listen.
 *     <li> Handle socket listen and command callbacks.
 * </ul>
 */
class RilSocket {
    protected:

        /**
         * Socket name.
         */
        const char* name;

        /**
         * Socket id.
         */
        RIL_SOCKET_ID id;

       /**
        * Listen socket file descriptor.
        */
        int listenFd = -1;

       /**
        * Commands socket file descriptor.
        */
        int commandFd = -1;

       /**
        * Socket request loop thread id.
        */
        pthread_t socketThreadId;

       /**
        * Listen event callack. Callback called when the other ends does accept.
        */
        ril_event_cb listenCb;

       /**
        * Commands event callack.Callback called when there are requests from the other side.
        */
        ril_event_cb commandCb;

        /**
         * Listen event to be added to eventloop after socket listen.
         */
        struct ril_event listenEvent;

        /**
         * Commands event to be added to eventloop after accept.
         */
        struct ril_event callbackEvent;

        /**
         * Static socket listen handler. Chooses the socket to call the listen callback
         * from ril.cpp.
         *
         * @param Listen fd.
         * @param flags.
         * @param Parameter for the listen handler.
         */
        static void sSocketListener(int fd, short flags, void *param);

        /**
         * Static socket request handler. Chooses the socket to call the request handler on.
         *
         * @param Commands fd.
         * @param flags.
         * @param Parameter for the request handler.
         */
        static void sSocketRequestsHandler(int fd, short flags, void *param);

        /**
         * Process record from the record stream and push the requests onto the queue.
         *
         * @param record data.
         * @param record length.
         */
        virtual void pushRecord(void *record, size_t recordlen) = 0;

        /**
         * Socket lock for writing data on the socket.
         */
        pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;

        /**
         * The loop to process the incoming requests.
         */
        virtual void *processRequestsLoop(void) = 0;

    private:
        friend void *::ril_socket_process_requests_loop(void *arg);

    public:

        /**
         * Constructor.
         *
         * @param Socket name.
         * @param Socket id.
         */
        RilSocket(const char* socketName, RIL_SOCKET_ID socketId) {
            name = socketName;
            id = socketId;
        }

        /**
         * Clean up function on commands socket close.
         */
        virtual void onCommandsSocketClosed(void) = 0;

        /**
         * Function called on new commands socket connect. Request loop thread is started here.
         */
        void onNewCommandConnect(void);

        /**
         * Set listen socket fd.
         *
         * @param Input fd.
         */
        void setListenFd(int listenFd);

        /**
         * Set commands socket fd.
         *
         * @param Input fd.
         */
        void setCommandFd(int commandFd);

        /**
         * Get listen socket fd.
         *
         * @return Listen fd.
         */
        int getListenFd(void);

        /**
         * Get commands socket fd.
         *
         * @return Commands fd.
         */
        int getCommandFd(void);

        /**
         * Set listen event callback.
         *
         * @param Input event callback.
         */
        void setListenCb(ril_event_cb listenCb);

        /**
         * Set command event callback.
         *
         * @param Input event callback.
         */
        void setCommandCb(ril_event_cb commandCb);

        /**
         * Get listen event callback.
         *
         * @return Listen event callback.
         */
        ril_event_cb getListenCb(void);

        /**
         * Gey command event callback.
         *
         * @return Command event callback.
         */
        ril_event_cb getCommandCb(void);

        /**
         * Set listen event.
         *
         * @param Input event.
         */
        void setListenEvent(ril_event listenEvent);

        /**
         * Set command callback event.
         *
         * @param Input event.
         */
        void setCallbackEvent(ril_event commandEvent);

        /**
         * Get listen event.
         *
         * @return Listen event.
         */
        ril_event* getListenEvent(void);

        /**
         * Get commands callback event.
         *
         * @return Commands callback event.
         */
        ril_event* getCallbackEvent(void);

        virtual ~RilSocket(){}

    protected:

        /**
         * Start listening on the socket and add the socket listen callback event.
         *
         * @return Result of the socket listen.
         */
        int socketInit(void);

        /**
         * Socket request handler
         *
         * @param Commands fd.
         * @param flags.
         * @param Record stream.
         */
        void socketRequestsHandler(int fd, short flags, RecordStream *rs);
};

class socketClient {
    public:
        RilSocket *socketPtr;
        RecordStream *rs;

        socketClient(RilSocket *socketPtr, RecordStream *rs) {
            this->socketPtr = socketPtr;
            this->rs = rs;
        }
};

typedef struct MySocketListenParam {
    SocketListenParam sListenParam;
    RilSocket *socket;
} MySocketListenParam;

typedef void* (RilSocket::*RilSocketFuncPtr)(void);
typedef void (RilSocket::*RilSocketEventPtr)(int fd,short flags, void *param);
typedef void* (*PthreadPtr)(void*);

#endif
