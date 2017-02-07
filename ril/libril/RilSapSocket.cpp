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

#define __STDC_LIMIT_MACROS
#include <stdint.h>
#define RIL_SHLIB
#include "telephony/ril.h"
#include "RilSapSocket.h"
#include "pb_decode.h"
#include "pb_encode.h"
#undef LOG_TAG
#define LOG_TAG "RIL_UIM_SOCKET"
#include <utils/Log.h>
#include <arpa/inet.h>
#include <errno.h>

static RilSapSocket::RilSapSocketList *head = NULL;

void ril_sap_on_request_complete (
        RIL_Token t, RIL_Errno e,
        void *response, size_t responselen
);

void ril_sap_on_unsolicited_response (
        int unsolResponse, const void *data,
        size_t datalen
);
extern "C" void
RIL_requestTimedCallback (RIL_TimedCallback callback, void *param,
        const struct timeval *relativeTime);

struct RIL_Env RilSapSocket::uimRilEnv = {
        .OnRequestComplete = RilSapSocket::sOnRequestComplete,
        .OnUnsolicitedResponse = RilSapSocket::sOnUnsolicitedResponse,
        .RequestTimedCallback = RIL_requestTimedCallback
};

void RilSapSocket::sOnRequestComplete (RIL_Token t,
        RIL_Errno e,
        void *response,
        size_t responselen) {
    RilSapSocket *sap_socket;
    SapSocketRequest *request = (SapSocketRequest*) t;

    RLOGD("Socket id:%d", request->socketId);

    sap_socket = getSocketById(request->socketId);

    if (sap_socket) {
        sap_socket->onRequestComplete(t,e,response,responselen);
    } else {
        RLOGE("Invalid socket id");
        if (request->curr->payload) {
            free(request->curr->payload);
        }
        free(request->curr);
        free(request);
    }
}

#if defined(ANDROID_MULTI_SIM)
void RilSapSocket::sOnUnsolicitedResponse(int unsolResponse,
        const void *data,
        size_t datalen,
        RIL_SOCKET_ID socketId) {
    RilSapSocket *sap_socket = getSocketById(socketId);
    if (sap_socket) {
        sap_socket->onUnsolicitedResponse(unsolResponse, (void *)data, datalen);
    }
}
#else
void RilSapSocket::sOnUnsolicitedResponse(int unsolResponse,
       const void *data,
       size_t datalen) {
    RilSapSocket *sap_socket = getSocketById(RIL_SOCKET_1);
    sap_socket->onUnsolicitedResponse(unsolResponse, (void *)data, datalen);
}
#endif

void RilSapSocket::printList() {
    RilSapSocketList *current = head;
    RLOGD("Printing socket list");
    while(NULL != current) {
        RLOGD("SocketName:%s",current->socket->name);
        RLOGD("Socket id:%d",current->socket->id);
        current = current->next;
    }
}

RilSapSocket *RilSapSocket::getSocketById(RIL_SOCKET_ID socketId) {
    RilSapSocket *sap_socket;
    RilSapSocketList *current = head;

    RLOGD("Entered getSocketById");
    printList();

    while(NULL != current) {
        if(socketId == current->socket->id) {
            sap_socket = current->socket;
            return sap_socket;
        }
        current = current->next;
    }
    return NULL;
}

void RilSapSocket::initSapSocket(const char *socketName,
        RIL_RadioFunctions *uimFuncs) {

    if (strcmp(socketName, "sap_uim_socket1") == 0) {
        if(!SocketExists(socketName)) {
            addSocketToList(socketName, RIL_SOCKET_1, uimFuncs);
        }
    }

#if (SIM_COUNT >= 2)
    if (strcmp(socketName, "sap_uim_socket2") == 0) {
        if(!SocketExists(socketName)) {
            addSocketToList(socketName, RIL_SOCKET_2, uimFuncs);
        }
    }
#endif

#if (SIM_COUNT >= 3)
    if (strcmp(socketName, "sap_uim_socket3") == 0) {
        if(!SocketExists(socketName)) {
            addSocketToList(socketName, RIL_SOCKET_3, uimFuncs);
        }
    }
#endif

#if (SIM_COUNT >= 4)
    if (strcmp(socketName, "sap_uim_socket4") == 0) {
        if(!SocketExists(socketName)) {
            addSocketToList(socketName, RIL_SOCKET_4, uimFuncs);
        }
    }
#endif
}

