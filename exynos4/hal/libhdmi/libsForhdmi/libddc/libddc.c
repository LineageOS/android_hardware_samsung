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
#include <string.h>

#include <linux/i2c.h>
#include <cutils/log.h>
#include "i2c-dev.h"

#include "libddc.h"

#define DDC_DEBUG 0

/**
 * @brief DDC device name.
 * User should change this.
 */
#ifdef DDC_CH_I2C_1
#define DEV_NAME    "/dev/i2c-1"
#endif

#ifdef DDC_CH_I2C_2
#define DEV_NAME    "/dev/i2c-2"
#endif

#ifdef DDC_CH_I2C_7
#define DEV_NAME    "/dev/i2c-7"
#endif

/**
 * DDC file descriptor
 */
static int ddc_fd = -1;

/**
 * Reference count of DDC file descriptor
 */
static unsigned int ref_cnt = 0;

/**
 * Check if DDC file is already opened or not
 * @return  If DDC file is already opened, return 1; Otherwise, return 0.
 */
static int DDCFileAvailable()
{
    return (ddc_fd < 0) ? 0 : 1;
}

/**
 * Initialze DDC library. Open DDC device
 * @return  If succeed in opening DDC device or it is already opened, return 1;@n
 *         Otherwise, return 0.
 */
int DDCOpen()
{
    int ret = 1;

    // check already open??
    if (ref_cnt > 0) {
        ref_cnt++;
        return 1;
    }

    // open
    if ((ddc_fd = open(DEV_NAME,O_RDWR)) < 0) {
        ALOGE("%s: Cannot open I2C_DDC : %s",__func__, DEV_NAME);
        ret = 0;
    }

    ref_cnt++;
    return ret;
}

/**
 * Finalize DDC library. Close DDC device
 * @return  If succeed in closing DDC device or it is being used yet, return 1;@n
 *          Otherwise, return 0.
 */
int DDCClose()
{
    int ret = 1;
    // check if fd is available
    if (ref_cnt == 0) {
#if DDC_DEBUG
        ALOGE("%s: I2C_DDC is not available!!!!", __func__);
#endif
        return 1;
    }

    // close
    if (ref_cnt > 1) {
        ref_cnt--;
        return 1;
    }

    if (close(ddc_fd) < 0) {
#if DDC_DEBUG
        ALOGE("%s: Cannot close I2C_DDC : %s",__func__,DEV_NAME);
#endif
        ret = 0;
    }

    ref_cnt--;
    ddc_fd = -1;

    return ret;
}

/**
 * Read data though DDC. For more information of DDC, refer DDC Spec.
 * @param   addr    [in]    Device address
 * @param   offset  [in]    Byte offset
 * @param   size    [in]    Sizes of data
 * @param   buffer  [out]   Pointer to buffer to store data
 * @return  If succeed in reading, return 1; Otherwise, return 0.
 */
int DDCRead(unsigned char addr, unsigned char offset,
            unsigned int size, unsigned char* buffer)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg msgs[2];
    int ret = 1;

    if (!DDCFileAvailable()) {
#if DDC_DEBUG
        ALOGE("%s: I2C_DDC is not available!!!!", __func__);
#endif
        return 0;
    }

    // set offset
    msgs[0].addr = addr>>1;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &offset;

    // read data
    msgs[1].addr = addr>>1;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = size;
    msgs[1].buf = buffer;

    // set rdwr ioctl data
    msgset.nmsgs = 2;
    msgset.msgs = msgs;

    // i2c fast read
    if ((ret = ioctl(ddc_fd, I2C_RDWR, &msgset)) < 0) {
        perror("ddc error:");
        ret = 0;
    }

    return ret;
}

/**
 * Read data though E-DDC. For more information of E-DDC, refer E-DDC Spec.
 * @param   segpointer  [in]    Segment pointer
 * @param   segment     [in]    Segment number
 * @param   addr        [in]    Device address
 * @param   offset      [in]    Byte offset
 * @param   size        [in]    Sizes of data
 * @param   buffer      [out]   Pointer to buffer to store data
 * @return  If succeed in reading, return 1; Otherwise, return 0.
 */

int EDDCRead(unsigned char segpointer, unsigned char segment, unsigned char addr,
  unsigned char offset, unsigned int size, unsigned char* buffer)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg msgs[3];
    int ret = 1;

    if (!DDCFileAvailable()) {
#if DDC_DEBUG
        ALOGE("%s: I2C_DDC is not available!!!!", __func__);
#endif
        return 0;
    }

    // set segment pointer
    msgs[0].addr  = segpointer>>1;
    // ignore ack only if segment is "0"
    if (segment == 0)
        msgs[0].flags = I2C_M_IGNORE_NAK;
    else
        msgs[0].flags = 0;

    msgs[0].len   = 1;
    msgs[0].buf   = &segment;

    // set offset
    msgs[1].addr  = addr>>1;
    msgs[1].flags = 0;
    msgs[1].len   = 1;
    msgs[1].buf   = &offset;

    // read data
    msgs[2].addr  = addr>>1;
    msgs[2].flags = I2C_M_RD;
    msgs[2].len   = size;
    msgs[2].buf   = buffer;

    msgset.nmsgs = 3;
    msgset.msgs  = msgs;

    // eddc read
    if (ioctl(ddc_fd, I2C_RDWR, &msgset) < 0) {
#if DDC_DEBUG
        ALOGE("%s: ioctl(I2C_RDWR) failed!!!", __func__);
#endif
        ret = 0;
    }
    return ret;
}

/**
 * Write data though DDC. For more information of DDC, refer DDC Spec.
 * @param   addr    [in]    Device address
 * @param   offset  [in]    Byte offset
 * @param   size    [in]    Sizes of data
 * @param   buffer  [out]   Pointer to buffer to write
 * @return  If succeed in writing, return 1; Otherwise, return 0.
 */
int DDCWrite(unsigned char addr, unsigned char offset, unsigned int size, unsigned char* buffer)
{
    unsigned char* temp;
    int bytes;
    int retval = 0;

    // allocate temporary buffer
    temp = (unsigned char*) malloc((size+1)*sizeof(unsigned char));
    if (!temp) {
        ALOGE("%s: not enough resources at %s", __FUNCTION__);
        goto exit;
    }

    temp[0] = offset;
    memcpy(temp+1,buffer,size);

    if (!DDCFileAvailable()) {
        ALOGE("%s: I2C_DDC is not available!!!!", __func__);
        goto exit;
    }

    if (ioctl(ddc_fd, I2C_SLAVE, addr>>1) < 0) {
        ALOGE("%s: cannot set slave address 0x%02x", __func__,addr);
        goto exit;
    }

    // write temp buffer
    if ((bytes = write(ddc_fd,temp,size+1)) != (size+1)) {
        ALOGE("%s: fail to write %d bytes, only write %d bytes",__func__, size, bytes);
        goto exit;
    }

    retval = 1;

exit:
    // free temp buffer
    if (temp)
        free(temp);

    return retval;
}
