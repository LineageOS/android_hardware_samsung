/*
 * Copyright (C) 2010 The Android Open Source Project
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

#define LOG_TAG "SapClient"

#include <binder/Parcel.h>
#include <telephony/ril.h>
#include <cutils/record_stream.h>

#include <unistd.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <android/log.h>
#include <pthread.h>
#include "secril-client-sap.h"
#include <hardware_legacy/power.h> // For wakelock

#define RIL_CLIENT_WAKE_LOCK "client-sap-interface"

namespace android {

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
#define DBG 0

#define MULTI_CLIENT_SOCKET_NAME "Multiclient"

#define MAX_COMMAND_BYTES       (8 * 1024)
#define REQ_POOL_SIZE           32
#define TOKEN_POOL_SIZE         32

// Constants for response types
#define RESPONSE_SOLICITED      0
#define RESPONSE_UNSOLICITED    1

#define max(a, b)   ((a) > (b) ? (a) : (b))

#define REQ_OEM_HOOK_RAW        RIL_REQUEST_OEM_HOOK_RAW

//---------------------------------------------------------------------------
// Type definitions
//---------------------------------------------------------------------------
typedef struct _ReqHistory {
    int         token;  // token used for request
    uint32_t    id;     // request ID
} ReqHistory;

typedef struct _ReqRespHandler {
    uint32_t        id;         // request ID
    RilOnComplete   handler;    // handler function
} ReqRespHandler;

typedef struct _UnsolHandler {
    uint32_t            id;         // unsolicited response ID
    RilOnUnsolicited    handler;    // handler function
} UnsolHandler;

typedef struct _RilClientPrv {
    HRilClient      parent;
    uint8_t         b_connect;  // connected to server?
    int             sock;       // socket
    int             pipefd[2];
    fd_set          sock_rfds;  // for read with select()
    RecordStream    *p_rs;
    uint32_t        token_pool; // each bit in token_pool used for token.
                                // so, pool size is 32.
    pthread_t       tid_reader; // socket reader thread id
    ReqHistory      history[TOKEN_POOL_SIZE];       // request history
    ReqRespHandler  req_handlers[REQ_POOL_SIZE];    // request response handler list
    UnsolHandler    unsol_handlers[REQ_POOL_SIZE];  // unsolicited response handler list
    RilOnError      err_cb;         // error callback
    void            *err_cb_data;   // error callback data
    uint8_t b_del_handler;
} RilClientPrv;


//---------------------------------------------------------------------------
// Local static function prototypes
//---------------------------------------------------------------------------
static void * RxReaderFunc(void *param);
static int processRxBuffer(RilClientPrv *prv, void *buffer, size_t buflen);
static uint32_t AllocateToken(uint32_t *token_pool);
static void FreeToken(uint32_t *token_pool, uint32_t token);
static uint8_t IsValidToken(uint32_t *token_pool, uint32_t token);
static int blockingWrite(int fd, const void *buffer, size_t len);
static int RecordReqHistory(RilClientPrv *prv, int token, uint32_t id);
static void ClearReqHistory(RilClientPrv *prv, int token);
static RilOnComplete FindReqHandler(RilClientPrv *prv, int token, uint32_t *id);
static RilOnUnsolicited FindUnsolHandler(RilClientPrv *prv, uint32_t id);
static int SendOemRequestHookRaw(HRilClient client, int req_id, char *data, size_t len);

/**
 * @fn  int RegisterUnsolicitedHandler(HRilClient client, uint32_t id, RilOnUnsolicited handler)
 *
 * @params  client: Client handle.
 *          id: Unsolicited response ID to which handler is registered.
 *          handler: Unsolicited handler. NULL for deregistration.
 *
 * @return  0 on success or error code.
 */
