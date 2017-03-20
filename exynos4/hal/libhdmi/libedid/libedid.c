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
#include <string.h>
#include <stdlib.h>

#include <cutils/log.h>

#include "edid.h"
#include "libedid.h"
#include "../libddc/libddc.h"

//#define EDID_DEBUG 1

#ifdef EDID_DEBUG
#define DPRINTF(args...)    ALOGI(args)
#else
#define DPRINTF(args...)
#endif

#define NUM_OF_VIC_FOR_3D           16

/**
 * @var gEdidData
 * Pointer to EDID data
 */
static unsigned char* gEdidData;

/**
 * @var gExtensions
 * Number of EDID extensions
 */
static int gExtensions;


/**
 * @var aVIC
 * This contains first 16 VIC in EDID
 */
static unsigned char aVIC[NUM_OF_VIC_FOR_3D];

//! Structure for parsing video timing parameter in EDID
static const struct edid_params {
    /** H Total */
    unsigned int HTotal;

    /** H Blank */
    unsigned int HBlank;

    /** V Total */
    unsigned int VTotal;

    /** V Blank */
    unsigned int VBlank;

    /** CEA VIC */
    unsigned char  VIC;

    /** CEA VIC for 16:9 aspect ratio */
    unsigned char  VIC16_9;

    /** 0 if progressive, 1 if interlaced */
    unsigned char  interlaced;

    /** Pixel frequency */
    enum PixelFreq PixelClock;
} aVideoParams[] =
{
    { 800 , 160 , 525 , 45, 1 , 1 , 0, PIXEL_FREQ_25_200 ,},        // v640x480p_60Hz
    { 858 , 138 , 525 , 45, 2 , 3 , 0, PIXEL_FREQ_27_027 ,},        // v720x480p_60Hz
    { 1650, 370 , 750 , 30, 4 , 4 , 0, PIXEL_FREQ_74_250 ,},        // v1280x720p_60Hz
    { 2200, 280 , 1125, 22, 5 , 5 , 1, PIXEL_FREQ_74_250 ,},        // v1920x1080i_60H
    { 1716, 276 , 525 , 22, 6 , 7 , 1, PIXEL_FREQ_74_250 ,},        // v720x480i_60Hz
    { 1716, 276 , 262 , 22, 8 , 9 , 0, PIXEL_FREQ_27_027 ,},        // v720x240p_60Hz
    //{ 1716, 276 , 263 , 23, 8 , 9 , 0, PIXEL_FREQ_27_027 , },     // v720x240p_60Hz(mode 2)
    { 3432, 552 , 525 , 22, 10, 11, 1, PIXEL_FREQ_54_054 , },       // v2880x480i_60Hz
    { 3432, 552 , 262 , 22, 12, 13, 0, PIXEL_FREQ_54_054 , },       // v2880x240p_60Hz
    //{ 3432, 552 , 263 , 23, 12, 13, 0, PIXEL_FREQ_54_054 , },     // v2880x240p_60Hz(mode 2)
    { 1716, 276 , 525 , 45, 14, 15, 0, PIXEL_FREQ_54_054 , },       // v1440x480p_60Hz
    { 2200, 280 , 1125, 45, 16, 16, 0, PIXEL_FREQ_148_500, },       // v1920x1080p_60H
    { 864 , 144 , 625 , 49, 17, 18, 0, PIXEL_FREQ_27 , },           // v720x576p_50Hz
    { 1980, 700 , 750 , 30, 19, 19, 0, PIXEL_FREQ_74_250 , },       // v1280x720p_50Hz
    { 2640, 720 , 1125, 22, 20, 20, 1, PIXEL_FREQ_74_250 , },       // v1920x1080i_50H
    { 1728, 288 , 625 , 24, 21, 22, 1, PIXEL_FREQ_27 , },           // v720x576i_50Hz
    { 1728, 288 , 312 , 24, 23, 24, 0, PIXEL_FREQ_27 , },           // v720x288p_50Hz
    //{ 1728, 288 , 313 , 25, 23, 24, 0, PIXEL_FREQ_27 , },         // v720x288p_50Hz(mode 2)
    //{ 1728, 288 , 314 , 26, 23, 24, 0, PIXEL_FREQ_27 , },         // v720x288p_50Hz(mode 3)
    { 3456, 576 , 625 , 24, 25, 26, 1, PIXEL_FREQ_54 , },           // v2880x576i_50Hz
    { 3456, 576 , 312 , 24, 27, 28, 0, PIXEL_FREQ_54 , },           // v2880x288p_50Hz
    //{ 3456, 576 , 313 , 25, 27, 28, 0, PIXEL_FREQ_54 , },         // v2880x288p_50Hz(mode 2)
    //{ 3456, 576 , 314 , 26, 27, 28, 0, PIXEL_FREQ_54 , },         // v2880x288p_50Hz(mode 3)
    { 1728, 288 , 625 , 49, 29, 30, 0, PIXEL_FREQ_54 , },           // v1440x576p_50Hz
    { 2640, 720 , 1125, 45, 31, 31, 0, PIXEL_FREQ_148_500,},        // v1920x1080p_50Hz
    { 2750, 830 , 1125, 45, 32, 32, 0, PIXEL_FREQ_74_250 ,},        // v1920x1080p_24Hz
    { 2640, 720 , 1125, 45, 33, 33, 0, PIXEL_FREQ_74_250 ,},        // v1920x1080p_25Hz
    { 2200, 280 , 1125, 45, 34, 34, 0, PIXEL_FREQ_74_250 ,},        // v1920x1080p_30Hz
    { 3432, 552 , 525 , 45, 35, 36, 0, PIXEL_FREQ_108_108,},        // v2880x480p_60Hz
    { 3456, 576 , 625 , 49, 37, 38, 0, PIXEL_FREQ_108 ,},           // v2880x576p_50Hz
    { 2304, 384 , 1250, 85, 39, 39, 1, PIXEL_FREQ_72 ,},            // v1920x1080i_50Hz(1250)
    { 2640, 720 , 1125, 22, 40, 40, 1, PIXEL_FREQ_148_500, },       // v1920x1080i_100Hz
    { 1980, 700 , 750 , 30, 41, 41, 0, PIXEL_FREQ_148_500, },       // v1280x720p_100Hz
    { 864 , 144 , 625 , 49, 42, 43, 0, PIXEL_FREQ_54 , },           // v720x576p_100Hz
    { 1728, 288 , 625 , 24, 44, 45, 1, PIXEL_FREQ_54 , },           // v720x576i_100Hz
    { 2200, 280 , 1125, 22, 46, 46, 1, PIXEL_FREQ_148_500, },       // v1920x1080i_120Hz
    { 1650, 370 , 750 , 30, 47, 47, 0, PIXEL_FREQ_148_500, },       // v1280x720p_120Hz
    { 858 , 138 , 525 , 54, 48, 49, 0, PIXEL_FREQ_54_054 , },       // v720x480p_120Hz
    { 1716, 276 , 525 , 22, 50, 51, 1, PIXEL_FREQ_54_054 , },       // v720x480i_120Hz
    { 864 , 144 , 625 , 49, 52, 53, 0, PIXEL_FREQ_108 , },          // v720x576p_200Hz
    { 1728, 288 , 625 , 24, 54, 55, 1, PIXEL_FREQ_108 , },          // v720x576i_200Hz
    { 858 , 138 , 525 , 45, 56, 57, 0, PIXEL_FREQ_108_108, },       // v720x480p_240Hz
    { 1716, 276 , 525 , 22, 58, 59, 1, PIXEL_FREQ_108_108, },       // v720x480i_240Hz
    // PHY Freq is not available yet
    //{ 3300, 2020, 750 , 30, 60, 60, 0, PIXEL_FREQ_59_400 ,},      // v1280x720p24Hz
    { 3960, 2680, 750 , 30, 61, 61, 0, PIXEL_FREQ_74_250 , },       // v1280x720p25Hz
    { 3300, 2020, 750 , 30, 62, 62, 0, PIXEL_FREQ_74_250 ,},        // v1280x720p30Hz
    // PHY Freq is not available yet
    //{ 2200, 280 , 1125, 45, 63, 63, 0, PIXEL_FREQ_297, },         // v1920x1080p120Hz
    //{ 2640, 720 , 1125, 45, 64, 64, 0, PIXEL_FREQ_297, },         // v1920x1080p100Hz
    //{ 4400, 560 , 2250, 90,  1, 1, 0, 0,  PIXEL_FREQ_297, },      // v4Kx2K30Hz
};

