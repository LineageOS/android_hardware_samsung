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

#ifndef _EDID_H_
#define _EDID_H_

//@{
/**
 * @name EDID Addresses
 */
#define EDID_ADDR                                       (0xA0)
#define EDID_SEGMENT_POINTER                            (0x60)
//@}

//@{
/**
 * @name EDID offset and bit values
 */
#define SIZEOFBYTE                                      (8)
#define SIZEOFEDIDBLOCK                                 (0x80)
#define EDID_EXTENSION_NUMBER_POS                       (0x7E)

#define EDID_TIMING_EXT_TAG_ADDR_POS                    (0)
#define EDID_TIMING_EXT_REV_NUMBER_POS                  (1)
#define EDID_DETAILED_TIMING_OFFSET_POS                 (2)
#define EDID_DATA_BLOCK_START_POS                       (4)

// for Extension Data Block
#define EDID_TIMING_EXT_TAG_VAL                         (0x02)
#define EDID_BLOCK_MAP_EXT_TAG_VAL                      (0xF0)

#define EDID_SHORT_AUD_DEC_TAG_VAL                      (1<<5)
#define EDID_SHORT_VID_DEC_TAG_VAL                      (2<<5)
#define EDID_VSDB_TAG_VAL                               (3<<5)
#define EDID_SPEAKER_ALLOCATION_TAG_VAL                 (4<<5)
#define EDID_VESA_DTC_TAG_VAL                           (5<<5)
#define EDID_RESERVED_TAG_VAL                           (6<<5)

#define EDID_EXTENDED_TAG_VAL                           (7<<5)
#define EDID_EXTENDED_COLORIMETRY_VAL                   (5)
#define EDID_EXTENDED_COLORIMETRY_BLOCK_LEN             (3)

#define EDID_TAG_CODE_MASK                              (1<<7 | 1<<6 | 1<<5)
#define EDID_DATA_BLOCK_SIZE_MASK                       (1<<4 | 1<<3 | 1<<2 | 1<<1 | 1<<0)

#define EDID_VSDB_MIN_LENGTH_VAL                        (5)

// for Established Timings
#define EDID_ET_POS                                     (0x23)
#define EDID_ET_640x480p_VAL                            (0x20)

// for DTD
#define EDID_DTD_START_ADDR                             (0x36)
#define EDID_DTD_BYTE_LENGTH                            (18)
#define EDID_DTD_TOTAL_LENGTH                           (EDID_DTD_BYTE_LENGTH*4)

#define EDID_DTD_PIXELCLOCK_POS1                        (0)
#define EDID_DTD_PIXELCLOCK_POS2                        (1)

#define EDID_DTD_HBLANK_POS1                            (3)
#define EDID_DTD_HBLANK_POS2                            (4)
#define EDID_DTD_HBLANK_POS2_MASK                       (0xF)

#define EDID_DTD_HACTIVE_POS1                           (2)
#define EDID_DTD_HACTIVE_POS2                           (4)
#define EDID_DTD_HACTIVE_POS2_MASK                      (0xF0)

#define EDID_DTD_VBLANK_POS1                            (6)
#define EDID_DTD_VBLANK_POS2                            (7)
#define EDID_DTD_VBLANK_POS2_MASK                       (0x0F)

#define EDID_DTD_VACTIVE_POS1                           (5)
#define EDID_DTD_VACTIVE_POS2                           (7)
#define EDID_DTD_VACTIVE_POS2_MASK                      (0xF0)

#define EDID_DTD_INTERLACE_POS                          (17)
#define EDID_DTD_INTERLACE_MASK                         (1<<7)

// for SVD
#define EDID_SVD_VIC_MASK                               (0x7F)

// for CS
#define EDID_COLOR_SPACE_POS                            (3)
#define EDID_YCBCR444_CS_MASK                           (1<<5)
#define EDID_YCBCR422_CS_MASK                           (1<<4)

// for Color Depth
#define EDID_DC_48_VAL                                  (1<<6)
#define EDID_DC_36_VAL                                  (1<<5)
#define EDID_DC_30_VAL                                  (1<<4)
#define EDID_DC_YCBCR_VAL                               (1<<3)

#define EDID_DC_POS                                     (6)
#define EDID_DC_MASK                                    (EDID_DC_48_VAL | EDID_DC_36_VAL| EDID_DC_30_VAL | EDID_DC_YCBCR_VAL)

