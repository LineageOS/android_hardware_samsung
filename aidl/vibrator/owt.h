/*
 * Copyright 2023 Cirrus Logic Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define WT_STR_MAX_LEN		512
#define WT_MAX_SECTIONS		256
#define WT_MAX_DELAY		10000
#define WT_INDEFINITE		0x00400000
#define WT_LEN_CALCD		0x00800000
#define WT_REPEAT_LOOP_MARKER	0xFF
#define WT_INDEF_TIME_VAL	0xFFFF
#define WT_MAX_TIME_VAL		16383 /* ms */
#define WT_SVC_METADATA_ID	1
#define WT_EP_METADATA_ID	2

#define WT_TYPE10_COMP_METADATA_LEN	2
#define WT_TYPE10_COMP_SEG_LEN_MAX	20
#define WT_TYPE10_COMP_DURATION_FLAG	0x80

#define WT_TYPE12_HEADER_WORDS			4
#define WT_TYPE12_TERMINATOR			0xFFFFFF
#define WT_TYPE12_PWLE_TOTAL_VALS		1787
#define WT_TYPE12_PWLE_MAX_SEG_BYTES		9
#define WT_TYPE12_PWLE_NON_SEG_BYTES		7
#define WT_TYPE12_PWLE_BYTES_MAX		2302
#define WT_TYPE12_PWLE_MAX_RP_VAL		255
#define WT_TYPE12_PWLE_INDEF_TIME_VAL		65535
#define WT_TYPE12_PWLE_MAX_WVFRM_FEAT		255
#define WT_TYPE12_PWLE_WVFRM_FT_SHFT		8
#define WT_TYPE12_PWLE_METADATA_FLAG		(1 << 10)
#define WT_TYPE12_PWLE_LF0T_FLAG		(1 << 8)
#define WT_TYPE12_PWLE_CHIRP_BIT		(1 << 7)
#define WT_TYPE12_PWLE_BRAKE_BIT		(1 << 6)
#define WT_TYPE12_PWLE_AMP_REG_BIT		(1 << 5)
#define WT_TYPE12_PWLE_EXT_FREQ_BIT		(1 << 4)
#define WT_TYPE12_PWLE_REL_FREQ_BIT		(1 << 3)
#define WT_TYPE12_PWLE_PHASE_OFFSET_BIT		(1 << 2)
#define WT_TYPE12_PWLE_SINGLE_PACKED_MAX	1152
#define WT_TYPE12_PWLE_MAX_BRAKING_TIME		1000 /* ms */
#define WT_TYPE12_PWLE				12

#define WVFRM_INDEX_MASK	0x7F
#define WVFRM_BUZZ_SHIFT	7
#define WVFRM_GPI_MASK		0x7
#define WVFRM_GPI_SHIFT		12
#define WVFRM_EDGE_SHIFT	15

enum wt_type12_pwle_specifier {
	PWLE_SPEC_SAVE,
	PWLE_SPEC_FEATURE,
	PWLE_SPEC_REPEAT,
	PWLE_SPEC_WAIT,
	PWLE_SPEC_SVC_MODE,
	PWLE_SPEC_SVC_BRAKING_TIME,
	PWLE_SPEC_EP_LENGTH,
	PWLE_SPEC_EP_PAYLOAD,
	PWLE_SPEC_EP_THRESH,
	PWLE_SPEC_NUM_VALS,
	PWLE_SPEC_TIME,
	PWLE_SPEC_LEVEL,
	PWLE_SPEC_FREQ,
	PWLE_SPEC_CHIRP,
	PWLE_SPEC_BRAKE,
	PWLE_SPEC_AR,
	PWLE_SPEC_VBT,
	PWLE_SPEC_RELFREQ,
	PWLE_SPEC_PHASE_OFFSET,
	PWLE_SPEC_INVALID,
};

enum wt_type10_comp_specifier {
	COMP_SPEC_OUTER_LOOP,
	COMP_SPEC_INNER_LOOP_START,
	COMP_SPEC_INNER_LOOP_STOP,
	COMP_SPEC_OUTER_LOOP_REPETITION,
	COMP_SPEC_EP_DATA_START,
	COMP_SPEC_WVFRM,
	COMP_SPEC_DELAY,
	COMP_SPEC_INVALID,
};

struct dspmem_chunk {
	uint8_t *data;
	uint8_t *max;
	int bytes;

	uint32_t cache;
	int cachebits;
};

struct wt_type12_pwle_section {
	uint16_t time;
	uint16_t level;
	uint16_t frequency;
	uint8_t flags;
	uint32_t vbtarget;
};

struct wt_type12_svc_metadata {
	uint8_t id;
	uint8_t length;
	uint8_t mode;
	uint32_t braking_time;
};

struct wt_ep_metadata {
	unsigned int id;
	unsigned int length;
	unsigned int payload;
	unsigned int custom_threshold;
};

struct wt_type12_pwle {
	uint16_t feature;
	unsigned int str_len;
	uint32_t wlength;
	uint8_t repeat;
	uint16_t wait;
	uint8_t nsections;
	double nampsections;

	struct wt_type12_svc_metadata svc_metadata;
	struct wt_ep_metadata ep_metadata;
	struct wt_type12_pwle_section sections[WT_MAX_SECTIONS];
};

struct wt_type10_comp_wvfrm {
	uint8_t index;
	uint8_t amplitude;
	uint16_t duration;
};

struct wt_type10_comp_section {
	uint8_t repeat;
	uint8_t flags;
	struct wt_type10_comp_wvfrm wvfrm;
	uint16_t delay;
};

struct wt_type10_comp {
	uint8_t nsections;
	uint8_t repeat;
	bool inner_loop;
	int fd;

	struct wt_type10_comp_section sections[WT_MAX_SECTIONS];
	struct wt_ep_metadata ep_metadata;
};

/*
 * dspmem_chunk_create() - Create dspmem_chunk struct for given buffer
 *
 * @data: Buffer to be associated with dspmem_chunk struct
 * @size: Size of data buffer in bytes
 *
 * Returns dspmem_chunk struct whose data pointer is the same as the provided
 * @data buffer with a max value at the expected size.
 *
 */
static inline struct dspmem_chunk dspmem_chunk_create(uint8_t *data, int size)
{
	struct dspmem_chunk ch = {
		.data = data,
		.max = data + size,
	};

	return ch;
}

/*
 * dspmem_chunk_end() - Check if dspmem_chunk struct is full
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns true if @ch's data buffer is full.
 * Returns false if @ch's data buffer is not full.
 *
 */
static inline bool dspmem_chunk_end(struct dspmem_chunk *ch)
{
	return ch->data == ch->max;
}

/*
 * dspmem_chunk_bytes() - Get number of bytes in dspmem_chunk struct data
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns number of bytes in @ch's data field.
 *
 */
static inline int dspmem_chunk_bytes(struct dspmem_chunk *ch)
{
	return ch->bytes;
}

uint16_t gpi_config(bool rising_edge, unsigned int gpi);
int get_owt_data(char *full_str, uint8_t *data);
int owt_upload(uint8_t *data, uint32_t num_bytes, int gpi, int fd, bool edit, struct ff_effect *effect);
int owt_trigger(int effect_id, int fd, bool play);
void owt_version_show(void);