//! Structure for Checking 3D Mandatory Format in EDID
static const struct edid_3d_mandatory {
    /** video Format */
    enum VideoFormat resolution;

    /** 3D Structure */
    enum HDMI3DVideoStructure hdmi_3d_format;
} edid_3d [] =
{
    { v1920x1080p_24Hz, HDMI_3D_FP_FORMAT },    // 1920x1080p @ 23.98/24Hz
    { v1280x720p_60Hz, HDMI_3D_FP_FORMAT },     // 1280x720p @ 59.94/60Hz
    { v1920x1080i_60Hz, HDMI_3D_SSH_FORMAT },   // 1920x1080i @ 59.94/60Hz
    { v1920x1080p_24Hz, HDMI_3D_TB_FORMAT },    // 1920x1080p @ 23.98/24Hz
    { v1280x720p_60Hz, HDMI_3D_TB_FORMAT },     // 1280x720p @ 59.94/60Hz
    { v1280x720p_50Hz, HDMI_3D_FP_FORMAT },     // 1280x720p @ 50Hz
    { v1920x1080i_50Hz, HDMI_3D_SSH_FORMAT },   // 1920x1080i @ 50Hz
    { v1280x720p_50Hz, HDMI_3D_TB_FORMAT },     // 1280x720p @ 50Hz
};

/**
 * Calculate a checksum.
 *
 * @param   buffer  [in]    Pointer to data to calculate a checksum
 * @param   size    [in]    Sizes of data
 *
 * @return  If checksum result is 0, return 1; Otherwise, return 0.
 */
static int CalcChecksum(const unsigned char* const buffer, const int size)
{
    unsigned char i,sum;
    int ret = 1;

    // check parameter
    if (buffer == NULL ) {
        DPRINTF("invalid parameter : buffer\n");
        return 0;
    }
    for (sum = 0, i = 0 ; i < size; i++)
        sum += buffer[i];

    // check checksum
    if (sum != 0)
        ret = 0;

    return ret;
}

/**
 * Read EDID Block(128 bytes)
 *
 * @param   blockNum    [in]    Number of block to read @n
 *                  For example, EDID block = 0, EDID first Extension = 1, and so on.
 * @param   outBuffer   [out]   Pointer to buffer to store EDID data
 *
 * @return  If fail to read, return 0; Otherwise, return 1.
 */
static int ReadEDIDBlock(const unsigned int blockNum, unsigned char* const outBuffer)
{
    int segNum, offset, dataPtr;

    // check parameter
    if (outBuffer == NULL) {
        DPRINTF("invalid parameter : outBuffer\n");
        return 0;
    }

    // calculate
    segNum = blockNum / 2;
    offset = (blockNum % 2) * SIZEOFEDIDBLOCK;
    dataPtr = (blockNum) * SIZEOFEDIDBLOCK;

    // read block
    if (!EDDCRead(EDID_SEGMENT_POINTER, segNum, EDID_ADDR, offset, SIZEOFEDIDBLOCK, outBuffer)) {
        DPRINTF("Fail to Read %dth EDID Block\n", blockNum);
        return 0;
    }

    if (!CalcChecksum(outBuffer, SIZEOFEDIDBLOCK)) {
        DPRINTF("CheckSum fail : %dth EDID Block\n", blockNum);
        return 0;
    }

    // print data
#ifdef EDID_DEBUG
    offset = 0;
    do {
        ALOGI("0x%02X", outBuffer[offset++]);
        if (offset % 16)
            ALOGI(" ");
        else
            ALOGI("\n");
    } while (SIZEOFEDIDBLOCK > offset);
#endif // EDID_DEBUG
    return 1;
}