// for colorimetry
#define EDID_XVYCC601_MASK                              (1<<0)
#define EDID_XVYCC709_MASK                              (1<<1)
#define EDID_EXTENDED_MASK                              (1<<0|1<<1|1<<2)

// for SAD
#define SHORT_AUD_DESCRIPTOR_LPCM                       (1<<0)
#define SHORT_AUD_DESCRIPTOR_AC3                        (1<<1)
#define SHORT_AUD_DESCRIPTOR_MPEG1                      (1<<2)
#define SHORT_AUD_DESCRIPTOR_MP3                        (1<<3)
#define SHORT_AUD_DESCRIPTOR_MPEG2                      (1<<4)
#define SHORT_AUD_DESCRIPTOR_AAC                        (1<<5)
#define SHORT_AUD_DESCRIPTOR_DTS                        (1<<6)
#define SHORT_AUD_DESCRIPTOR_ATRAC                      (1<<7)

#define EDID_SAD_CODE_MASK                              (1<<6 | 1<<5 | 1<<4 | 1<<3)
#define EDID_SAD_CHANNEL_MASK                           (1<<2 | 1<<1 | 1<<0)
#define EDID_SAD_192KHZ_MASK                            (1<<6)
#define EDID_SAD_176KHZ_MASK                            (1<<5)
#define EDID_SAD_96KHZ_MASK                             (1<<4)
#define EDID_SAD_88KHZ_MASK                             (1<<3)
#define EDID_SAD_48KHZ_MASK                             (1<<2)
#define EDID_SAD_44KHZ_MASK                             (1<<1)
#define EDID_SAD_32KHZ_MASK                             (1<<0)

#define EDID_SAD_WORD_24_MASK                           (1<<2)
#define EDID_SAD_WORD_20_MASK                           (1<<1)
#define EDID_SAD_WORD_16_MASK                           (1<<0)

// for CEC
#define EDID_CEC_PHYICAL_ADDR                           (4)

// for 3D
#define EDID_HDMI_EXT_POS                               (8)
#define EDID_HDMI_VIDEO_PRESENT_MASK                    (1<<5)

// latency
#define EDID_HDMI_LATENCY_MASK                          (1<<7|1<<6)
#define EDID_HDMI_LATENCY_POS                           (6)

#define EDID_HDMI_3D_PRESENT_POS                        (13)
#define EDID_HDMI_3D_PRESENT_MASK                       (1<<7)
#define EDID_HDMI_3D_MULTI_PRESENT_MASK                 (1<<6 | 1<<5)
#define EDID_HDMI_3D_MULTI_PRESENT_BIT                  5

#define EDID_3D_STRUCTURE_ONLY_EXIST                    (1<<5)
#define EDID_3D_STRUCTURE_MASK_EXIST                    (1<<6)

#define EDID_3D_STRUCTURE_FP                            (0)
#define EDID_3D_STRUCTURE_FA                            (1)
#define EDID_3D_STRUCTURE_LA                            (2)
#define EDID_3D_STRUCTURE_SSF                           (3)
#define EDID_3D_STRUCTURE_LD                            (4)
#define EDID_3D_STRUCTURE_LDGFX                         (5)
#define EDID_3D_STRUCTURE_TB                            (6)
#define EDID_3D_STRUCTURE_SSH                           (8)

#define EDID_HDMI_EXT_LENGTH_POS                        (14)
#define EDID_HDMI_VSDB_VIC_LEN_BIT                      (5)
#define EDID_HDMI_VSDB_VIC_LEN_MASK                     (1<<7|1<<6|1<<5)
#define EDID_HDMI_VSDB_3D_LEN_MASK                      (1<<4|1<<3|1<<2|1<<1|1<<0)

#define EDID_HDMI_2D_VIC_ORDER_MASK                     (1<<7|1<<6|1<<5|1<<4)
#define EDID_HDMI_3D_STRUCTURE_MASK                     (1<<3|1<<2|1<<1|1<<0)

// for MAX TMDS
#define EDID_MAX_TMDS_POS                               (7)

// for 3D Structure
#define NUM_OF_VIC_FOR_3D                               16
//@}

#endif /* _EDID_H_ */
