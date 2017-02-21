/**
 * @file    secril-client.h
 *
 * @author  Myeongcheol Kim (mcmount.kim@samsung.com)
 *
 * @brief   RIL client library for multi-client support
 */

#ifndef __SECRIL_CLIENT_H__
#define __SECRIL_CLIENT_H__

#include <sys/types.h>
//#include "SecProductFeature_RIL.h"

#ifdef __cplusplus
extern "C" {
#endif

struct RilClient {
    void *prv;
};

typedef struct RilClient * HRilClient;


//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
#define RIL_CLIENT_ERR_SUCCESS      0
#define RIL_CLIENT_ERR_AGAIN        1
#define RIL_CLIENT_ERR_INIT         2   // Client is not initialized
#define RIL_CLIENT_ERR_INVAL        3   // Invalid value
#define RIL_CLIENT_ERR_CONNECT      4   // Connection error
#define RIL_CLIENT_ERR_IO           5   // IO error
#define RIL_CLIENT_ERR_RESOURCE     6   // Resource not available
#define RIL_CLIENT_ERR_UNKNOWN      7


//---------------------------------------------------------------------------
// Type definitions
//---------------------------------------------------------------------------

typedef int (*RilOnComplete)(HRilClient handle, const void *data, size_t datalen);

typedef int (*RilOnUnsolicited)(HRilClient handle, const void *data, size_t datalen);

typedef int (*RilOnError)(void *data, int error);


//---------------------------------------------------------------------------
// Client APIs
//---------------------------------------------------------------------------

/**
 * Open RILD multi-client.
 * Return is client handle, NULL on error.
 */
HRilClient OpenClient_RILD(void);

/**
 * Stop RILD multi-client. If client socket was connected,
 * it will be disconnected.
 */
int CloseClient_RILD(HRilClient client);

/**
 * Connect to RIL deamon. One client task starts.
 * Return is 0 or error code.
 */
int Connect_RILD(HRilClient client);

/**
 * Connect to QRIL deamon. One client task starts.
 * Return is 0 or error code.
 */
int Connect_QRILD(HRilClient client);

/**
 * Connect to RIL deamon. One client task starts.
 * Return is 0 or error code.
 */
int Connect_RILD_Second(HRilClient client);

/**
 * check whether RILD is connected
 * Returns 0 or 1
 */
int isConnected_RILD(HRilClient client);

/**
 * Disconnect connection to RIL deamon(socket close).
 * Return is 0 or error code.
 */
int Disconnect_RILD(HRilClient client);

/**
 * Register unsolicited response handler. If handler is NULL,
 * the handler for the request ID is unregistered.
 * The response handler is invoked in the client task context.
 * Return is 0 or error code.
 */
int RegisterUnsolicitedHandler(HRilClient client, uint32_t id, RilOnUnsolicited handler);

/**
 * Register solicited response handler. If handler is NULL,
 * the handler for the ID is unregistered.
 * The response handler is invoked in the client task context.
 * Return is 0 or error code.
 */
int RegisterRequestCompleteHandler(HRilClient client, uint32_t id, RilOnComplete handler);

/**
 * Register error callback. If handler is NULL,
 * the callback is unregistered.
 * The response handler is invoked in the client task context.
 * Return is 0 or error code.
 */
int RegisterErrorCallback(HRilClient client, RilOnError cb, void *data);

/**
 * Invoke OEM request. Request ID is RIL_REQUEST_OEM_HOOK_RAW.
 * Return is 0 or error code. For RIL_CLIENT_ERR_AGAIN caller should retry.
 */
int InvokeOemRequestHookRaw(HRilClient client, char *data, size_t len);

/**
 * Sound device types.
 */
typedef enum _SoundType {
    SOUND_TYPE_VOICE,
    SOUND_TYPE_SPEAKER,
    SOUND_TYPE_HEADSET,
    SOUND_TYPE_BTVOICE
} SoundType;

/**
 * External sound device path.
 */
typedef enum _AudioPath {
    SOUND_AUDIO_PATH_EARPIECE,
    SOUND_AUDIO_PATH_HANDSET = SOUND_AUDIO_PATH_EARPIECE,
    SOUND_AUDIO_PATH_HEADSET,
    SOUND_AUDIO_PATH_SPEAKER,
    SOUND_AUDIO_PATH_BLUETOOTH,
    SOUND_AUDIO_PATH_STEREO_BT,
    SOUND_AUDIO_PATH_HEADPHONE,
    SOUND_AUDIO_PATH_BLUETOOTH_NO_NR,
    SOUND_AUDIO_PATH_MIC1,
    SOUND_AUDIO_PATH_MIC2,
    SOUND_AUDIO_PATH_BLUETOOTH_WB,
    SOUND_AUDIO_PATH_BLUETOOTH_WB_NO_NR
} AudioPath;

/**
 * ExtraVolume
 */
#ifdef RIL_CALL_AUDIO_PATH_EXTRAVOLUME
typedef enum _ExtraVolume {
    ORIGINAL_PATH,
    EXTRA_VOLUME_PATH
} ExtraVolume;
#endif

/**
 * Clock adjustment parameters.
 */
typedef enum _SoundClockCondition {
    SOUND_CLOCK_STOP,
    SOUND_CLOCK_START
} SoundClockCondition;

/**
 * Call record adjustment parameters.
 */
typedef enum _CallRecCondition {
    CALL_REC_STOP,
    CALL_REC_START
} CallRecCondition;

/**
 * Mute adjustment parameters.
 */
typedef enum _MuteCondition {
      TX_UNMUTE, /* 0x00: TX UnMute */
      TX_MUTE,   /* 0x01: TX Mute */
      RX_UNMUTE, /* 0x02: RX UnMute */
      RX_MUTE,   /* 0x03: RX Mute */
      RXTX_UNMUTE, /* 0x04: RXTX UnMute */
      RXTX_MUTE,   /* 0x05: RXTX Mute */  
} MuteCondition;

/**
 * Two mic Solution control
 * Two MIC Solution Device
 */
typedef enum __TwoMicSolDevice {
    AUDIENCE,
    FORTEMEDIA
} TwoMicSolDevice;

/**
 * Two MIC Solution Report
 */
typedef enum __TwoMicSolReport {
    TWO_MIC_SOLUTION_OFF,
    TWO_MIC_SOLUTION_ON
} TwoMicSolReport;

/**
 * DHA Mode
 */
typedef enum __DhaSolMode {
    DHA_MODE_OFF,
    DHA_MODE_ON
} DhaSolMode;

/**
 * DHA Select
 */
typedef enum __DhaSolSelect {
    DHA_SEL_LEFT,
    DHA_SEL_RIGHT
} DhaSolSelect;

/**
 * LoopbackTest parameters.
 */
typedef enum __LoopbackMode {
    LOOPBACK_END,
    LOOPBACK_ON_PCM,
    LOOPBACK_ON_PACKET
} LoopbackMode;

typedef enum __LoopbackPath {
    RECEIVER,
    EARPHONE,
    LOUDSPEAKER
} LoopbackPath;


/**
 * Set in-call volume.
 */
int SetCallVolume(HRilClient client, SoundType type, int vol_level);

/**
 * Set external sound device path for noise reduction.
 */
#ifdef RIL_CALL_AUDIO_PATH_EXTRAVOLUME
int SetCallAudioPath(HRilClient client, AudioPath path, ExtraVolume mode);
#else
int SetCallAudioPath(HRilClient client, AudioPath path);
#endif

/**
 * Set modem clock to master or slave.
 */
int SetCallClockSync(HRilClient client, SoundClockCondition condition);

/**
 * Set modem vtcall clock to master or slave.
 */
int SetVideoCallClockSync(HRilClient client, SoundClockCondition condition);

/**
 * Set voice call record
 */
int SetCallRecord(HRilClient client, CallRecCondition condition);

/**
 * Set mute or unmute
 */
int SetMute(HRilClient client, MuteCondition condition);

/**
 * Get mute state
 */
int GetMute(HRilClient client, RilOnComplete handler);

int SetTwoMicControl(HRilClient client, TwoMicSolDevice device, TwoMicSolReport report);

/**
 * DHA Solution Set
 */
int SetDhaSolution(HRilClient client, DhaSolMode mode, DhaSolSelect select, char *parameter);

/**
 * Set Loopback Test Mode and Path
 */
int SetLoopbackTest(HRilClient client, LoopbackMode mode, AudioPath path);

#ifdef __cplusplus
};
#endif

#endif // __SECRIL_CLIENT_H__

// end of file