/**
 * Check if EDID data is valid or not.
 *
 * @return  if EDID data is valid, return 1; Otherwise, return 0.
 */
static inline int EDIDValid(void)
{
    return (gEdidData == NULL) ?  0 : 1;
}

/**
 * Search HDMI Vender Specific Data Block(VSDB) in EDID extension block.
 *
 * @param   extension   [in]    the number of EDID extension block to check
 *
 * @return  if there is a HDMI VSDB, return the offset from start of @n
 *        EDID extension block. if there is no VSDB, return 0.
 */
static int GetVSDBOffset(const int extension)
{
    unsigned int BlockOffset = extension*SIZEOFEDIDBLOCK;
    unsigned int offset = BlockOffset + EDID_DATA_BLOCK_START_POS;
    unsigned int tag,blockLen,DTDOffset;

    if (!EDIDValid() || (extension > gExtensions)) {
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    DTDOffset = gEdidData[BlockOffset + EDID_DETAILED_TIMING_OFFSET_POS];

    // check if there is HDMI VSDB
    while (offset < BlockOffset + DTDOffset) {
        // find the block tag and length
        // tag
        tag = gEdidData[offset] & EDID_TAG_CODE_MASK;
        // block len
        blockLen = (gEdidData[offset] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

        // check if it is HDMI VSDB
        // if so, check identifier value, if it's hdmi vsbd - return offset
        if (tag == EDID_VSDB_TAG_VAL &&
            gEdidData[offset+1] == 0x03 &&
            gEdidData[offset+2] == 0x0C &&
            gEdidData[offset+3] == 0x0 &&
            blockLen > EDID_VSDB_MIN_LENGTH_VAL )
            return offset;

        // else find next block
        offset += blockLen;
    }

    // return error
    return 0;
}

/**
 * Check if Sink supports the HDMI mode.
 * @return  If Sink supports HDMI mode, return 1; Otherwise, return 0.
 */
static int CheckHDMIMode(void)
{
    int i;

    // read EDID
    if (!EDIDRead())
        return 0;

    // find VSDB
    for (i = 1; i <= gExtensions; i++)
        if (GetVSDBOffset(i) > 0) // if there is a VSDB, it means RX support HDMI mode
            return 1;

    return 0;
}

/**
 * Check if EDID extension block is timing extension block or not.
 * @param   extension   [in] The number of EDID extension block to check
 * @return  If the block is timing extension, return 1; Otherwise, return 0.
 */
static int IsTimingExtension(const int extension)
{
    int ret = 0;
    if (!EDIDValid() || (extension > gExtensions)) {
        DPRINTF("EDID Data is not available\n");
        return ret;
    }

    if (gEdidData[extension*SIZEOFEDIDBLOCK] == EDID_TIMING_EXT_TAG_VAL) {
        // check extension revsion number
        // revision num == 3
        if (gEdidData[extension*SIZEOFEDIDBLOCK + EDID_TIMING_EXT_REV_NUMBER_POS] == 3)
            ret = 1;
        // revison num != 3 && DVI mode
        else if (!CheckHDMIMode() &&
                gEdidData[extension*SIZEOFEDIDBLOCK + EDID_TIMING_EXT_REV_NUMBER_POS] != 2)
            ret = 1;
    }
    return ret;
}

/**
 * Check if the video format is contained in - @n
 * Detailed Timing Descriptor(DTD) of EDID extension block.
 * @param   extension   [in]    Number of EDID extension block to check
 * @param   videoFormat [in]    Video format to check
 * @return  If the video format is contained in DTD of EDID extension block, -@n
 *        return 1; Otherwise, return 0.
 */
static int IsContainVideoDTD(const int extension,const enum VideoFormat videoFormat)
{
    int i, StartOffset, EndOffset;

    if (!EDIDValid() || (extension > gExtensions)) {
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    // if edid block( 0th block )
    if (extension == 0) {
        StartOffset = EDID_DTD_START_ADDR;
        EndOffset = StartOffset + EDID_DTD_TOTAL_LENGTH;
    } else { // if edid extension block
        StartOffset = extension*SIZEOFEDIDBLOCK + gEdidData[extension*SIZEOFEDIDBLOCK + EDID_DETAILED_TIMING_OFFSET_POS];
        EndOffset = (extension+1)*SIZEOFEDIDBLOCK;
    }

    // check DTD(Detailed Timing Description)
    for (i = StartOffset; i < EndOffset; i+= EDID_DTD_BYTE_LENGTH) {
        unsigned int hblank = 0, hactive = 0, vblank = 0, vactive = 0, interlaced = 0, pixelclock = 0;
        unsigned int vHActive = 0, vVActive = 0, vVBlank = 0;

        // get pixel clock
        pixelclock = (gEdidData[i+EDID_DTD_PIXELCLOCK_POS2] << SIZEOFBYTE);
        pixelclock |= gEdidData[i+EDID_DTD_PIXELCLOCK_POS1];

        if (!pixelclock)
            continue;

        // get HBLANK value in pixels
        hblank = gEdidData[i+EDID_DTD_HBLANK_POS2] & EDID_DTD_HBLANK_POS2_MASK;
        hblank <<= SIZEOFBYTE; // lower 4 bits
        hblank |= gEdidData[i+EDID_DTD_HBLANK_POS1];

        // get HACTIVE value in pixels
        hactive = gEdidData[i+EDID_DTD_HACTIVE_POS2] & EDID_DTD_HACTIVE_POS2_MASK;
        hactive <<= (SIZEOFBYTE/2); // upper 4 bits
        hactive |= gEdidData[i+EDID_DTD_HACTIVE_POS1];

        // get VBLANK value in pixels
        vblank = gEdidData[i+EDID_DTD_VBLANK_POS2] & EDID_DTD_VBLANK_POS2_MASK;
        vblank <<= SIZEOFBYTE; // lower 4 bits
        vblank |= gEdidData[i+EDID_DTD_VBLANK_POS1];

        // get VACTIVE value in pixels
        vactive = gEdidData[i+EDID_DTD_VACTIVE_POS2] & EDID_DTD_VACTIVE_POS2_MASK;
        vactive <<= (SIZEOFBYTE/2); // upper 4 bits
        vactive |= gEdidData[i+EDID_DTD_VACTIVE_POS1];

        vHActive = aVideoParams[videoFormat].HTotal - aVideoParams[videoFormat].HBlank;
        if (aVideoParams[videoFormat].interlaced == 1) {
            if (aVideoParams[videoFormat].VIC == v1920x1080i_50Hz_1250) { // VTOP and VBOT are same
                vVActive = (aVideoParams[videoFormat].VTotal - aVideoParams[videoFormat].VBlank*2)/2;
                vVBlank = aVideoParams[videoFormat].VBlank;
            } else {
                vVActive = (aVideoParams[videoFormat].VTotal - aVideoParams[videoFormat].VBlank*2 - 1)/2;
                vVBlank = aVideoParams[videoFormat].VBlank;
            }
        } else {
            vVActive = aVideoParams[videoFormat].VTotal - aVideoParams[videoFormat].VBlank;
            vVBlank = aVideoParams[videoFormat].VBlank;
        }

        // get Interlaced Mode Value
        interlaced = (int)(gEdidData[i+EDID_DTD_INTERLACE_POS] & EDID_DTD_INTERLACE_MASK);
        if (interlaced)
            interlaced = 1;

        DPRINTF("EDID: hblank = %d,vblank = %d, hactive = %d, vactive = %d\n"
                            ,hblank,vblank,hactive,vactive);
        DPRINTF("REQ: hblank = %d,vblank = %d, hactive = %d, vactive = %d\n"
                            ,aVideoParams[videoFormat].HBlank
                            ,vVBlank,vHActive,vVActive);

        if (hblank == aVideoParams[videoFormat].HBlank && vblank == vVBlank // blank
            && hactive == vHActive && vactive == vVActive) { //line
            unsigned int EDIDpixelclock = aVideoParams[videoFormat].PixelClock;
            EDIDpixelclock /= 100; pixelclock /= 100;

            if (pixelclock == EDIDpixelclock) {
                DPRINTF("Sink Support the Video mode\n");
                return 1;
            }
        }
    }
    return 0;
}

/**
 * Check if a VIC(Video Identification Code) is contained in -@n
 * EDID extension block.
 * @param   extension   [in]    Number of EDID extension block to check
 * @param   VIC      [in]   VIC to check
 * @return  If the VIC is contained in contained in EDID extension block, -@n
 *        return 1; Otherwise, return 0.
 */
static int IsContainVIC(const int extension, const int VIC)
{
    unsigned int StartAddr = extension*SIZEOFEDIDBLOCK;
    unsigned int ExtAddr = StartAddr + EDID_DATA_BLOCK_START_POS;
    unsigned int tag,blockLen;
    unsigned int DTDStartAddr = gEdidData[StartAddr + EDID_DETAILED_TIMING_OFFSET_POS];

    if (!EDIDValid() || (extension > gExtensions)) {
        DPRINTF("EDID Data is not available\n");
        return 0;
    }

    // while
    while (ExtAddr < StartAddr + DTDStartAddr) {
        // find the block tag and length
        // tag
        tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
        // block len
        blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;
        DPRINTF("tag = %d\n",tag);
        DPRINTF("blockLen = %d\n",blockLen-1);

        // check if it is short video description
        if (tag == EDID_SHORT_VID_DEC_TAG_VAL) {
            // if so, check SVD
            unsigned int i;
            for (i = 1; i < blockLen; i++) {
                DPRINTF("EDIDVIC = %d\n",gEdidData[ExtAddr+i] & EDID_SVD_VIC_MASK);
                DPRINTF("VIC = %d\n",VIC);

                // check VIC with SVDB
                if (VIC == (gEdidData[ExtAddr+i] & EDID_SVD_VIC_MASK)) {
                    DPRINTF("Sink Device supports requested video mode\n");
                    return 1;
                }
            }
        }
        // else find next block
        ExtAddr += blockLen;
    }

    return 0;
}

/**
 * Check if EDID contains the video format.
 * @param   videoFormat [in]    Video format to check
 * @param   pixelRatio  [in]    Pixel aspect ratio of video format to check
 * @return  if EDID contains the video format, return 1; Otherwise, return 0.
 */
static int CheckResolution(const enum VideoFormat videoFormat,
                            const enum PixelAspectRatio pixelRatio)
{
    int i, vic;

    // read EDID
    if (!EDIDRead())
        return 0;

    // check ET(Established Timings) for 640x480p@60Hz
    if (videoFormat == v640x480p_60Hz // if it's 640x480p@60Hz
        && (gEdidData[EDID_ET_POS] & EDID_ET_640x480p_VAL)) // it support
         return 1;

    // check STI(Standard Timing Identification)
    // do not need

    // check DTD(Detailed Timing Description) of EDID block(0th)
    if (IsContainVideoDTD(0,videoFormat))
        return 1;

    // check EDID Extension
    vic = (pixelRatio == HDMI_PIXEL_RATIO_16_9) ?
            aVideoParams[videoFormat].VIC16_9 : aVideoParams[videoFormat].VIC;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i)) // if it's timing block
            if (IsContainVIC(i, vic) || IsContainVideoDTD(i, videoFormat))
                return 1;
    }

    return 0;
}