extern "C"
int RegisterUnsolicitedHandler(HRilClient client, uint32_t id, RilOnUnsolicited handler) {
    RilClientPrv *client_prv;
    int match_slot = -1;
    int first_empty_slot = -1;
    int i;

    if (client == NULL || client->prv == NULL)
        return RIL_CLIENT_ERR_INVAL;

    client_prv = (RilClientPrv *)(client->prv);

    for (i = 0; i < REQ_POOL_SIZE; i++) {
        // Check if  there is matched handler.
        if (id == client_prv->unsol_handlers[i].id) {
            match_slot = i;
        }
        // Find first empty handler slot.
        if (first_empty_slot == -1 && client_prv->unsol_handlers[i].id == 0) {
            first_empty_slot = i;
        }
    }

    if (handler == NULL) {  // Unregister.
        if (match_slot >= 0) {
            memset(&(client_prv->unsol_handlers[match_slot]), 0, sizeof(UnsolHandler));
            return RIL_CLIENT_ERR_SUCCESS;
        }
        else {
            return RIL_CLIENT_ERR_SUCCESS;
        }
    }
    else {// Register.
        if (match_slot >= 0) {
            client_prv->unsol_handlers[match_slot].handler = handler;   // Just update.
        }
        else if (first_empty_slot >= 0) {
            client_prv->unsol_handlers[first_empty_slot].id = id;
            client_prv->unsol_handlers[first_empty_slot].handler = handler;
        }
        else {
            return RIL_CLIENT_ERR_RESOURCE;
        }
    }

    return RIL_CLIENT_ERR_SUCCESS;
}


/**
 * @fn  int RegisterRequestCompleteHandler(HRilClient client, uint32_t id, RilOnComplete handler)
 *
 * @params  client: Client handle.
 *          id: Request ID to which handler is registered.
 *          handler: Request complete handler. NULL for deregistration.
 *
 * @return  0 on success or error code.
 */
extern "C"
int RegisterRequestCompleteHandler(HRilClient client, uint32_t id, RilOnComplete handler) {
    RilClientPrv *client_prv;
    int match_slot = -1;
    int first_empty_slot = -1;
    int i;

    if (client == NULL || client->prv == NULL)
        return RIL_CLIENT_ERR_INVAL;

    client_prv = (RilClientPrv *)(client->prv);

    for (i = 0; i < REQ_POOL_SIZE; i++) {
        // Check if  there is matched handler.
        if (id == client_prv->req_handlers[i].id) {
            match_slot = i;
        }
        // Find first empty handler slot.
        if (first_empty_slot == -1 && client_prv->req_handlers[i].id == 0) {
            first_empty_slot = i;
        }
    }

    if (handler == NULL) {  // Unregister.
        if (match_slot >= 0) {
            memset(&(client_prv->req_handlers[match_slot]), 0, sizeof(ReqRespHandler));
            return RIL_CLIENT_ERR_SUCCESS;
        }
        else {
            return RIL_CLIENT_ERR_SUCCESS;
        }
    }
    else {  // Register.
        if (match_slot >= 0) {
            client_prv->req_handlers[match_slot].handler = handler; // Just update.
        }
        else if (first_empty_slot >= 0) {
            client_prv->req_handlers[first_empty_slot].id = id;
            client_prv->req_handlers[first_empty_slot].handler = handler;
        }
        else {
            return RIL_CLIENT_ERR_RESOURCE;
        }
    }

    return RIL_CLIENT_ERR_SUCCESS;
}


/**
 * @fn  int RegisterErrorCallback(HRilClient client, RilOnError cb, void *data)
 *
 * @params  client: Client handle.
 *          cb: Error callback. NULL for unregistration.
 *          data: Callback data.
 *
 * @return  0 for success or error code.
 */
extern "C"
int RegisterErrorCallback(HRilClient client, RilOnError cb, void *data) {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL)
        return RIL_CLIENT_ERR_INVAL;

    client_prv = (RilClientPrv *)(client->prv);

    client_prv->err_cb = cb;
    client_prv->err_cb_data = data;

    return RIL_CLIENT_ERR_SUCCESS;
}


/**
 * @fn  HRilClient OpenClient_RILD(void)
 *
 * @params  None.
 *
 * @return  Client handle, NULL on error.
 */
extern "C"
HRilClient OpenClient_RILD(void) {
    HRilClient client = (HRilClient)malloc(sizeof(struct RilClient));
    if (client == NULL)
        return NULL;

    client->prv = (RilClientPrv *)malloc(sizeof(RilClientPrv));
    if (client->prv == NULL) {
        free(client);
        return NULL;
    }

    memset(client->prv, 0, sizeof(RilClientPrv));

    ((RilClientPrv *)(client->prv))->parent = client;
    ((RilClientPrv *)(client->prv))->sock = -1;

    return client;
}