void RilSapSocket::addSocketToList(const char *socketName, RIL_SOCKET_ID socketid,
        RIL_RadioFunctions *uimFuncs) {
    RilSapSocket* socket = NULL;
    RilSapSocketList *current;

    if(!SocketExists(socketName)) {
        socket = new RilSapSocket(socketName, socketid, uimFuncs);
        RilSapSocketList* listItem = (RilSapSocketList*)malloc(sizeof(RilSapSocketList));
        if (!listItem) {
            RLOGE("addSocketToList: OOM");
            return;
        }
        listItem->socket = socket;
        listItem->next = NULL;

        RLOGD("Adding socket with id: %d", socket->id);

        if(NULL == head) {
            head = listItem;
            head->next = NULL;
        }
        else {
            current = head;
            while(NULL != current->next) {
                current = current->next;
            }
            current->next = listItem;
        }
        socket->socketInit();
    }
}

bool RilSapSocket::SocketExists(const char *socketName) {
    RilSapSocketList* current = head;

    while(NULL != current) {
        if(strcmp(current->socket->name, socketName) == 0) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void* RilSapSocket::processRequestsLoop(void) {
    RLOGI("UIM_SOCKET:Request loop started");

    while(true) {
        SapSocketRequest *req = dispatchQueue.dequeue();

        RLOGI("New request from the dispatch Queue");

        if (req != NULL) {
            dispatchRequest(req->curr);
            free(req);
        } else {
            RLOGE("Fetched null buffer from queue!");
        }
    }
    return NULL;
}

RilSapSocket::RilSapSocket(const char *socketName,
        RIL_SOCKET_ID socketId,
        RIL_RadioFunctions *inputUimFuncs):
        RilSocket(socketName, socketId) {
    if (inputUimFuncs) {
        uimFuncs = inputUimFuncs;
    }
}

#define BYTES_PER_LINE 16

#define NIBBLE_TO_HEX(n) ({ \
  uint8_t __n = (uint8_t) n & 0x0f; \
  __nibble >= 10 ? 'A' + __n - 10: '0' + __n; \
})

#define HEX_HIGH(b) ({ \
  uint8_t __b = (uint8_t) b; \
  uint8_t __nibble = (__b >> 4) & 0x0f; \
  NIBBLE_TO_HEX(__nibble); \
})

#define HEX_LOW(b) ({ \
  uint8_t __b = (uint8_t) b; \
  uint8_t __nibble = __b & 0x0f; \
  NIBBLE_TO_HEX(__nibble); \
})

void log_hex(const char *who, const uint8_t *buffer, int length) {
    char out[80];
    int source = 0;
    int dest = 0;
    int dest_len = sizeof(out);
    int per_line = 0;

    do {
        dest += sprintf(out, "%8.8s [%8.8x] ", who, source);
        for(; source < length && dest_len - dest > 3 && per_line < BYTES_PER_LINE; source++,
        per_line ++) {
            out[dest++] = HEX_HIGH(buffer[source]);
            out[dest++] = HEX_LOW(buffer[source]);
            out[dest++] = ' ';
        }
        if (dest < dest_len && (per_line == BYTES_PER_LINE || source >= length)) {
            out[dest++] = 0;
            per_line = 0;
            dest = 0;
            RLOGD("%s\n", out);
        }
    } while(source < length && dest < dest_len);
}

void RilSapSocket::dispatchRequest(MsgHeader *req) {
    // SapSocketRequest will be deallocated in onRequestComplete()
    SapSocketRequest* currRequest=(SapSocketRequest*)malloc(sizeof(SapSocketRequest));
    if (!currRequest) {
        RLOGE("dispatchRequest: OOM");
        // Free MsgHeader allocated in pushRecord()
        free(req);
        return;
    }
    currRequest->token = req->token;
    currRequest->curr = req;
    currRequest->p_next = NULL;
    currRequest->socketId = id;

    pendingResponseQueue.enqueue(currRequest);

    if (uimFuncs) {
        RLOGI("[%d] > SAP REQUEST type: %d. id: %d. error: %d",
        req->token,
        req->type,
        req->id,
        req->error );

#if defined(ANDROID_MULTI_SIM)
        uimFuncs->onRequest(req->id, req->payload->bytes, req->payload->size, currRequest, id);
#else
        uimFuncs->onRequest(req->id, req->payload->bytes, req->payload->size, currRequest);
#endif
    }
}