/**
 * Check if EDID supports the color depth.
 * @param   depth [in]  Color depth
 * @param   space [in]  Color space
 * @return  If EDID supports the color depth, return 1; Otherwise, return 0.
 */
static int CheckColorDepth(const enum ColorDepth depth,const enum ColorSpace space)
{
    int i;
    unsigned int StartAddr;

    // if color depth == 24 bit, no need to check
    if (depth == HDMI_CD_24)
        return 1;

    // check EDID data is valid or not
    // read EDID
    if (!EDIDRead())
        return 0;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i) // if it's timing block
            && ((StartAddr = GetVSDBOffset(i)) > 0)) { // check block
            int blockLength = gEdidData[StartAddr] & EDID_DATA_BLOCK_SIZE_MASK;
            if (blockLength >= EDID_DC_POS) {
                // get supported DC value
                int deepColor = gEdidData[StartAddr + EDID_DC_POS] & EDID_DC_MASK;
                DPRINTF("EDID deepColor = %x\n",deepColor);
                // check supported DeepColor
                // if YCBCR444
                if (space == HDMI_CS_YCBCR444) {
                    if ( !(deepColor & EDID_DC_YCBCR_VAL))
                        return 0;
                }

                // check colorDepth
                switch (depth) {
                case HDMI_CD_36:
                    deepColor &= EDID_DC_36_VAL;
                    break;
                case HDMI_CD_30:
                    deepColor &= EDID_DC_30_VAL;
                    break;
                default :
                    deepColor = 0;
                }
                if (deepColor)
                    return 1;
                else
                    return 0;
            }
        }
    }

    return 0;
}

