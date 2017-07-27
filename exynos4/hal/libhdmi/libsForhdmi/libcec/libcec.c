/*
* Copyright@ Samsung Electronics Co. LTD
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <cutils/log.h>

/* drv. header */
#include "cec.h"

#include "libcec.h"

#define CEC_DEBUG 0

/**
 * @def CEC_DEVICE_NAME
 * Defines simbolic name of the CEC device.
 */
#define CEC_DEVICE_NAME         "/dev/CEC"

static struct {
    enum CECDeviceType devtype;
    unsigned char laddr;
} laddresses[] = {
    { CEC_DEVICE_RECODER, 1  },
    { CEC_DEVICE_RECODER, 2  },
    { CEC_DEVICE_TUNER,   3  },
    { CEC_DEVICE_PLAYER,  4  },
    { CEC_DEVICE_AUDIO,   5  },
    { CEC_DEVICE_TUNER,   6  },
    { CEC_DEVICE_TUNER,   7  },
    { CEC_DEVICE_PLAYER,  8  },
    { CEC_DEVICE_RECODER, 9  },
    { CEC_DEVICE_TUNER,   10 },
    { CEC_DEVICE_PLAYER,  11 },
};

static int CECSetLogicalAddr(unsigned int laddr);

#ifdef CEC_DEBUG
inline static void CECPrintFrame(unsigned char *buffer, unsigned int size);
#endif

static int fd = -1;

/**
 * Open device driver and assign CEC file descriptor.
 *
 * @return  If success to assign CEC file descriptor, return 1; otherwise, return 0.
 */
int CECOpen()
{
    int res = 1;

    if (fd != -1)
        CECClose();

    if ((fd = open(CEC_DEVICE_NAME, O_RDWR)) < 0) {
        ALOGE("Can't open %s!\n", CEC_DEVICE_NAME);
        res = 0;
    }

    return res;
}

/**
 * Close CEC file descriptor.
 *
 * @return  If success to close CEC file descriptor, return 1; otherwise, return 0.
 */
int CECClose()
{
    int res = 1;

    if (fd != -1) {
        if (close(fd) != 0) {
            ALOGE("close() failed!\n");
            res = 0;
        }
        fd = -1;
    }

    return res;
}

/**
 * Allocate logical address.
 *
 * @param paddr   [in] CEC device physical address.
 * @param devtype [in] CEC device type.
 *
 * @return new logical address, or 0 if an arror occured.
 */
int CECAllocLogicalAddress(int paddr, enum CECDeviceType devtype)
{
    unsigned char laddr = CEC_LADDR_UNREGISTERED;
    int i = 0;

    if (fd == -1) {
        ALOGE("open device first!\n");
        return 0;
    }

    if (CECSetLogicalAddr(laddr) < 0) {
        ALOGE("CECSetLogicalAddr() failed!\n");
        return 0;
    }

    if (paddr == CEC_NOT_VALID_PHYSICAL_ADDRESS)
        return CEC_LADDR_UNREGISTERED;

    /* send "Polling Message" */
    while (i < sizeof(laddresses)/sizeof(laddresses[0])) {
        if (laddresses[i].devtype == devtype) {
            unsigned char _laddr = laddresses[i].laddr;
            unsigned char message = ((_laddr << 4) | _laddr);
            if (CECSendMessage(&message, 1) != 1) {
                laddr = _laddr;
                break;
            }
        }
        i++;
    }

    if (laddr == CEC_LADDR_UNREGISTERED) {
        ALOGE("All LA addresses in use!!!\n");
        return CEC_LADDR_UNREGISTERED;
    }

    if (CECSetLogicalAddr(laddr) < 0) {
        ALOGE("CECSetLogicalAddr() failed!\n");
        return 0;
    }

    /* broadcast "Report Physical Address" */
    unsigned char buffer[5];
    buffer[0] = (laddr << 4) | CEC_MSG_BROADCAST;
    buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
    buffer[2] = (paddr >> 8) & 0xFF;
    buffer[3] = paddr & 0xFF;
    buffer[4] = devtype;

    if (CECSendMessage(buffer, 5) != 5) {
        ALOGE("CECSendMessage() failed!\n");
        return 0;
    }

    return laddr;
}

/**
 * Send CEC message.
 *
 * @param *buffer   [in] pointer to buffer address where message located.
 * @param size      [in] message size.
 *
 * @return number of bytes written, or 0 if an arror occured.
 */
int CECSendMessage(unsigned char *buffer, int size)
{
    if (fd == -1) {
        ALOGE("open device first!\n");
        return 0;
    }

    if (size > CEC_MAX_FRAME_SIZE) {
        ALOGE("size should not exceed %d\n", CEC_MAX_FRAME_SIZE);
        return 0;
    }

#if CEC_DEBUG
    ALOGI("CECSendMessage() : ");
    CECPrintFrame(buffer, size);
#endif

    return write(fd, buffer, size);
}