/**
 * @fn  int Connect_RILD(void)
 *
 * @params  client: Client handle.
 *
 * @return  0, or error code.
 */
extern "C"
int Connect_RILD(HRilClient client) {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL) {
        ALOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    // Open client socket and connect to server.
    client_prv->sock = socket_local_client(MULTI_CLIENT_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM );

    if (client_prv->sock < 0) {
        ALOGE("%s: Connecting failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_CONNECT;
    }

    client_prv->b_connect = 1;

    if (fcntl(client_prv->sock, F_SETFL, O_NONBLOCK) < 0) {
        close(client_prv->sock);
        return RIL_CLIENT_ERR_IO;
    }

    client_prv->p_rs = record_stream_new(client_prv->sock, MAX_COMMAND_BYTES);

    if (pipe(client_prv->pipefd) < 0) {
        close(client_prv->sock);
        ALOGE("%s: Creating command pipe failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_IO;
    }

    if (fcntl(client_prv->pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
        close(client_prv->sock);
        close(client_prv->pipefd[0]);
        close(client_prv->pipefd[1]);
        return RIL_CLIENT_ERR_IO;
    }

    // Start socket read thread.
    if (pthread_create(&(client_prv->tid_reader), NULL, RxReaderFunc, (void *)client_prv) != 0) {
        close(client_prv->sock);
        close(client_prv->pipefd[0]);
        close(client_prv->pipefd[1]);

        memset(client_prv, 0, sizeof(RilClientPrv));
        client_prv->sock = -1;
        ALOGE("%s: Can't create Reader thread. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_CONNECT;
    }

    return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * @fn  int Disconnect_RILD(HRilClient client)
 *
 * @params  client: Client handle.
 *
 * @return  0 on success, or error code.
 */
extern "C"
int Disconnect_RILD(HRilClient client) {
    RilClientPrv *client_prv;
    int ret = 0;

    if (client == NULL || client->prv == NULL) {
        ALOGE("%s: invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock == -1)
        return RIL_CLIENT_ERR_SUCCESS;

    ALOGD("[*] %s(): sock=%d\n", __FUNCTION__, client_prv->sock);

    if (client_prv->sock > 0) {
        do {
            ret = write(client_prv->pipefd[1], "close", strlen("close"));
        } while (ret < 0 && errno == EINTR);
    }

    client_prv->b_connect = 0;

    pthread_join(client_prv->tid_reader, NULL);

    return RIL_CLIENT_ERR_SUCCESS;
}


/**
 * @fn  int CloseClient_RILD(HRilClient client)
 *
 * @params  client: Client handle.
 *
 * @return  0 on success, or error code.
 */
extern "C"
int CloseClient_RILD(HRilClient client) {
    if (client == NULL || client->prv == NULL) {
        ALOGE("%s: invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    Disconnect_RILD(client);

    free(client->prv);
    free(client);

    return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * @fn  int InvokeOemRequestHookRaw(HRilClient client, char *data, size_t len)
 *
 * @params  client: Client handle.
 *          data: Request data.
 *          len: Request data length.
 *
 * @return  0 for success or error code. On receiving RIL_CLIENT_ERR_AGAIN,
 *          caller should retry.
 */
extern "C"
int InvokeOemRequestHookRaw(HRilClient client, char *data, size_t len) {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL) {
        ALOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        ALOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    return SendOemRequestHookRaw(client, REQ_OEM_HOOK_RAW, data, len);
}


static int SendOemRequestHookRaw(HRilClient client, int req_id, char *data, size_t len) {
    int token = 0;
    int ret = 0;
    uint32_t header = 0;
    android::Parcel p;
    RilClientPrv *client_prv;

    client_prv = (RilClientPrv *)(client->prv);

    // Allocate a token.
    token = AllocateToken(&(client_prv->token_pool));
    if (token == 0) {
        ALOGE("%s: No token.", __FUNCTION__);
        return RIL_CLIENT_ERR_AGAIN;
    }

    // Record token for the request sent.
    if (RecordReqHistory(client_prv, token, req_id) != RIL_CLIENT_ERR_SUCCESS) {
        goto error;
    }

    // Make OEM request data.
    p.writeInt32(RIL_REQUEST_OEM_HOOK_RAW);
    p.writeInt32(token);
    p.writeInt32(len);
    p.write((void *)data, len);

    // DO TX: header(size).
    header = htonl(p.dataSize());

    if (DBG) ALOGD("%s(): token = %d\n", __FUNCTION__, token);

    ret = blockingWrite(client_prv->sock, (void *)&header, sizeof(header));
    if (ret < 0) {
        ALOGE("%s: send request header failed. (%d)", __FUNCTION__, ret);
        goto error;
    }

    // Do TX: response data.
    ret = blockingWrite(client_prv->sock, p.data(), p.dataSize());
    if (ret < 0) {
        ALOGE("%s: send request data failed. (%d)", __FUNCTION__, ret);
        goto error;
    }

    return RIL_CLIENT_ERR_SUCCESS;

error:
    FreeToken(&(client_prv->token_pool), token);
    ClearReqHistory(client_prv, token);

    return RIL_CLIENT_ERR_UNKNOWN;
}

static void * RxReaderFunc(void *param) {
    RilClientPrv *client_prv = (RilClientPrv *)param;
    int maxfd = 0;
    void *p_record = NULL;
    size_t recordlen = 0;
    int ret = 0;
    int n;

    if (client_prv == NULL)
        return NULL;

    maxfd = max(client_prv->sock, client_prv->pipefd[0]) + 1;

    ALOGD("[*] %s() b_connect=%d, maxfd=%d\n", __FUNCTION__, client_prv->b_connect, maxfd);
    while (client_prv->b_connect) {
        FD_ZERO(&(client_prv->sock_rfds));

        FD_SET(client_prv->sock, &(client_prv->sock_rfds));
        FD_SET(client_prv->pipefd[0], &(client_prv->sock_rfds));

        if (DBG) ALOGD("[*] %s() b_connect=%d\n", __FUNCTION__, client_prv->b_connect);
        if (select(maxfd, &(client_prv->sock_rfds), NULL, NULL, NULL) > 0) {
            if (FD_ISSET(client_prv->sock, &(client_prv->sock_rfds))) {
                // Read incoming data
                for (;;) {
                    // loop until EAGAIN/EINTR, end of stream, or other error
                    ret = record_stream_get_next(client_prv->p_rs, &p_record, &recordlen);
                    if (ret == 0 && p_record == NULL) { // end-of-stream
                        break;
                    }
                    else if (ret < 0) {
                        break;
                    }
                    else if (ret == 0) {    // && p_record != NULL
                        n = processRxBuffer(client_prv, p_record, recordlen);
                        if (n != RIL_CLIENT_ERR_SUCCESS) {
                            ALOGE("%s: processRXBuffer returns %d", __FUNCTION__, n);
                        }
                    }
                    else {
                        ALOGD("[*] %s()\n", __FUNCTION__);
                    }
                }

                if (ret == 0 || !(errno == EAGAIN || errno == EINTR)) {
                    // fatal error or end-of-stream
                    if (client_prv->sock > 0) {
                        close(client_prv->sock);
                        client_prv->sock = -1;
                        client_prv->b_connect = 0;
                    }

                    if (client_prv->p_rs)
                        record_stream_free(client_prv->p_rs);

                    // EOS
                    if (client_prv->err_cb) {
                        client_prv->err_cb(client_prv->err_cb_data, RIL_CLIENT_ERR_CONNECT);
                        return NULL;
                    }

                    break;
                }
            }
            if (FD_ISSET(client_prv->pipefd[0], &(client_prv->sock_rfds))) {
                char end_cmd[10];

                if (DBG) ALOGD("%s(): close\n", __FUNCTION__);

                if (read(client_prv->pipefd[0], end_cmd, sizeof(end_cmd)) > 0) {
                    close(client_prv->sock);
                    close(client_prv->pipefd[0]);
                    close(client_prv->pipefd[1]);

                    client_prv->sock = -1;
                    client_prv->b_connect = 0;
                }
            }
        }
    }

    return NULL;
}


static int processUnsolicited(RilClientPrv *prv, Parcel &p) {
    int32_t resp_id, len;
    status_t status;
    const void *data = NULL;
    RilOnUnsolicited unsol_func = NULL;

    status = p.readInt32(&resp_id);
    if (status != NO_ERROR) {
        ALOGE("%s: read resp_id failed.", __FUNCTION__);
        return RIL_CLIENT_ERR_IO;
    }

    status = p.readInt32(&len);
    if (status != NO_ERROR) {
        //LOGE("%s: read length failed. assume zero length.", __FUNCTION__);
        len = 0;
    }

    ALOGD("%s(): resp_id (%d), len(%d)\n", __FUNCTION__, resp_id, len);

    if (len)
        data = p.readInplace(len);

    // Find unsolicited response handler.
    unsol_func = FindUnsolHandler(prv, (uint32_t)resp_id);
    if (unsol_func) {
        unsol_func(prv->parent, data, len);
    }

    return RIL_CLIENT_ERR_SUCCESS;
}


static int processSolicited(RilClientPrv *prv, Parcel &p) {
    int32_t token, err, len;
    status_t status;
    const void *data = NULL;
    RilOnComplete req_func = NULL;
    int ret = RIL_CLIENT_ERR_SUCCESS;
    uint32_t req_id = 0;

    if (DBG) ALOGD("%s()", __FUNCTION__);

    status = p.readInt32(&token);
    if (status != NO_ERROR) {
        ALOGE("%s: Read token fail. Status %d\n", __FUNCTION__, status);
        return RIL_CLIENT_ERR_IO;
    }

    if (IsValidToken(&(prv->token_pool), token) == 0) {
        ALOGE("%s: Invalid Token", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;    // Invalid token.
    }

    status = p.readInt32(&err);
    if (status != NO_ERROR) {
        ALOGE("%s: Read err fail. Status %d\n", __FUNCTION__, status);
        ret = RIL_CLIENT_ERR_IO;
        goto error;
    }

    // Don't go further for error response.
    if (err != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("%s: Error %d\n", __FUNCTION__, err);
        if (prv->err_cb)
            prv->err_cb(prv->err_cb_data, err);
        ret = RIL_CLIENT_ERR_SUCCESS;
        goto error;
    }

    status = p.readInt32(&len);
    if (status != NO_ERROR) {
        /* no length field */
        len = 0;
    }

    if (len)
        data = p.readInplace(len);

    // Find request handler for the token.
    // First, FindReqHandler() searches request history with the token
    // and finds out a request ID. Then, it search request handler table
    // with the request ID.
    req_func = FindReqHandler(prv, token, &req_id);
    if (req_func)
    {
        if (DBG) ALOGD("[*] Call handler");
        req_func(prv->parent, data, len);

        if(prv->b_del_handler) {
         prv->b_del_handler = 0;
            RegisterRequestCompleteHandler(prv->parent, req_id, NULL);
        }
    } else {
        if (DBG) ALOGD("%s: No handler for token %d\n", __FUNCTION__, token);
    }

error:
    FreeToken(&(prv->token_pool), token);
    ClearReqHistory(prv, token);
    return ret;
}


static int processRxBuffer(RilClientPrv *prv, void *buffer, size_t buflen) {
    Parcel p;
    int32_t response_type;
    status_t status;
    int ret = RIL_CLIENT_ERR_SUCCESS;

    acquire_wake_lock(PARTIAL_WAKE_LOCK, RIL_CLIENT_WAKE_LOCK);

    p.setData((uint8_t *)buffer, buflen);

    status = p.readInt32(&response_type);
    if (DBG) ALOGD("%s: status %d response_type %d", __FUNCTION__, status, response_type);

    if (status != NO_ERROR) {
     ret = RIL_CLIENT_ERR_IO;
        goto EXIT;
    }

    // FOr unsolicited response.
    if (response_type == RESPONSE_UNSOLICITED) {
        ret = processUnsolicited(prv, p);
    }
    // For solicited response.
    else if (response_type == RESPONSE_SOLICITED) {
        ret = processSolicited(prv, p);
        if (ret != RIL_CLIENT_ERR_SUCCESS && prv->err_cb) {
            prv->err_cb(prv->err_cb_data, ret);
        }
    }
    else {
        ret =  RIL_CLIENT_ERR_INVAL;
    }

EXIT:
    release_wake_lock(RIL_CLIENT_WAKE_LOCK);
    return ret;
}


static uint32_t AllocateToken(uint32_t *token_pool) {
    int i;

    // Token pool is full.
    if (*token_pool == 0xFFFFFFFF)
        return 0;

    for (i = 0; i < 32; i++) {
        uint32_t new_token = 0x00000001 << i;

        if ((*token_pool & new_token) == 0) {
            *token_pool |= new_token;
            return new_token;
        }
    }

    return 0;
}


static void FreeToken(uint32_t *token_pool, uint32_t token) {
    *token_pool &= ~token;
}


static uint8_t IsValidToken(uint32_t *token_pool, uint32_t token) {
    if (token == 0)
        return 0;

    if ((*token_pool & token) == token)
        return 1;
    else
        return 0;
}


static int RecordReqHistory(RilClientPrv *prv, int token, uint32_t id) {
    int i = 0;

    if (DBG) ALOGD("[*] %s(): token(%d), ID(%d)\n", __FUNCTION__, token, id);
    for (i = 0; i < TOKEN_POOL_SIZE; i++) {
        if (prv->history[i].token == 0) {
            prv->history[i].token = token;
            prv->history[i].id = id;

            if (DBG) ALOGD("[*] %s(): token(%d), ID(%d)\n", __FUNCTION__, token, id);

            return RIL_CLIENT_ERR_SUCCESS;
        }
    }

    ALOGE("%s: No free record for token %d", __FUNCTION__, token);

    return RIL_CLIENT_ERR_RESOURCE;
}

static void ClearReqHistory(RilClientPrv *prv, int token) {
    int i = 0;

    if (DBG) ALOGD("[*] %s(): token(%d)\n", __FUNCTION__, token);
    for (i = 0; i < TOKEN_POOL_SIZE; i++) {
        if (prv->history[i].token == token) {
            memset(&(prv->history[i]), 0, sizeof(ReqHistory));
            break;
        }
    }
}


static RilOnUnsolicited FindUnsolHandler(RilClientPrv *prv, uint32_t id) {
    int i;

    // Search unsolicited handler table.
    for (i = 0; i < REQ_POOL_SIZE; i++) {
        if (prv->unsol_handlers[i].id == id)
            return prv->unsol_handlers[i].handler;
    }

    return (RilOnUnsolicited)NULL;
}


static RilOnComplete FindReqHandler(RilClientPrv *prv, int token, uint32_t *id) {
    int i = 0;
    int j = 0;

    if (DBG) ALOGD("[*] %s(): token(%d)\n", __FUNCTION__, token);

    // Search request history.
    for (i = 0; i < TOKEN_POOL_SIZE; i++) {
        ALOGD("[*] %s(): history_token(%d)\n", __FUNCTION__, prv->history[i].token);
        if (prv->history[i].token == token) {
            // Search request handler with request ID found.
            for (j = 0; j < REQ_POOL_SIZE; j++) {
                ALOGD("[*] %s(): token(%d), req_id(%d), history_id(%d)\n", __FUNCTION__, token, prv->history[i].id, prv->history[i].id);
                if (prv->req_handlers[j].id == prv->history[i].id) {
              *id = prv->req_handlers[j].id;
                    return prv->req_handlers[j].handler;
                }
            }
        }
    }

    return NULL;
}


static int blockingWrite(int fd, const void *buffer, size_t len) {
    size_t writeOffset = 0;
    const uint8_t *toWrite;
    ssize_t written = 0;

    if (buffer == NULL)
        return -1;

    toWrite = (const uint8_t *)buffer;

    while (writeOffset < len) {
        do
        {
            written = write(fd, toWrite + writeOffset, len - writeOffset);
        } while (written < 0 && errno == EINTR);

        if (written >= 0) {
            writeOffset += written;
        }
        else {
            ALOGE ("RIL Response: unexpected error on write errno:%d", errno);
            close(fd);
            return -1;
        }
    }

    return 0;
}

} // namespace android

// end of file