/**
 * Check if EDID supports the color space.
 * @param   space [in]  Color space
 * @return  If EDID supports the color space, return 1; Otherwise, return 0.
 */
static int CheckColorSpace(const enum ColorSpace space)
{
    int i;

    // RGB is default
    if (space == HDMI_CS_RGB)
        return 1;

    // check EDID data is valid or not
    // read EDID
    if (!EDIDRead())
        return 0;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i)) { // if it's timing block
            // read Color Space
            int CS = gEdidData[i*SIZEOFEDIDBLOCK + EDID_COLOR_SPACE_POS];

            if ((space == HDMI_CS_YCBCR444 && (CS & EDID_YCBCR444_CS_MASK)) || // YCBCR444
                    (space == HDMI_CS_YCBCR422 && (CS & EDID_YCBCR422_CS_MASK))) // YCBCR422
                return 1;
        }
    }
    return 0;
}

/**
 * Check if EDID supports the colorimetry.
 * @param   color [in]  Colorimetry
 * @return  If EDID supports the colorimetry, return 1; Otherwise, return 0.
 */
static int CheckColorimetry(const enum HDMIColorimetry color)
{
    int i;

    // do not need to parse if not extended colorimetry
    if (color == HDMI_COLORIMETRY_NO_DATA ||
            color == HDMI_COLORIMETRY_ITU601 ||
            color == HDMI_COLORIMETRY_ITU709)
        return 1;

    // read EDID
    if (!EDIDRead())
       return 0;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i)) { // if it's timing block
            // check address
            unsigned int ExtAddr = i*SIZEOFEDIDBLOCK + EDID_DATA_BLOCK_START_POS;
            unsigned int EndAddr = i*SIZEOFEDIDBLOCK + gEdidData[i*SIZEOFEDIDBLOCK + EDID_DETAILED_TIMING_OFFSET_POS];
            unsigned int tag,blockLen;

            while (ExtAddr < EndAddr) {
                // find the block tag and length
                // tag
                tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
                // block len
                blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

                // check if it is colorimetry block
                if (tag == EDID_EXTENDED_TAG_VAL && // extended tag
                    gEdidData[ExtAddr+1] == EDID_EXTENDED_COLORIMETRY_VAL && // colorimetry block
                    (blockLen-1) == EDID_EXTENDED_COLORIMETRY_BLOCK_LEN) { // check length
                    // get supported DC value
                    int colorimetry = (gEdidData[ExtAddr + 2]);
                    int metadata = (gEdidData[ExtAddr + 3]);

                    DPRINTF("EDID extened colorimetry = %x\n",colorimetry);
                    DPRINTF("EDID gamut metadata profile = %x\n",metadata);

                    // check colorDepth
                    switch (color) {
                    case HDMI_COLORIMETRY_EXTENDED_xvYCC601:
                        if (colorimetry & EDID_XVYCC601_MASK && metadata)
                            return 1;
                        break;
                    case HDMI_COLORIMETRY_EXTENDED_xvYCC709:
                        if (colorimetry & EDID_XVYCC709_MASK && metadata)
                            return 1;
                        break;
                    default:
                        break;
                    }
                    return 0;
                }
                // else find next block
                ExtAddr += blockLen;
            }
        }
    }

    return 0;
}

/**
 * Get Max TMDS clock that HDMI Rx can receive.
 * @return  If available, return MaxTMDS clock; Otherwise, return 0.
 */
static unsigned int GetMaxTMDS(void)
{
    int i;
    unsigned int StartAddr;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i) // if it's timing block
            && ((StartAddr = GetVSDBOffset(i)) > 0)) { // check block
            int blockLength = gEdidData[StartAddr] & EDID_DATA_BLOCK_SIZE_MASK;
            if (blockLength >= EDID_MAX_TMDS_POS) {
                // get supported DC value
                return gEdidData[StartAddr + EDID_MAX_TMDS_POS];
            }
        }
    }

    return 0;
}

/**
 * Save first 16 VIC of EDID
 */
static void SaveVIC(void)
{
    int extension;
    int vic_count = 0;
    for (extension = 1; extension <= gExtensions && vic_count < NUM_OF_VIC_FOR_3D; extension++) {
        unsigned int StartAddr = extension*SIZEOFEDIDBLOCK;
        unsigned int ExtAddr = StartAddr + EDID_DATA_BLOCK_START_POS;
        unsigned int tag,blockLen;
        unsigned int DTDStartAddr = gEdidData[StartAddr + EDID_DETAILED_TIMING_OFFSET_POS];

        while (ExtAddr < StartAddr + DTDStartAddr) {
            // find the block tag and length
            // tag
            tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
            // block len
            blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

            // check if it is short video description
            if (tag == EDID_SHORT_VID_DEC_TAG_VAL) {
                // if so, check SVD
                unsigned int edid_index;
                for (edid_index = 1; edid_index < blockLen && vic_count < NUM_OF_VIC_FOR_3D; edid_index++) {
                    DPRINTF("EDIDVIC = %d\r\n", gEdidData[ExtAddr+edid_index] & EDID_SVD_VIC_MASK);

                    // check VIC with SVDB
                    aVIC[vic_count++] = (gEdidData[ExtAddr+edid_index] & EDID_SVD_VIC_MASK);
                }
            }
            // else find next block
            ExtAddr += blockLen;
        }
    }
}

