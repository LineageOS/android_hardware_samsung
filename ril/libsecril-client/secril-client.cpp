/**
 * @file    secril-client.cpp
 *
 * @author  Myeongcheol Kim (mcmount.kim@samsung.com)
 *
 * @brief   RIL client library for multi-client support
 */

#define LOG_TAG "RILClient"
/*#define LOG_NDEBUG 0*/

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
#include "secril-client.h"
#include <hardware_legacy/power.h> // For wakelock


#define RIL_CLIENT_WAKE_LOCK "client-interface"

namespace android {

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
#define RILD_PORT               7777
#ifdef USES_VND_SECRIL
#define MULTI_CLIENT_SOCKET_NAME "VND_Multiclient"
#define MULTI_CLIENT_SOCKET_NAME_2 "VND_Multiclient2"
#else
#define MULTI_CLIENT_SOCKET_NAME "Multiclient"
#define MULTI_CLIENT_SOCKET_NAME_2 "Multiclient2"
#endif
#define MULTI_CLIENT_Q_SOCKET_NAME "QMulticlient"

#define MAX_COMMAND_BYTES       (8 * 1024)
#define REQ_POOL_SIZE           32
#define TOKEN_POOL_SIZE         32

// Constants for response types
#define RESPONSE_SOLICITED      0
#define RESPONSE_UNSOLICITED    1

#define max(a, b)   ((a) > (b) ? (a) : (b))

#define REQ_OEM_HOOK_RAW        RIL_REQUEST_OEM_HOOK_RAW
#define REQ_SET_CALL_VOLUME     101
#define REQ_SET_AUDIO_PATH      102
#define REQ_SET_CALL_CLOCK_SYNC 103
#define REQ_SET_CALL_RECORDING  104
#define REQ_SET_CALL_MUTE           105
#define REQ_GET_CALL_MUTE           106
#define REQ_SET_CALL_VT_CTRL        107
#define REQ_SET_TWO_MIC_CTRL        108
#define REQ_SET_DHA_CTRL        109
#define REQ_SET_LOOPBACK            110

// OEM request function ID
#define OEM_FUNC_SOUND          0x08

// OEM request sub function ID
#define OEM_SND_SET_VOLUME_CTRL     0x03
#define OEM_SND_SET_AUDIO_PATH      0x05
#define OEM_SND_GET_AUDIO_PATH      0x06
#define OEM_SND_SET_VIDEO_CALL_CTRL 0x07
#define OEM_SND_SET_LOOPBACK_CTRL 0x08
#define OEM_SND_SET_VOICE_RECORDING_CTRL  0x09
#define OEM_SND_SET_CLOCK_CTRL      0x0A
#define OEM_SND_SET_MUTE        0x0B
#define OEM_SND_GET_MUTE        0x0C
#define OEM_SND_SET_TWO_MIC_CTL     0x0D
#define OEM_SND_SET_DHA_CTL     0x0E

#define OEM_SND_TYPE_VOICE          0x01 // Receiver(0x00) + Voice(0x01)
#define OEM_SND_TYPE_SPEAKER        0x11 // SpeakerPhone(0x10) + Voice(0x01)
#define OEM_SND_TYPE_HEADSET        0x31 // Headset(0x30) + Voice(0x01)
#define OEM_SND_TYPE_BTVOICE        0x41 // BT(0x40) + Voice(0x01)

#ifdef SAMSUNG_NEXT_GEN_MODEM
#define OEM_SND_AUDIO_PATH_EARPIECE           0x01
#define OEM_SND_AUDIO_PATH_HEADSET            0x02
#define OEM_SND_AUDIO_PATH_HFK                0x06
#define OEM_SND_AUDIO_PATH_BLUETOOTH          0x04
#define OEM_SND_AUDIO_PATH_STEREO_BLUETOOTH   0x05
#define OEM_SND_AUDIO_PATH_SPEAKER            0x07
#define OEM_SND_AUDIO_PATH_HEADPHONE          0x08
#define OEM_SND_AUDIO_PATH_BT_NSEC_OFF        0x09
#define OEM_SND_AUDIO_PATH_MIC1               0x0A
#define OEM_SND_AUDIO_PATH_MIC2               0x0B
#define OEM_SND_AUDIO_PATH_BT_WB              0x0C
#define OEM_SND_AUDIO_PATH_BT_WB_NSEC_OFF     0x0D
#else
#define OEM_SND_AUDIO_PATH_EARPIECE     0x01
#define OEM_SND_AUDIO_PATH_HEADSET      0x02
#define OEM_SND_AUDIO_PATH_HFK                0x03
#define OEM_SND_AUDIO_PATH_BLUETOOTH    0x04
#define OEM_SND_AUDIO_PATH_STEREO_BLUETOOTH   0x05
#define OEM_SND_AUDIO_PATH_SPEAKER      0x06
#define OEM_SND_AUDIO_PATH_HEADPHONE      0x07
#define OEM_SND_AUDIO_PATH_BT_NSEC_OFF  0x08
#define OEM_SND_AUDIO_PATH_MIC1 0x09
#define OEM_SND_AUDIO_PATH_MIC2 0x0A
#define OEM_SND_AUDIO_PATH_BT_WB  0x0B
#define OEM_SND_AUDIO_PATH_BT_WB_NSEC_OFF  0x0C
#endif

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
static bool isValidSoundType(SoundType type);
static bool isValidAudioPath(AudioPath path);
static bool isValidSoundClockCondition(SoundClockCondition condition);
static bool isValidCallRecCondition(CallRecCondition condition);
static bool isValidMuteCondition(MuteCondition condition);
static bool isValidTwoMicCtrl(TwoMicSolDevice device, TwoMicSolReport report);
static char ConvertSoundType(SoundType type);
static char ConvertAudioPath(AudioPath path);


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
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    // Open client socket and connect to server.
    //client_prv->sock = socket_loopback_client(RILD_PORT, SOCK_STREAM);
    client_prv->sock = socket_local_client(MULTI_CLIENT_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM );

    if (client_prv->sock < 0) {
        RLOGE("%s: Connecting failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Creating command pipe failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Can't create Reader thread. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_CONNECT;
    }

    return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * @fn  int Connect_QRILD(void)
 *
 * @params  client: Client handle.
 *
 * @return  0, or error code.
 */
extern "C"
int Connect_QRILD(HRilClient client) {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    // Open client socket and connect to server.
    //client_prv->sock = socket_loopback_client(RILD_PORT, SOCK_STREAM);
    client_prv->sock = socket_local_client(MULTI_CLIENT_Q_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM );

    if (client_prv->sock < 0) {
        RLOGE("%s: Connecting failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Creating command pipe failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Can't create Reader thread. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_CONNECT;
    }

    return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * @fn    int Connect_RILD_Second(void)
 *
 * @params    client: Client handle.
 *
 * @return    0, or error code.
 */
extern "C"
int Connect_RILD_Second(HRilClient client)    {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    // Open client socket and connect to server.
    //client_prv->sock = socket_loopback_client(RILD_PORT, SOCK_STREAM);
    client_prv->sock = socket_local_client(MULTI_CLIENT_SOCKET_NAME_2, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM );

    if (client_prv->sock < 0) {
        RLOGE("%s: Connecting failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Creating command pipe failed. %s(%d)", __FUNCTION__, strerror(errno), errno);
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
        RLOGE("%s: Can't create Reader thread. %s(%d)", __FUNCTION__, strerror(errno), errno);
        return RIL_CLIENT_ERR_CONNECT;
    }

    return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * @fn  int isConnected_RILD(HRilClient client)
 *
 * @params  client: Client handle.
 *
 * @return  0, or 1.
 */
extern "C"
int isConnected_RILD(HRilClient client) {
    RilClientPrv *client_prv;

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    return client_prv->b_connect == 1;
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
        RLOGE("%s: invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock == -1)
        return RIL_CLIENT_ERR_SUCCESS;

    printf("[*] %s(): sock=%d\n", __FUNCTION__, client_prv->sock);

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
        RLOGE("%s: invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    Disconnect_RILD(client);

    free(client->prv);
    free(client);

    return RIL_CLIENT_ERR_SUCCESS;
}


/**
 * Set in-call volume.
 */
extern "C"
int SetCallVolume(HRilClient client, SoundType type, int vol_level) {
    RilClientPrv *client_prv;
    int ret;
    char data[6] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidSoundType(type) == false) {
        RLOGE("%s: Invalid sound type", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_VOLUME_CTRL;
    data[2] = 0x00;         // data length
    data[3] = 0x06;         // data length
    data[4] = ConvertSoundType(type);   // volume type
    data[5] = vol_level;    // volume level

    RegisterRequestCompleteHandler(client, REQ_SET_CALL_VOLUME, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_CALL_VOLUME, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_CALL_VOLUME, NULL);
    }

    return ret;
}


/**
 * Set external sound device path for noise reduction.
 */
extern "C"
#ifdef RIL_CALL_AUDIO_PATH_EXTRAVOLUME
int SetCallAudioPath(HRilClient client, AudioPath path, ExtraVolume mode)
#else
int SetCallAudioPath(HRilClient client, AudioPath path)
#endif
{
    RilClientPrv *client_prv;
    int ret;
    char data[6] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidAudioPath(path) == false) {
        RLOGE("%s: Invalid audio path", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_AUDIO_PATH;
    data[2] = 0x00;     // data length
    data[3] = 0x06;     // data length
    data[4] = ConvertAudioPath(path); // audio path
#ifdef RIL_CALL_AUDIO_PATH_EXTRAVOLUME
    data[5] = mode; // ExtraVolume
#endif

    RegisterRequestCompleteHandler(client, REQ_SET_AUDIO_PATH, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_AUDIO_PATH, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_AUDIO_PATH, NULL);
    }

    return ret;
}


/**
 * Set modem clock to master or slave.
 */
extern "C"
int SetCallClockSync(HRilClient client, SoundClockCondition condition) {
    RilClientPrv *client_prv;
    int ret;
    char data[5] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidSoundClockCondition(condition) == false) {
        RLOGE("%s: Invalid sound clock condition", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_CLOCK_CTRL;
    data[2] = 0x00; // data length
    data[3] = 0x05; // data length
    data[4] = condition;

    RegisterRequestCompleteHandler(client, REQ_SET_CALL_CLOCK_SYNC, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_CALL_CLOCK_SYNC, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_CALL_CLOCK_SYNC, NULL);
    }

    return ret;
}

/**
 * Set modem VTCall clock to master or slave.
 */
extern "C"
int SetVideoCallClockSync(HRilClient client, SoundClockCondition condition) {
    RilClientPrv *client_prv;
    int ret;
    char data[5] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidSoundClockCondition(condition) == false) {
        RLOGE("%s: Invalid sound clock condition", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_VIDEO_CALL_CTRL;
    data[2] = 0x00; // data length
    data[3] = 0x05; // data length
    data[4] = condition;

    RegisterRequestCompleteHandler(client, REQ_SET_CALL_VT_CTRL, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_CALL_VT_CTRL, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_CALL_VT_CTRL, NULL);
    }

    return ret;
}

/**
 * Set voice recording.
 */
extern "C"
int SetCallRecord(HRilClient client, CallRecCondition condition) {
    RilClientPrv *client_prv;
    int ret;
    char data[5] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidCallRecCondition(condition) == false) {
        RLOGE("%s: Invalid sound clock condition", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_VOICE_RECORDING_CTRL;
    data[2] = 0x00; // data length
    data[3] = 0x05; // data length
    data[4] = condition;

    RegisterRequestCompleteHandler(client, REQ_SET_CALL_RECORDING, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_CALL_RECORDING, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_CALL_RECORDING, NULL);
    }

    return ret;
}

/**
 * Set mute or unmute.
 */
extern "C"
int SetMute(HRilClient client, MuteCondition condition) {
    RilClientPrv *client_prv;
    int ret;
    char data[5] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidMuteCondition(condition) == false) {
        RLOGE("%s: Invalid sound clock condition", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_MUTE;
    data[2] = 0x00; // data length
    data[3] = 0x05; // data length
    data[4] = condition;

    RegisterRequestCompleteHandler(client, REQ_SET_CALL_MUTE, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_CALL_MUTE, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_CALL_MUTE, NULL);
    }

    return ret;
}

/**
 * Get mute state.
 */
extern "C"
int GetMute(HRilClient client, RilOnComplete handler) {
    RilClientPrv *client_prv;
    int ret;
    char data[4] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    client_prv->b_del_handler = 1;

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_GET_MUTE;
    data[2] = 0x00; // data length
    data[3] = 0x04; // data length

    RegisterRequestCompleteHandler(client, REQ_GET_CALL_MUTE, handler);

    ret = SendOemRequestHookRaw(client, REQ_GET_CALL_MUTE, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_GET_CALL_MUTE, NULL);
    }

    return ret;
}

extern "C"
int SetTwoMicControl(HRilClient client, TwoMicSolDevice device, TwoMicSolReport report) {
    RilClientPrv *client_prv;
    int ret;
    char data[6] = {0,};

    RLOGV(" + %s", __FUNCTION__);

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    if (isValidTwoMicCtrl(device, report) == false) {
        RLOGE("%s: Invalid sound set two params", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_TWO_MIC_CTL;
    data[2] = 0x00; // data length
    data[3] = 0x06; // data length
    data[4] = device;
    data[5] = report;

    RegisterRequestCompleteHandler(client, REQ_SET_TWO_MIC_CTRL, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_TWO_MIC_CTRL, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_TWO_MIC_CTRL, NULL);
    }

    RLOGV(" - %s", __FUNCTION__);

    return ret;
}

extern "C"
int SetDhaSolution(HRilClient client, DhaSolMode mode, DhaSolSelect select, char *parameter) {
    RilClientPrv *client_prv;
    int ret;
    char data[30] = {0,};
    char tempPara[24]={0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    RLOGE("%s: DHA mode=%d, select=%d", __FUNCTION__,mode, select);

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_DHA_CTL;
    data[2] = 0x00; // data length
    data[3] = 0x1E; // data length
    data[4] = mode;
    data[5] = select;

    memcpy(tempPara, parameter, 24);
    for(int i=0; i<24; i++)
         data[6+i]= tempPara[i];

    RegisterRequestCompleteHandler(client, REQ_SET_DHA_CTRL, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_DHA_CTRL, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_DHA_CTRL, NULL);
    }

    return ret;
}

/**
 * Set LoopbackTest mode, path.
 */
extern "C"
int SetLoopbackTest(HRilClient client, LoopbackMode mode, AudioPath path) {
    RilClientPrv *client_prv;
    int ret;
    char data[6] = {0,};

    if (client == NULL || client->prv == NULL) {
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
        return RIL_CLIENT_ERR_CONNECT;
    }

    // Make raw data
    data[0] = OEM_FUNC_SOUND;
    data[1] = OEM_SND_SET_LOOPBACK_CTRL;
    data[2] = 0x00;     // data length
    data[3] = 0x06;     // data length
    data[4] = mode; // Loopback Mode
    data[5] = ConvertAudioPath(path); // Loopback path

    RegisterRequestCompleteHandler(client, REQ_SET_LOOPBACK, NULL);

    ret = SendOemRequestHookRaw(client, REQ_SET_LOOPBACK, data, sizeof(data));
    if (ret != RIL_CLIENT_ERR_SUCCESS) {
        RegisterRequestCompleteHandler(client, REQ_SET_LOOPBACK, NULL);
    }

    return ret;
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
        RLOGE("%s: Invalid client %p", __FUNCTION__, client);
        return RIL_CLIENT_ERR_INVAL;
    }

    client_prv = (RilClientPrv *)(client->prv);

    if (client_prv->sock < 0 ) {
        RLOGE("%s: Not connected.", __FUNCTION__);
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

    unsigned int check_req_id = req_id;

    client_prv = (RilClientPrv *)(client->prv);

    // Allocate a token.
    token = AllocateToken(&(client_prv->token_pool));
    if (token == 0) {
        RLOGE("%s: No token.", __FUNCTION__);
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

    RLOGV("%s(): token = %d\n", __FUNCTION__, token);

    ret = blockingWrite(client_prv->sock, (void *)&header, sizeof(header));
    if (ret < 0) {
        RLOGE("%s: send request header failed. (%d)", __FUNCTION__, ret);
        goto error;
    }

    // Do TX: response data.
    ret = blockingWrite(client_prv->sock, p.data(), p.dataSize());
    if (ret < 0) {
        RLOGE("%s: send request data failed. (%d)", __FUNCTION__, ret);
        goto error;
    }

    // check if the handler for specified event is NULL and deregister token
    // to prevent token pool overflow
    if(!FindReqHandler(client_prv, token, &check_req_id)) {
        FreeToken(&(client_prv->token_pool), token);
        ClearReqHistory(client_prv, token);
    }

    return RIL_CLIENT_ERR_SUCCESS;

error:
    FreeToken(&(client_prv->token_pool), token);
    ClearReqHistory(client_prv, token);

    if (ret == -EPIPE || ret == -EBADFD) {
        close(client_prv->sock);
        client_prv->sock = -1;
        client_prv->b_connect = 0;
    }

    return RIL_CLIENT_ERR_UNKNOWN;
}


static bool isValidSoundType(SoundType type) {
    return (type >= SOUND_TYPE_VOICE && type <= SOUND_TYPE_BTVOICE);
}


static bool isValidAudioPath(AudioPath path) {
    return (path >= SOUND_AUDIO_PATH_EARPIECE && path <= OEM_SND_AUDIO_PATH_BT_WB_NSEC_OFF);
}


static bool isValidSoundClockCondition(SoundClockCondition condition) {
    return (condition >= SOUND_CLOCK_STOP && condition <= SOUND_CLOCK_START);
}

static bool isValidCallRecCondition(CallRecCondition condition) {
    return (condition >= CALL_REC_STOP && condition <= CALL_REC_START);
}

static bool isValidMuteCondition(MuteCondition condition) {
    return (condition >= TX_UNMUTE && condition <= RXTX_MUTE);
}

static bool isValidTwoMicCtrl(TwoMicSolDevice device, TwoMicSolReport report) {
    return (device >= AUDIENCE && device <= FORTEMEDIA && report >= TWO_MIC_SOLUTION_OFF && report <= TWO_MIC_SOLUTION_ON  );
}


static char ConvertSoundType(SoundType type) {
    switch (type) {
        case SOUND_TYPE_VOICE:
            return OEM_SND_TYPE_VOICE;
        case SOUND_TYPE_SPEAKER:
            return OEM_SND_TYPE_SPEAKER;
        case SOUND_TYPE_HEADSET:
            return OEM_SND_TYPE_HEADSET;
        case SOUND_TYPE_BTVOICE:
            return OEM_SND_TYPE_BTVOICE;
        default:
            return OEM_SND_TYPE_VOICE;
    }
}


static char ConvertAudioPath(AudioPath path) {
    switch (path) {
        case SOUND_AUDIO_PATH_EARPIECE:
            return OEM_SND_AUDIO_PATH_EARPIECE;
        case SOUND_AUDIO_PATH_HEADSET:
            return OEM_SND_AUDIO_PATH_HEADSET;
        case SOUND_AUDIO_PATH_SPEAKER:
            return OEM_SND_AUDIO_PATH_SPEAKER;
        case SOUND_AUDIO_PATH_BLUETOOTH:
            return OEM_SND_AUDIO_PATH_BLUETOOTH;
        case SOUND_AUDIO_PATH_STEREO_BT:
            return OEM_SND_AUDIO_PATH_STEREO_BLUETOOTH;
        case SOUND_AUDIO_PATH_HEADPHONE:
            return OEM_SND_AUDIO_PATH_HEADPHONE;
        case SOUND_AUDIO_PATH_BLUETOOTH_NO_NR:
            return OEM_SND_AUDIO_PATH_BT_NSEC_OFF;
        case SOUND_AUDIO_PATH_MIC1:
            return OEM_SND_AUDIO_PATH_MIC1;
        case SOUND_AUDIO_PATH_MIC2:
            return OEM_SND_AUDIO_PATH_MIC2;
        case SOUND_AUDIO_PATH_BLUETOOTH_WB:
            return OEM_SND_AUDIO_PATH_BT_WB;
        case SOUND_AUDIO_PATH_BLUETOOTH_WB_NO_NR:
            return OEM_SND_AUDIO_PATH_BT_WB_NSEC_OFF;

        default:
            return OEM_SND_AUDIO_PATH_EARPIECE;
    }
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

    printf("[*] %s() b_connect=%d, maxfd=%d\n", __FUNCTION__, client_prv->b_connect, maxfd);
    while (client_prv->b_connect) {
        FD_ZERO(&(client_prv->sock_rfds));

        FD_SET(client_prv->sock, &(client_prv->sock_rfds));
        FD_SET(client_prv->pipefd[0], &(client_prv->sock_rfds));

        RLOGV("[*] %s() b_connect=%d\n", __FUNCTION__, client_prv->b_connect);
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
                            RLOGE("%s: processRXBuffer returns %d", __FUNCTION__, n);
                        }
                    }
                    else {
                        printf("[*] %s()\n", __FUNCTION__);
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

                RLOGV("%s(): close\n", __FUNCTION__);

                if (read(client_prv->pipefd[0], end_cmd, sizeof(end_cmd)) > 0) {
                    close(client_prv->sock);
                    close(client_prv->pipefd[0]);
                    close(client_prv->pipefd[1]);

                    client_prv->sock = -1;
                    client_prv->b_connect = 0;
                }
            }
        } else {
            RLOGE("%s: select() returned %d\n", __FUNCTION__, -errno);

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
        RLOGE("%s: read resp_id failed.", __FUNCTION__);
        return RIL_CLIENT_ERR_IO;
    }

    status = p.readInt32(&len);
    if (status != NO_ERROR) {
        //RLOGE("%s: read length failed. assume zero length.", __FUNCTION__);
        len = 0;
    }

    RLOGD("%s(): resp_id (%d), len(%d)\n", __FUNCTION__, resp_id, len);

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

    RLOGV("%s()", __FUNCTION__);

    status = p.readInt32(&token);
    if (status != NO_ERROR) {
        RLOGE("%s: Read token fail. Status %d\n", __FUNCTION__, status);
        return RIL_CLIENT_ERR_IO;
    }

    if (IsValidToken(&(prv->token_pool), token) == 0) {
        RLOGE("%s: Invalid Token", __FUNCTION__);
        return RIL_CLIENT_ERR_INVAL;    // Invalid token.
    }

    status = p.readInt32(&err);
    if (status != NO_ERROR) {
        RLOGE("%s: Read err fail. Status %d\n", __FUNCTION__, status);
        ret = RIL_CLIENT_ERR_IO;
        goto error;
    }

    // Don't go further for error response.
    if (err != RIL_CLIENT_ERR_SUCCESS) {
        RLOGE("%s: Error %d\n", __FUNCTION__, err);
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
        RLOGV("[*] Call handler");
        req_func(prv->parent, data, len);

        if(prv->b_del_handler) {
         prv->b_del_handler = 0;
            RegisterRequestCompleteHandler(prv->parent, req_id, NULL);
        }
    } else {
        RLOGV("%s: No handler for token %d\n", __FUNCTION__, token);
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
    RLOGV("%s: status %d response_type %d", __FUNCTION__, status, response_type);

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

    RLOGV("[*] %s(): token(%d), ID(%d)\n", __FUNCTION__, token, id);
    for (i = 0; i < TOKEN_POOL_SIZE; i++) {
        if (prv->history[i].token == 0) {
            prv->history[i].token = token;
            prv->history[i].id = id;

            RLOGV("[*] %s(): token(%d), ID(%d)\n", __FUNCTION__, token, id);

            return RIL_CLIENT_ERR_SUCCESS;
        }
    }

    RLOGE("%s: No free record for token %d", __FUNCTION__, token);

    return RIL_CLIENT_ERR_RESOURCE;
}

static void ClearReqHistory(RilClientPrv *prv, int token) {
    int i = 0;

    RLOGV("[*] %s(): token(%d)\n", __FUNCTION__, token);
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

    RLOGV("[*] %s(): token(%d)\n", __FUNCTION__, token);

    // Search request history.
    for (i = 0; i < TOKEN_POOL_SIZE; i++) {
        printf("[*] %s(): history_token(%d)\n", __FUNCTION__, prv->history[i].token);
        if (prv->history[i].token == token) {
            // Search request handler with request ID found.
            for (j = 0; j < REQ_POOL_SIZE; j++) {
                printf("[*] %s(): token(%d), req_id(%d), history_id(%d)\n", __FUNCTION__, token, prv->history[i].id, prv->history[i].id);
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
            RLOGE ("RIL Response: unexpected error on write errno:%d", errno);
            close(fd);
            return -errno;
        }
    }

    return 0;
}

} // namespace android

// end of file