/**
 * Receive CEC message.
 *
 * @param *buffer   [in] pointer to buffer address where message will be stored.
 * @param size      [in] buffer size.
 * @param timeout   [in] timeout in microseconds.
 *
 * @return number of bytes received, or 0 if an arror occured.
 */
int CECReceiveMessage(unsigned char *buffer, int size, long timeout)
{
    int bytes = 0;
    fd_set rfds;
    struct timeval tv;
    int retval;

    if (fd == -1) {
        ALOGE("open device first!\n");
        return 0;
    }

    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    retval = select(fd + 1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        return 0;
    } else if (retval) {
        bytes = read(fd, buffer, size);
#if CEC_DEBUG
        ALOGI("CECReceiveMessage() : size(%d)", bytes);
        if(bytes > 0)
            CECPrintFrame(buffer, bytes);
#endif
    }

    return bytes;
}

/**
 * Set CEC logical address.
 *
 * @return 1 if success, otherwise, return 0.
 */
int CECSetLogicalAddr(unsigned int laddr)
{
    if (ioctl(fd, CEC_IOC_SETLADDR, &laddr)) {
        ALOGE("ioctl(CEC_IOC_SETLA) failed!\n");
        return 0;
    }

    return 1;
}

#if CEC_DEBUG
/**
 * Print CEC frame.
 */
void CECPrintFrame(unsigned char *buffer, unsigned int size)
{
    if (size > 0) {
        int i;
        ALOGI("fsize: %d ", size);
        ALOGI("frame: ");
        for (i = 0; i < size; i++)
            ALOGI("0x%02x ", buffer[i]);

        ALOGI("\n");
    }
}
#endif

/**
 * Check CEC message.
 *
 * @param opcode   [in] pointer to buffer address where message will be stored.
 * @param lsrc     [in] buffer size.
 *
 * @return 1 if message should be ignored, otherwise, return 0.
 */
//TODO: not finished
int CECIgnoreMessage(unsigned char opcode, unsigned char lsrc)
{
    int retval = 0;

    /* if a message coming from address 15 (unregistered) */
    if (lsrc == CEC_LADDR_UNREGISTERED) {
        switch (opcode) {
        case CEC_OPCODE_DECK_CONTROL:
        case CEC_OPCODE_PLAY:
            retval = 1;
        default:
            break;
        }
    }

    return retval;
}

/**
 * Check CEC message.
 *
 * @param opcode   [in] pointer to buffer address where message will be stored.
 * @param size     [in] message size.
 *
 * @return 0 if message should be ignored, otherwise, return 1.
 */
//TODO: not finished
int CECCheckMessageSize(unsigned char opcode, int size)
{
    int retval = 1;

    switch (opcode) {
    case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
        if (size != 1)
            retval = 0;
        break;
    case CEC_OPCODE_SET_SYSTEM_AUDIO_MODE:
        if (size != 2)
            retval = 0;
        break;
    case CEC_OPCODE_PLAY:
    case CEC_OPCODE_DECK_CONTROL:
    case CEC_OPCODE_SET_MENU_LANGUAGE:
    case CEC_OPCODE_ACTIVE_SOURCE:
    case CEC_OPCODE_ROUTING_INFORMATION:
    case CEC_OPCODE_SET_STREAM_PATH:
        if (size != 3)
            retval = 0;
        break;
    case CEC_OPCODE_FEATURE_ABORT:
    case CEC_OPCODE_DEVICE_VENDOR_ID:
    case CEC_OPCODE_REPORT_PHYSICAL_ADDRESS:
        if (size != 4)
            retval = 0;
        break;
    case CEC_OPCODE_ROUTING_CHANGE:
        if (size != 5)
            retval = 0;
        break;
    /* CDC - 1.4 */
    case 0xf8:
        if (!(size > 5 && size <= 16))
            retval = 0;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * Check CEC message.
 *
 * @param opcode    [in] pointer to buffer address where message will be stored.
 * @param broadcast [in] broadcast/direct message.
 *
 * @return 0 if message should be ignored, otherwise, return 1.
 */
//TODO: not finished
int CECCheckMessageMode(unsigned char opcode, int broadcast)
{
    int retval = 1;

    switch (opcode) {
    case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
    case CEC_OPCODE_SET_MENU_LANGUAGE:
    case CEC_OPCODE_ACTIVE_SOURCE:
        if (!broadcast)
            retval = 0;
        break;
    case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
    case CEC_OPCODE_DECK_CONTROL:
    case CEC_OPCODE_PLAY:
    case CEC_OPCODE_FEATURE_ABORT:
    case CEC_OPCODE_ABORT:
        if (broadcast)
            retval = 0;
        break;
    default:
        break;
    }

    return retval;
}