/**
 * Check if Rx supports requested 3D format.
 * @param   pVideo [in]   HDMI Video Parameter
 * @return  If Rx supports requested 3D format, return 1; Otherwise, return 0.
 */
static int EDID3DFormatSupport(const struct HDMIVideoParameter * const pVideo)
{
    int edid_index;
    unsigned int StartAddr;
    unsigned int vic;
    vic = (pVideo->pixelAspectRatio == HDMI_PIXEL_RATIO_16_9) ?
            aVideoParams[pVideo->resolution].VIC16_9 : aVideoParams[pVideo->resolution].VIC;

    // if format == 2D, no need to check
    if (pVideo->hdmi_3d_format == HDMI_2D_VIDEO_FORMAT)
        return 1;

    // check EDID data is valid or not
    if (!EDIDRead())
        return 0;

    // save first 16 VIC to check
    SaveVIC();

    // find VSDB
    for (edid_index = 1; edid_index <= gExtensions; edid_index++) {
        if (IsTimingExtension(edid_index) // if it's timing block
            && ((StartAddr = GetVSDBOffset(edid_index)) > 0)) { // check block
            unsigned int blockLength = gEdidData[StartAddr] & EDID_DATA_BLOCK_SIZE_MASK;
            unsigned int VSDBHdmiVideoPre = 0;
            unsigned int VSDB3DPresent = 0;
            unsigned int VSDB3DMultiPresent = 0;
            unsigned int HDMIVICLen;
            unsigned int HDMI3DLen;
            int Hdmi3DStructure = 0;
            unsigned int Hdmi3DMask = 0xFFFF;
            unsigned int latency_offset = 0;

            DPRINTF("VSDB Block length[0x%x] = 0x%x\r\n",StartAddr,blockLength);

            // get HDMI Video Present  value
            if (blockLength >= EDID_HDMI_EXT_POS) {
                VSDBHdmiVideoPre = gEdidData[StartAddr + EDID_HDMI_EXT_POS]
                            & EDID_HDMI_VIDEO_PRESENT_MASK;
                DPRINTF("EDID HDMI Video Present = 0x%x\n",VSDBHdmiVideoPre);
            } else { // data related to 3D format is not available
                return 0;
            }

            // check if latency field is available
            latency_offset = (gEdidData[StartAddr + EDID_HDMI_EXT_POS]
                                                        & EDID_HDMI_LATENCY_MASK) >> EDID_HDMI_LATENCY_POS;
            if (latency_offset == 0)
                latency_offset = 4;
            else if (latency_offset == 3)
                latency_offset = 0;
            else
                latency_offset = 2;

            StartAddr -= latency_offset;

            // HDMI_VIC_LEN
            HDMIVICLen = (gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS]
                    & EDID_HDMI_VSDB_VIC_LEN_MASK) >> EDID_HDMI_VSDB_VIC_LEN_BIT;

            if (pVideo->hdmi_3d_format == HDMI_VIC_FORMAT) {
                if (HDMIVICLen) {
                    for (edid_index = 0; edid_index < (int)HDMIVICLen; edid_index++) {
                        if (vic == gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + edid_index])
                            return 1;
                    }
                    return 0;
                } else {
                    return 0;
                }
            }

            // HDMI_3D_LEN
            HDMI3DLen = gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS]
                        & EDID_HDMI_VSDB_3D_LEN_MASK;

            DPRINTF("HDMI VIC LENGTH[%x] = %x\r\n",
                                    StartAddr + EDID_HDMI_EXT_LENGTH_POS, HDMIVICLen);
            DPRINTF("HDMI 3D LENGTH[%x] = %x\r\n",
                                    StartAddr + EDID_HDMI_EXT_LENGTH_POS, HDMI3DLen);

            // check 3D_Present bit
            if (blockLength >= (EDID_HDMI_3D_PRESENT_POS - latency_offset)) {
                VSDB3DPresent = gEdidData[StartAddr + EDID_HDMI_3D_PRESENT_POS]
                                & EDID_HDMI_3D_PRESENT_MASK;
                VSDB3DMultiPresent = gEdidData[StartAddr + EDID_HDMI_3D_PRESENT_POS]
                                & EDID_HDMI_3D_MULTI_PRESENT_MASK;
            }

            if (VSDB3DPresent) {
                DPRINTF("VSDB 3D Present!!!\r\n");
                // check with 3D madatory format
                if (CheckResolution(pVideo->resolution, pVideo->pixelAspectRatio)) {
                    int size = sizeof(edid_3d)/sizeof(struct edid_3d_mandatory);
                    for (edid_index = 0; edid_index < size; edid_index++) {
                        if (edid_3d[edid_index].resolution == pVideo->resolution &&
                            edid_3d[edid_index].hdmi_3d_format == pVideo->hdmi_3d_format )
                            return 1;
                    }
                }
            }

            // check 3D_Multi_Present bit
            if (VSDB3DMultiPresent) {
                DPRINTF("VSDB 3D Multi Present!!! = 0x%02x\r\n",VSDB3DMultiPresent);
                // 3D Structure only
                if (VSDB3DMultiPresent == EDID_3D_STRUCTURE_ONLY_EXIST) {
                    // 3D Structure All
                    Hdmi3DStructure = (gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 1] << 8);
                    Hdmi3DStructure |= gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 2];
                    DPRINTF("VSDB 3D Structure!!! = [0x%02x]\r\n",Hdmi3DStructure);
                }

                // 3D Structure and Mask
                if (VSDB3DMultiPresent == EDID_3D_STRUCTURE_MASK_EXIST) {
                    // 3D Structure All
                    Hdmi3DStructure = (gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 1] << 8);
                    Hdmi3DStructure |= gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 2];
                    // 3D Structure Mask
                    Hdmi3DMask |= (gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 3] << 8);
                    Hdmi3DMask |= gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen + 4];
                    DPRINTF("VSDB 3D Structure!!! = [0x%02x]\r\n",Hdmi3DStructure);
                    DPRINTF("VSDB 3D Mask!!! = [0x%02x]\r\n",Hdmi3DMask);
                    DPRINTF("Current 3D Video format!!! = [%d]\r\n",pVideo->hdmi_3d_format);
                    DPRINTF("Current 3D Video format!!! = [0x%02x]\r\n",1<<pVideo->hdmi_3d_format);
                }

                // check 3D Structure and Mask
                if (Hdmi3DStructure & (1<<pVideo->hdmi_3d_format)) {
                    DPRINTF("VSDB 3D Structure Contains Current Video Structure!!!\r\n");
                    // check first 16 EDID
                    for (edid_index = 0; edid_index < NUM_OF_VIC_FOR_3D; edid_index++) {
                        DPRINTF("VIC = %d, EDID Vic = %d!!!\r\n",vic,aVIC[edid_index]);
                        if (Hdmi3DMask & (1<<edid_index)) {
                            if (vic == aVIC[edid_index]) {
                                DPRINTF("VSDB 3D Mask Contains Current Video format!!!\r\n");
                                return 1;
                            }
                        }
                    }
                }
            }

            // check block length if HDMI_VIC or HDMI Multi available
            if (blockLength >= (EDID_HDMI_EXT_LENGTH_POS - latency_offset)) {
                unsigned int HDMI3DExtLen = HDMI3DLen - (VSDB3DMultiPresent>>EDID_HDMI_3D_MULTI_PRESENT_BIT)*2;
                unsigned int VICOrder;

                // check if there is 3D extra data ?
                //TODO: check 3D_Detail in case of SSH
                if (HDMI3DExtLen) {
                    // check HDMI 3D Extra Data
                    for (edid_index = 0; edid_index < (int)(HDMI3DExtLen / 2); edid_index++) {
                        VICOrder = gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen +
                                          (VSDB3DMultiPresent>>EDID_HDMI_3D_MULTI_PRESENT_BIT) * 2 + edid_index * 2]
                                                    & EDID_HDMI_2D_VIC_ORDER_MASK;
                        VICOrder = (1<<VICOrder);
                        Hdmi3DStructure = gEdidData[StartAddr + EDID_HDMI_EXT_LENGTH_POS + HDMIVICLen +
                                                 (VSDB3DMultiPresent>>EDID_HDMI_3D_MULTI_PRESENT_BIT) * 2 + edid_index * 2]
                                                 & EDID_HDMI_3D_STRUCTURE_MASK;
                        Hdmi3DStructure = (1<<Hdmi3DStructure);
                        if (Hdmi3DStructure == pVideo->hdmi_3d_format && vic == aVIC[VICOrder])
                            return 1;
                    }
                }
            }
        }
    }

    return 0;
}