void RilSapSocket::onRequestComplete(RIL_Token t, RIL_Errno e, void *response,
        size_t response_len) {
    SapSocketRequest* request= (SapSocketRequest*)t;
    MsgHeader *hdr = request->curr;

    MsgHeader rsp;
    rsp.token = request->curr->token;
    rsp.type = MsgType_RESPONSE;
    rsp.id = request->curr->id;
    rsp.error = (Error)e;
    rsp.payload = (pb_bytes_array_t *)calloc(1, sizeof(pb_bytes_array_t) + response_len);
    if (!rsp.payload) {
        RLOGE("onRequestComplete: OOM");
    } else {
        if (response && response_len > 0) {
            memcpy(rsp.payload->bytes, response, response_len);
            rsp.payload->size = response_len;
        } else {
            rsp.payload->size = 0;
        }

        RLOGE("Token:%d, MessageId:%d", hdr->token, hdr->id);

        sendResponse(&rsp);
        free(rsp.payload);
    }

    // Deallocate SapSocketRequest
    if(!pendingResponseQueue.checkAndDequeue(hdr->id, hdr->token)) {
        RLOGE("Token:%d, MessageId:%d", hdr->token, hdr->id);
        RLOGE ("RilSapSocket::onRequestComplete: invalid Token or Message Id");
    }

    // Deallocate MsgHeader
    free(hdr);
}

void RilSapSocket::sendResponse(MsgHeader* hdr) {
    size_t encoded_size = 0;
    uint32_t written_size;
    size_t buffer_size = 0;
    pb_ostream_t ostream;
    bool success = false;

    pthread_mutex_lock(&write_lock);

    if ((success = pb_get_encoded_size(&encoded_size, MsgHeader_fields,
        hdr)) && encoded_size <= INT32_MAX && commandFd != -1) {
        buffer_size = encoded_size + sizeof(uint32_t);
        uint8_t* buffer = (uint8_t*)malloc(buffer_size);
        if (!buffer) {
            RLOGE("sendResponse: OOM");
            pthread_mutex_unlock(&write_lock);
            return;
        }
        written_size = htonl((uint32_t) encoded_size);
        ostream = pb_ostream_from_buffer(buffer, buffer_size);
        pb_write(&ostream, (uint8_t *)&written_size, sizeof(written_size));
        success = pb_encode(&ostream, MsgHeader_fields, hdr);

        if (success) {
            RLOGD("Size: %zu (0x%zx) Size as written: 0x%x", encoded_size,
                    encoded_size, written_size);
            log_hex("onRequestComplete", &buffer[sizeof(written_size)], encoded_size);
            RLOGI("[%d] < SAP RESPONSE type: %d. id: %d. error: %d",
        hdr->token, hdr->type, hdr->id,hdr->error );

            if ( 0 != blockingWrite_helper(commandFd, buffer, buffer_size)) {
                RLOGE("Error %d while writing to fd", errno);
            } else {
                RLOGD("Write successful");
            }
        } else {
            RLOGE("Error while encoding response of type %d id %d buffer_size: %zu: %s.",
                    hdr->type, hdr->id, buffer_size, PB_GET_ERROR(&ostream));
        }
        free(buffer);
    } else {
        RLOGE("Not sending response type %d: encoded_size: %zu. commandFd: %d. encoded size result:\
                %d", hdr->type, encoded_size, commandFd, success);
    }

    pthread_mutex_unlock(&write_lock);
}

void RilSapSocket::onUnsolicitedResponse(int unsolResponse, void *data, size_t datalen) {
    if (data && datalen > 0) {
        pb_bytes_array_t *payload = (pb_bytes_array_t *)calloc(1,
                sizeof(pb_bytes_array_t) + datalen);
        if (!payload) {
            RLOGE("onUnsolicitedResponse: OOM");
            return;
        }
        memcpy(payload->bytes, data, datalen);
        payload->size = datalen;
        MsgHeader rsp;
        rsp.payload = payload;
        rsp.type = MsgType_UNSOL_RESPONSE;
        rsp.id = (MsgId)unsolResponse;
        rsp.error = Error_RIL_E_SUCCESS;
        sendResponse(&rsp);
        free(payload);
    }
}