/**
 * Initialize EDID library. This will intialize DDC library.
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDOpen(void)
{
    // init DDC
    return DDCOpen();
}

/**
 * Finalize EDID library. This will finalize DDC library.
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDClose(void)
{
    // reset EDID
    EDIDReset();

    // close EDDC
    return DDCClose();
}

/**
 * Read EDID data of Rx.
 * @return If success, return 1; Otherwise, return 0;
 */
int EDIDRead(void)
{
    int block,dataPtr;
    unsigned char temp[SIZEOFEDIDBLOCK];

    // if already read??
    if (EDIDValid())
        return 1;

    // read EDID Extension Number
    // read EDID
    if (!ReadEDIDBlock(0,temp))
        return 0;

    // get extension
    gExtensions = temp[EDID_EXTENSION_NUMBER_POS];

    // prepare buffer
    gEdidData = (unsigned char*)malloc((gExtensions+1)*SIZEOFEDIDBLOCK);
    if (!gEdidData)
        return 0;

    // copy EDID Block 0
    memcpy(gEdidData,temp,SIZEOFEDIDBLOCK);

    // read EDID Extension
    for (block = 1,dataPtr = SIZEOFEDIDBLOCK; block <= gExtensions; block++,dataPtr+=SIZEOFEDIDBLOCK) {
        // read extension 1~gExtensions
        if (!ReadEDIDBlock(block, gEdidData+dataPtr)) {
            // reset buffer
            EDIDReset();
            return 0;
        }
    }

    // check if extension is more than 1, and first extension block is not block map.
    if (gExtensions > 1 && gEdidData[SIZEOFEDIDBLOCK] != EDID_BLOCK_MAP_EXT_TAG_VAL) {
        // reset buffer
        DPRINTF("EDID has more than 1 extension but, first extension block is not block map\n");
        EDIDReset();
        return 0;
    }

    return 1;
}

/**
 * Reset stored EDID data.
 */
void EDIDReset(void)
{
    if (gEdidData) {
        free(gEdidData);
        gEdidData = NULL;
        DPRINTF("\t\t\t\tEDID is reset!!!\n");
    }
}

/**
 * Get CEC physical address.
 * @param   outAddr [out]   CEC physical address. LSB 2 bytes is available. [0:0:AB:CD]
 * @return  If success, return 1; Otherwise, return 0.
 */
int EDIDGetCECPhysicalAddress(int* const outAddr)
{
    int i;
    unsigned int StartAddr;

    // check EDID data is valid or not
    // read EDID
    if (!EDIDRead())
        return 0;

    // find VSDB
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i) // if it's timing block
            && (StartAddr = GetVSDBOffset(i)) > 0) { // check block
            // get supported DC value
            // int tempDC1 = (int)(gEdidData[tempAddr+EDID_DC_POS]);
            int phyAddr = gEdidData[StartAddr + EDID_CEC_PHYICAL_ADDR] << 8;
            phyAddr |= gEdidData[StartAddr + EDID_CEC_PHYICAL_ADDR+1];

            DPRINTF("phyAddr = %x\n",phyAddr);

            *outAddr = phyAddr;

            return 1;
        }
    }

    return 0;
}

/**
 * Check if Rx supports HDMI/DVI mode or not.
 * @param   video [in]   HDMI or DVI mode to check
 * @return  If Rx supports requested mode, return 1; Otherwise, return 0.
 */
int EDIDHDMIModeSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check hdmi mode
    if (video->mode == HDMI) {
        if (!CheckHDMIMode()) {
            DPRINTF("HDMI mode Not Supported\n");
            return 0;
        }
    }
    return 1;
}

/**
 * Check if Rx supports requested video resoultion or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDVideoResolutionSupport(struct HDMIVideoParameter * const video)
{
    unsigned int TMDSClock;
    unsigned int MaxTMDS = 0;

    // check if read edid?
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // get max tmds
    MaxTMDS = GetMaxTMDS()*5;

    // Check MAX TMDS
    TMDSClock = aVideoParams[video->resolution].PixelClock/100;
    if (video->colorDepth == HDMI_CD_36)
        TMDSClock *= 1.5;
    else if (video->colorDepth == HDMI_CD_30)
        TMDSClock *=1.25;

    DPRINTF("MAX TMDS = %d, Current TMDS = %d\n",MaxTMDS, TMDSClock);
    if (MaxTMDS != 0 && MaxTMDS < TMDSClock) {
        DPRINTF("Pixel clock is beyond Maximun TMDS in EDID\n");
        return 0;
    }

    // check resolution
    if (!CheckResolution(video->resolution,video->pixelAspectRatio)) {
        DPRINTF("Video Resolution Not Supported\n");
        return 0;
    }

    // check 3D format
    if (!EDID3DFormatSupport(video)) {
        DPRINTF("3D Format Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested color depth or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorDepthSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check resolution
    if (!CheckColorDepth(video->colorDepth,video->colorSpace)) {
        DPRINTF("Color Depth Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested color space or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorSpaceSupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }
    // check color space
    if (!CheckColorSpace(video->colorSpace)) {
        DPRINTF("Color Space Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested colorimetry or not.
 * @param   video [in]   Video parameters to check
 * @return  If Rx supports video parameters, return 1; Otherwise, return 0.
 */
int EDIDColorimetrySupport(struct HDMIVideoParameter * const video)
{
    // check if read edid?
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check colorimetry
    if (!CheckColorimetry(video->colorimetry)) {
        DPRINTF("Colorimetry Not Supported\n");
        return 0;
    }

    return 1;
}

/**
 * Check if Rx supports requested audio parameters or not.
 * @param   audio [in]   Audio parameters to check
 * @return  If Rx supports audio parameters, return 1; Otherwise, return 0.
 */
int EDIDAudioModeSupport(struct HDMIAudioParameter * const audio)
{
    int i;

    // read EDID
    if (!EDIDRead()) {
        DPRINTF("EDID Read Fail!!!\n");
        return 0;
    }

    // check EDID Extension
    // find timing block
    for (i = 1; i <= gExtensions; i++) {
        if (IsTimingExtension(i)) { // if it's timing block
            // find Short Audio Description
            unsigned int StartAddr = i*SIZEOFEDIDBLOCK;
            unsigned int ExtAddr = StartAddr + EDID_DATA_BLOCK_START_POS;
            unsigned int tag,blockLen;
            unsigned int DTDStartAddr = gEdidData[StartAddr + EDID_DETAILED_TIMING_OFFSET_POS];

            while (ExtAddr < StartAddr + DTDStartAddr) {
                // find the block tag and length
                // tag
                tag = gEdidData[ExtAddr] & EDID_TAG_CODE_MASK;
                // block len
                blockLen = (gEdidData[ExtAddr] & EDID_DATA_BLOCK_SIZE_MASK) + 1;

                DPRINTF("tag = %d\n",tag);
                DPRINTF("blockLen = %d\n",blockLen-1);

                // check if it is short video description
                if (tag == EDID_SHORT_AUD_DEC_TAG_VAL) {
                    // if so, check SAD
                    unsigned int j, channelNum;
                    int audioFormat,sampleFreq,wordLen;
                    for (j = 1; j < blockLen; j += 3) {
                        audioFormat = gEdidData[ExtAddr+j] & EDID_SAD_CODE_MASK;
                        channelNum = gEdidData[ExtAddr+j] & EDID_SAD_CHANNEL_MASK;
                        sampleFreq = gEdidData[ExtAddr+j+1];
                        wordLen = gEdidData[ExtAddr+j+2];

                        DPRINTF("request = %d, EDIDAudioFormatCode = %d\n",(audio->formatCode)<<3, audioFormat);
                        DPRINTF("request = %d, EDIDChannelNumber= %d\n",(audio->channelNum)-1, channelNum);
                        DPRINTF("request = %d, EDIDSampleFreq= %d\n",1<<(audio->sampleFreq), sampleFreq);
                        DPRINTF("request = %d, EDIDWordLeng= %d\n",1<<(audio->wordLength), wordLen);

                        // check parameter
                        // check audioFormat
                        if (audioFormat & ( (audio->formatCode) << 3) &&  // format code
                                channelNum >= ( (audio->channelNum) -1) &&  // channel number
                                (sampleFreq & (1<<(audio->sampleFreq)))) { // sample frequency
                            if (audioFormat == LPCM_FORMAT) { // check wordLen
                                int ret = 0;
                                switch (audio->wordLength) {
                                case WORD_16:
                                case WORD_17:
                                case WORD_18:
                                case WORD_19:
                                case WORD_20:
                                    ret = wordLen & (1<<1);
                                    break;
                                case WORD_21:
                                case WORD_22:
                                case WORD_23:
                                case WORD_24:
                                    ret = wordLen & (1<<2);
                                    break;
                                }
                                return ret;
                            }
                            return 1; // if not LPCM
                        }
                    }
                }
                // else find next block
                ExtAddr += blockLen;
            }
        }
    }

    return 0;
}