void RilSapSocket::pushRecord(void *p_record, size_t recordlen) {
    pb_istream_t stream = pb_istream_from_buffer((uint8_t *)p_record, recordlen);
    // MsgHeader will be deallocated in onRequestComplete()
    MsgHeader *reqHeader = (MsgHeader *)malloc(sizeof (MsgHeader));
    if (!reqHeader) {
        RLOGE("pushRecord: OOM");
        return;
    }
    memset(reqHeader, 0, sizeof(MsgHeader));

    log_hex("BtSapTest-Payload", (const uint8_t*)p_record, recordlen);

    if (!pb_decode(&stream, MsgHeader_fields, reqHeader) ) {
        RLOGE("Error decoding protobuf buffer : %s", PB_GET_ERROR(&stream));
        free(reqHeader);
    } else {
        // SapSocketRequest will be deallocated in processRequestsLoop()
        SapSocketRequest *recv = (SapSocketRequest*)malloc(sizeof(SapSocketRequest));
        if (!recv) {
            RLOGE("pushRecord: OOM");
            free(reqHeader);
            return;
        }
        recv->token = reqHeader->token;
        recv->curr = reqHeader;
        recv->socketId = id;

        dispatchQueue.enqueue(recv);
    }
}

void RilSapSocket::sendDisconnect() {
    size_t encoded_size = 0;
    uint32_t written_size;
    size_t buffer_size = 0;
    pb_ostream_t ostream;
    bool success = false;

    RIL_SIM_SAP_DISCONNECT_REQ disconnectReq;

   if ((success = pb_get_encoded_size(&encoded_size, RIL_SIM_SAP_DISCONNECT_REQ_fields,
        &disconnectReq)) && encoded_size <= INT32_MAX) {
        buffer_size = encoded_size + sizeof(uint32_t);
        uint8_t* buffer = (uint8_t*)malloc(buffer_size);
        if (!buffer) {
            RLOGE("sendDisconnect: OOM");
            return;
        }
        written_size = htonl((uint32_t) encoded_size);
        ostream = pb_ostream_from_buffer(buffer, buffer_size);
        pb_write(&ostream, (uint8_t *)&written_size, sizeof(written_size));
        success = pb_encode(&ostream, RIL_SIM_SAP_DISCONNECT_REQ_fields, buffer);

        if(success) {
            // Buffer will be deallocated in sOnRequestComplete()
            pb_bytes_array_t *payload = (pb_bytes_array_t *)calloc(1,
                    sizeof(pb_bytes_array_t) + written_size);
            if (!payload) {
                RLOGE("sendDisconnect: OOM");
                return;
            }
            memcpy(payload->bytes, buffer, written_size);
            payload->size = written_size;
            // MsgHeader will be deallocated in sOnRequestComplete()
            MsgHeader *hdr = (MsgHeader *)malloc(sizeof(MsgHeader));
            if (!hdr) {
                RLOGE("sendDisconnect: OOM");
                free(payload);
                return;
            }
            hdr->payload = payload;
            hdr->type = MsgType_REQUEST;
            hdr->id = MsgId_RIL_SIM_SAP_DISCONNECT;
            hdr->error = Error_RIL_E_SUCCESS;
            dispatchDisconnect(hdr);
        }
        else {
            RLOGE("Encode failed in send disconnect!");
        }
        free(buffer);
    }
}

void RilSapSocket::dispatchDisconnect(MsgHeader *req) {
    // SapSocketRequest will be deallocated in sOnRequestComplete()
    SapSocketRequest* currRequest=(SapSocketRequest*)malloc(sizeof(SapSocketRequest));
    if (!currRequest) {
        RLOGE("dispatchDisconnect: OOM");
        // Free memory allocated in sendDisconnect
        free(req->payload);
        free(req);
        return;
    }
    currRequest->token = -1;
    currRequest->curr = req;
    currRequest->p_next = NULL;
    currRequest->socketId = (RIL_SOCKET_ID)99;

    RLOGD("Sending disconnect on command close!");

#if defined(ANDROID_MULTI_SIM)
    uimFuncs->onRequest(req->id, req->payload->bytes, req->payload->size, currRequest, id);
#else
    uimFuncs->onRequest(req->id, req->payload->bytes, req->payload->size, currRequest);
#endif
}

void RilSapSocket::onCommandsSocketClosed() {
    sendDisconnect();
    RLOGE("Socket command closed");
}
