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

#include <errno.h>
#include <math.h>
#include <ctype.h>
#include "owt.h"

#define min(x, y)	(((x) < (y)) ? (x) : (y))

/*
 * strnchr() - Find character in a given string
 *
 * @s: String to search
 * @count: Number of characters to search
 * @c: Character being searched for
 *
 * Parses @s looking for @c for @count elements or until the end of @s.
 *
 * Returns the character being searched for or NULL if not found.
 *
 */

static char *strnchr(const char *s, unsigned int count, int c)
{
	while (count--) {
		if (*s == (char) c)
			return (char *) s;
		if (*s++ == '\0')
			break;
	}
	return NULL;
}

/*
 * parse_float() - Convert floating point value to integer
 *
 * @frac - Floating point value to be converted
 * @result - Resulting integer
 * @scale - Multiplier to increase the result by to obtain an integer
 * @minimum - Minimum allowable value for @result
 * @maximum - Maximum allowable value for @result
 *
 * Returns 0 upon success.
 * Returns negative errno upon error condition.
 *
 */
static int parse_float(char *frac, int *result, int scale, float min, float max)
{
	float fres;

	errno = 0;
	fres = strtof(frac, NULL);
	if (errno)
		return -errno;

	if (fres < min || fres > max)
		return -ERANGE;

	*result = roundf(fres * scale);

	return 0;
}

/*
 * dspmem_chunk_write() - Write data to dspmem_chunk struct
 *
 * @ch: Pointer to dspmem_chunk struct
 * @nbits: Number of bits to be written
 * @val: Value to be written
 *
 * Returns 0.
 *
 */
static int dspmem_chunk_write(struct dspmem_chunk *ch, int nbits, uint32_t val)
{
	int nwrite, i;

	nwrite = min(24 - ch->cachebits, nbits);

	ch->cache <<= nwrite;
	ch->cache |= val >> (nbits - nwrite);
	ch->cachebits += nwrite;
	nbits -= nwrite;

	if (ch->cachebits == 24) {
		if (dspmem_chunk_end(ch))
			return -ENOSPC;

		ch->cache &= 0xFFFFFF;
		for (i = 0; i < sizeof(ch->cache); i++, ch->cache <<= 8)
			*ch->data++ = (ch->cache & 0xFF000000) >> 24;

		ch->bytes += sizeof(ch->cache);
		ch->cachebits = 0;
	}

	if (nbits)
		return dspmem_chunk_write(ch, nbits, val);

	return 0;
}

/*
 * dspmem_chunk_flush() - Clear data from given memchunk's cache
 *
 * @ch: Pointer to dspmem_chunk struct
 *
 * Returns 0.
 *
 */
static int dspmem_chunk_flush(struct dspmem_chunk *ch)
{
	if (!ch->cachebits)
		return 0;

	return dspmem_chunk_write(ch, 24 - ch->cachebits, 0);
}

uint16_t gpi_config(bool rising_edge, unsigned int gpi)
{
	uint16_t cfg = 0;

	cfg |= (rising_edge << WVFRM_EDGE_SHIFT);
	cfg |= (gpi & WVFRM_GPI_MASK) << WVFRM_GPI_SHIFT;

	return cfg;
}

/*
 * owt_upload() - Upload the Open Wavetable Waveform via Input FF
 *
 * @data: Pointer to data to be written
 * @num_bytes: Size of data in bytes
 * @gpi: Value to tie to GPI
 * @fd: File descriptor corresponding to the open Input FF Device
 * @effect: Pointer to effect struct being uploaded
 *
 * Returns effect ID upon success.
 * Return negative errno on failure.
 *
 */
int owt_upload(uint8_t *data, uint32_t num_bytes, int gpi, int fd, bool edit,
		struct ff_effect *effect)
{
	int ret = 0;

	if (!edit) {
		effect->id = -1; /* New Effect */
		effect->type = FF_PERIODIC;
		effect->u.periodic.waveform = FF_CUSTOM;
		effect->replay.length = 0; /* Reserved value for OWT */
	}

	if (gpi)
		effect->trigger.button = gpi_config((gpi >= 0), abs(gpi));
	else
		effect->trigger.button = 0;

	effect->direction = 0;
	effect->u.periodic.custom_len = num_bytes / 2; /* # of 16-bit elements */
	effect->u.periodic.custom_data =
			(int16_t *) malloc(sizeof(int16_t) *
			effect->u.periodic.custom_len);
	if (effect->u.periodic.custom_data == NULL) {
		printf("Failed to allocate memory for custom data\n");
		return -ENOMEM;
	}

	memcpy(effect->u.periodic.custom_data, data, num_bytes);

	fflush(stdout);
	if (ioctl(fd, EVIOCSFF, effect) == -1) {
		printf("Failed to upload waveform\n");
		ret = -ENXIO;
		goto err_free;
	}

	printf("Successfully uploaded OWT effect with ID = %d\n", effect->id);
	return effect->id;
err_free:
	free(effect->u.periodic.custom_data);

	return ret;
}

/*
 * owt_trigger() - Start or stop playback of Open Wavetable effect
 *
 * @effect_id: Uploaded effect representing OWT waveform
 * @fd: File descriptor corresponding to the open Input FF Device
 * @play: Boolean determining if playing or stopping effect
 *
 * Returns 0 upon success.
 * Return negative errno on failure.
 *
 */
int owt_trigger(int effect_id, int fd, bool play)
{
	struct input_event event;

	memset(&event, 0, sizeof(event));
	event.type = EV_FF;
	event.value = play ? 1 : 0;
	event.code = effect_id;

	fflush(stdout);
	if ((write(fd, (const void *) &event, sizeof(event))) == -1) {
		printf("Could not %s effect\n", play ? "play" : "stop");
		return -ENXIO;
	}

	return 0;
}

/*******************************/
/* Waveform Type 10: Composite */
/*******************************/

/*
 * wt_type10_comp_to_buffer() - Format data to be written directly to wavetable
 *
 * @wave: Pointer to the composite struct
 * @size: Size of @buf in bytes
 * @buf: Data buffer to be written to
 *
 * Use dspmem_chunk() functions to format data from sections into structure
 * expected by the device. This buffer represents the data that will be written
 * directly to the RAM wavetable.
 *
 * Returns the total number of bytes written to the buffer. Note that this is
 * different than the buffer size.
 *
 */

static int wt_type10_comp_to_buffer(struct wt_type10_comp *comp, int size,
		void *buf)
{
	struct dspmem_chunk ch = dspmem_chunk_create(buf, size);
	int i;

	dspmem_chunk_write(&ch, 8, 0); /* Padding */
	dspmem_chunk_write(&ch, 8, comp->nsections);
	dspmem_chunk_write(&ch, 8, comp->repeat);

	for (i = 0; i < comp->nsections; i++) {
		dspmem_chunk_write(&ch, 8, comp->sections[i].wvfrm.amplitude);
		dspmem_chunk_write(&ch, 8, comp->sections[i].wvfrm.index);
		dspmem_chunk_write(&ch, 8, comp->sections[i].repeat);
		dspmem_chunk_write(&ch, 8, comp->sections[i].flags);
		dspmem_chunk_write(&ch, 16, comp->sections[i].delay);

		if (comp->sections[i].flags & WT_TYPE10_COMP_DURATION_FLAG) {
			dspmem_chunk_write(&ch, 8, 0); /* Padding */
			dspmem_chunk_write(&ch, 16, comp->sections[i].wvfrm.duration);
		}
	}

	return dspmem_chunk_bytes(&ch);
}

/*
 * wt_type10_comp_specifier_get() - Retrieve symbol specifier from given string
 *
 * @str: The string to be analyzed, a segment of the larger composite string
 *
 * Retrieve the specifier enum value that corresponds to the given string.
 * This is used to determine which action should be taken when parsing.
 *
 * Returns the enum value of the associated string; COMP_SPEC_INVALID is
 * returned if the string does not match any expected case.
 *
 */
static enum wt_type10_comp_specifier wt_type10_comp_specifier_get(char *str)
{
	if (!strcmp(str, "~"))
		return COMP_SPEC_OUTER_LOOP;
	else if (!strcmp(str, "!!"))
		return COMP_SPEC_INNER_LOOP_START;
	else if (strstr(str, "!!"))
		return COMP_SPEC_INNER_LOOP_STOP;
	else if (strnchr(str, WT_TYPE10_COMP_SEG_LEN_MAX, '!'))
		return COMP_SPEC_OUTER_LOOP_REPETITION;
	else if (strnchr(str, WT_TYPE10_COMP_SEG_LEN_MAX, '.'))
		return COMP_SPEC_WVFRM;
	else
		return COMP_SPEC_DELAY;

	return COMP_SPEC_INVALID;
}

/*
 * wt_type10_comp_waveform_get() - Retrieve waveform information from string
 *
 * @str: String containing waveform information
 * @wave: Pointer to the waveform structure to be populated
 *
 * Gets index, amplitude, and duration (if applicable) for
 * COMP_SEGMENT_TYPE_WVFRM segments.
 *
 * A value of 0 will be returned on success; a negative errno will be returned
 * in error cases.
 *
 */
static int wt_type10_comp_waveform_get(char *str,
				       struct wt_type10_comp_section *section)
{
	struct wt_type10_comp_wvfrm *wave = &section->wvfrm;
	unsigned int duration_tmp = 0;
	int i = 0, j = 0;
	unsigned int index_tmp, amp_tmp;
	char bank[4];
	int ret;

	if (strlen(str) < 3)
		return -EINVAL;

	if (isalpha(str[i++]) && isalpha(str[i++]) && isalpha(str[i++])) {
		/* Bank detected */
		while (j + i < strlen(str) && isalpha(str[j + i]))
			j++;
		ret = sscanf(str + j, "%3s%u.%u.%u", bank, &index_tmp, &amp_tmp, &duration_tmp);
		if (ret < 3) {
			printf("Failed to parse waveform\n");
			return -EINVAL;
		}
	} else {
		/* No bank specified */
		ret = sscanf(str + i - 1, "%u.%u.%u", &index_tmp, &amp_tmp, &duration_tmp);
		if (ret < 2) {
			printf("Failed to parse waveform\n");
			return -EINVAL;
		}
	}

	if (strncmp(bank, "RAM", 3) && strncmp(bank, "ROM", 3) && strncmp(bank, "OWT", 3)) {
		/* Default to RAM effect */
		strcpy(bank, "RAM");
	}

	if (amp_tmp == 0 || amp_tmp > 100) {
		printf("Invalid waveform amplitude: %u\n", amp_tmp);
		return -EINVAL;
	}

	if (duration_tmp != 0 && duration_tmp != WT_INDEF_TIME_VAL) {
		if (duration_tmp > WT_MAX_TIME_VAL) {
			printf("Duration too long: %u ms\n", duration_tmp);
			return -EINVAL;
		}
		duration_tmp *= 4;
	}

	wave->index = (uint8_t) (0xFF & index_tmp);
	wave->amplitude = (uint8_t) (0xFF & amp_tmp);
	wave->duration = (uint16_t) (0xFFFF & duration_tmp);
	if (!strncmp(bank, "ROM", 3))
		section->flags |= 0x40;
	else if (!strncmp(bank, "OWT", 3))
		section->flags |= 0x20;

	return 0;
}

/*
 * wt_type10_comp_decode() - Define segments according to given specifier
 *
 * @comp: Pointer to the composite waveform type struct
 * @specifier: Symbol to decode
 * @str: Value associated with the given specifier. i.e. Number of repeats
 * for COMP_SPEC_INNER_LOOP_STOP
 *
 * Populates the segment struct array which will be used to format binary
 * information. This is based on the current symbol as well as its relation
 * to the rest of the sequence.
 *
 * A value of 0 will be returned on success; a negative errno will be returned
 * in error cases.
 *
 */
static int wt_type10_comp_decode(struct wt_type10_comp *comp,
				 enum wt_type10_comp_specifier specifier,
				 char *str)
{
	struct wt_type10_comp_section *section;
	unsigned long l_val;
	int ret;

	section = &comp->sections[comp->nsections];

	switch (specifier) {
	case COMP_SPEC_OUTER_LOOP:
		if (comp->repeat) {
			printf("Duplicate outer loop specifier\n");
			return -EINVAL;
		}

		comp->repeat = WT_REPEAT_LOOP_MARKER;
		break;
	case COMP_SPEC_OUTER_LOOP_REPETITION:
		if (comp->repeat) {
			printf("Duplicate outer loop specifier\n");
			return -EINVAL;
		}

		l_val = strtoul(strsep(&str, "!"), NULL, 10);
		if (!l_val) {
			printf("Failed to get outer loop specifier\n");
			return -EINVAL;
		}

		comp->repeat = (uint8_t) (0xFF & l_val);
		break;
	case COMP_SPEC_INNER_LOOP_START:
		if (comp->inner_loop) {
			printf("Nested inner loop specifier not allowed\n");
			return -EINVAL;
		}

		if (section->wvfrm.amplitude || section->delay) {
			section++;
			comp->nsections++;
		}

		section->repeat = WT_REPEAT_LOOP_MARKER;

		comp->inner_loop = true;
		break;
	case COMP_SPEC_INNER_LOOP_STOP:
		if (!comp->inner_loop) {
			printf("Error: Inner loop stop with no start\n");
			return -EINVAL;
		}
		comp->inner_loop = false;

		l_val = strtoul(strsep(&str, "!"), NULL, 10);
		if (!l_val) {
			printf("Failed to get inner loop repeat value\n");
			return -EINVAL;
		}

		section->repeat = (uint8_t) (0xFF & l_val);

		section++;
		comp->nsections++;
		break;
	case COMP_SPEC_WVFRM:
		if (section->wvfrm.amplitude || section->delay) {
			section++;
			comp->nsections++;
		}

		ret = wt_type10_comp_waveform_get(str, section);
		if (ret)
			return ret;

		if (section->wvfrm.duration != 0)
			section->flags |= WT_TYPE10_COMP_DURATION_FLAG;
		break;
	case COMP_SPEC_DELAY:
		if (section->delay) {
			section++;
			comp->nsections++;
		}

		l_val = strtoul(str, NULL, 10);
		if (!l_val) {
			printf("Failed to get inner loop repeat value\n");
			return -EINVAL;
		}

		if (l_val > WT_MAX_DELAY) {
			printf("Delay %lu too long\n", l_val);
			return -EINVAL;
		}
		section->delay = (uint16_t) (0xFFFF & l_val);
		break;
	default:
		printf("Invalid specifier\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * wt_type10_comp_str_to_bin() - Converts Composite string to binary data
 *
 * @full_str: Entirety of the type 10 (Composite) OWT string
 * @data: Data buffer to be written to
 *
 * Converts the string to binary data that can be written directly to the
 * device for Composite Waveforms.
 *
 * Returns total number of bytes in @data upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type10_comp_str_to_bin(char *full_str, uint8_t *data)
{
	enum wt_type10_comp_specifier specifier;
	char delim[] = ", \n";
	struct wt_type10_comp comp = { 0 };
	char *str;
	int ret;

	str = strtok(full_str, delim);
	while (str != NULL) {
		specifier = wt_type10_comp_specifier_get(str);
		ret = wt_type10_comp_decode(&comp, specifier, str);
		if (ret)
			return ret;

		str = strtok(NULL, delim);
	}

	if (comp.inner_loop) {
		printf("Inner loop never terminated\n");
		return -EINVAL;
	}

	if (comp.sections[comp.nsections].wvfrm.amplitude ||
	    comp.sections[comp.nsections].delay)
		comp.nsections++;

	return wt_type10_comp_to_buffer(&comp,
			WT_TYPE12_PWLE_SINGLE_PACKED_MAX, data);
}

/**************************/
/* Waveform Type 12: PWLE */
/**************************/

/*
 * wt_type12_pwle_specifier_get() - Retrieve parameter type from string
 *
 * @str: String to be analyzed
 *
 * Returns the type of parameter being described in the provided string.
 *
 * Returns enum value for specifier.
 * Returns PWLE_SPEC_INVALID if no specifier is found in @str
 *
 */
static enum wt_type12_pwle_specifier wt_type12_pwle_specifier_get(char *str)
{
	if (str[0] == 'S')
		return PWLE_SPEC_SAVE;
	else if (!strncmp(str, "WF", 2))
		return PWLE_SPEC_FEATURE;
	else if (!strncmp(str, "RP", 2))
		return PWLE_SPEC_REPEAT;
	else if (!strncmp(str, "WT", 2))
		return PWLE_SPEC_WAIT;
	else if (str[0] == 'T')
		return PWLE_SPEC_TIME;
	else if (str[0] == 'L')
		return PWLE_SPEC_LEVEL;
	else if (str[0] == 'F')
		return PWLE_SPEC_FREQ;
	else if (str[0] == 'C')
		return PWLE_SPEC_CHIRP;
	else if (str[0] == 'B')
		return PWLE_SPEC_BRAKE;
	else if (!strncmp(str, "AR", 2))
		return PWLE_SPEC_AR;
	else if (str[0] == 'V')
		return PWLE_SPEC_VBT;
	else if (str[0] == 'M')
		return PWLE_SPEC_SVC_MODE;
	else if (str[0] == 'K')
		return PWLE_SPEC_SVC_BRAKING_TIME;
	else if (!strncmp(str, "EM", 2))
		return PWLE_SPEC_EP_LENGTH;
	else if (!strncmp(str, "ET", 2))
		return PWLE_SPEC_EP_PAYLOAD;
	else if (!strncmp(str, "EC", 2))
		return PWLE_SPEC_EP_THRESH;
	else if (str[0] == 'R')
		return PWLE_SPEC_RELFREQ;
	else if (!strncmp(str, "PO", 2))
		return PWLE_SPEC_PHASE_OFFSET;
	else
		return PWLE_SPEC_INVALID;
}

/*
 * wt_type12_pwle_save_entry() - Get value for save (1 or 0)
 *
 * @token: Portion of string being analyzed
 *
 * Verifies if the save value is valid. This value is unused but required
 * to ensure that the string is formatted properly.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_save_entry(char *token)
{
	int val = atoi(token);

	if (val == 1 || val == 0)
		return 0;

	printf("Save value must be 0 or 1\n");

	return -EINVAL;
}

/*
 * wt_type12_pwle_feature_entry() - Get feature specification from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the feature information from the string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_feature_entry(struct wt_type12_pwle *pwle,
		char *token)
{
	int val = atoi(token);

	if (val > WT_TYPE12_PWLE_MAX_WVFRM_FEAT || val < 0) {
		printf("Valid waveform features are: 0-255\n");
		return -EINVAL;
	}

	pwle->feature = val << WT_TYPE12_PWLE_WVFRM_FT_SHFT;

	return 0;
}

/*
 * wt_type12_pwle_repeat_entry() - Get repeat value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the global repeat value from the PWLE string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_repeat_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < 0 || val > WT_TYPE12_PWLE_MAX_RP_VAL) {
		printf("Valid repeat: 0 to 255\n");
		return -EINVAL;
	}

	pwle->repeat = val;

	return 0;
}

/*
 * wt_type12_pwle_svc_mode_entry() - Get SVC mode from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the SVC mode from the PWLE string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_svc_mode_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < -1 || val > 3) {
		printf("Valid SVC modes are 0 to 3, or -1 indicating none\n");
		return -EINVAL;
	} else if (val != -1) {
		pwle->svc_metadata.id = WT_SVC_METADATA_ID;
		pwle->svc_metadata.length = 1;
		pwle->svc_metadata.mode = val;
	}

	return 0;
}

/*
 * wt_type12_pwle_svc_braking_time_entry() - Get SVC braking time from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Get the SVC braking time from the PWLE string.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_svc_braking_time_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < 0 || val > WT_TYPE12_PWLE_MAX_BRAKING_TIME) {
		printf("Valid braking time: 0 to 1000 (ms)\n");
		return -EINVAL;
	}

	pwle->svc_metadata.braking_time = val * 8;

	return 0;
}


/*
 * wt_type12_pwle_ep_length_entry() - Get threshold mode from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_ep_length_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val > 1 || val < -1) {
		printf("Valid EP threshold mode: -1, 0, or 1 (is %d)\n", val);
		return -EINVAL;
	}

	if (val != -1) {
		pwle->ep_metadata.length = val;
		pwle->ep_metadata.id = 2;
	}

	return 0;
}

/*
 * wt_type12_pwle_ep_payload_entry() - Get excursion limit payload from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_ep_payload_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < 0 || val > 7) {
		printf("Valid EP payload: 0 - 7\n");
		return -EINVAL;
	}

	pwle->ep_metadata.payload = val;

	return 0;
}

/*
 * wt_type12_pwle_ep_threshold_entry() - Get excursion limit payload from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_ep_threshold_entry(struct wt_type12_pwle *pwle, char *token)
{
	int val = atoi(token);

	if (val < 0) {
		printf("Valid EP custom threshold: >= 0\n");
		return -EINVAL;
	}

	pwle->ep_metadata.custom_threshold = val;

	return 0;
}

/*
 * wt_type12_pwle_wait_time_entry() - Get wait time value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 *
 * Converts the wait time floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_wait_time_entry(struct wt_type12_pwle *pwle,
		char *token)
{
	int ret, val;

	/* Valid values from spec.: 0 ms - 1023.75 ms */
	ret = parse_float(token, &val, 4, 0.0f, 1023.75f);
	if (ret) {
		printf("Failed to parse wait time: %d\n", ret);
		return ret;
	}

	pwle->wait = val;
	pwle->wlength += pwle->wait;

	return 0;
}

/*
 * wt_type12_pwle_time_entry() - Get time value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 * @indef: Boolean to set to true if indefinite value (65535) given
 *
 * Converts the time floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_time_entry(struct wt_type12_pwle *pwle, char *token,
		struct wt_type12_pwle_section *section, bool *indef)
{
	int ret, val;

	/* Valid values from spec.: 0 ms - 16383.75 ms = infinite */
	ret = parse_float(token, &val, 4, 0.0f, 16383.75f);
	if (ret) {
		printf("Failed to parse time: %d\n", ret);
		return ret;
	}

	section->time = val;

	if (val == WT_TYPE12_PWLE_INDEF_TIME_VAL)
		*indef = true;
	else
		pwle->wlength += section->time;

	return 0;
}

/*
 * wt_type12_pwle_level_entry() - Get level value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the level floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_level_entry(struct wt_type12_pwle *pwle, char *token,
		struct wt_type12_pwle_section *section)
{
	int ret, val;

	/* Valid values from spec.: -1 - 0.9995118 */
	ret = parse_float(token, &val, 2048, -1.0f, 0.9995118f);
	if (ret) {
		printf("Failed to parse level: %d\n", ret);
		return ret;
	}

	section->level = val;

	return 0;
}

/*
 * wt_type12_pwle_freq_entry() - Get frequency value from string
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the frequency floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_freq_entry(struct wt_type12_pwle *pwle, char *token, char *freq_val,
		char *phase_offset, struct wt_type12_pwle_section *section)
{
	int ret, val, rel_val = atoi(token), offset = atoi(phase_offset);

	if (offset != 0 && offset != 1) {
		printf("Valid phase offset setting: 0 or 1\n");
		return -EINVAL;
	}

	if (rel_val != 0 && rel_val != 1) {
		printf("Valid relative frequency setting: 0 or 1\n");
		return -EINVAL;
	}

	if (offset == 1 && (pwle->feature & WT_TYPE12_PWLE_LF0T_FLAG) == 0) {
		printf("Live F0 tracking must be enabled in the feature bitmap to use phase offset\n");
		return -EINVAL;
	}

	if (!rel_val) {
		if (offset) {
			printf("F0-relative must be enabled in this section to use phase offset\n");
			return -EINVAL;
		}

		section->flags |= WT_TYPE12_PWLE_EXT_FREQ_BIT;

		/* Frequency is 0 (Resonant Frequency), or 0.25 Hz - 1023.75 Hz */
		ret = parse_float(freq_val, &val, 4, 0.25f, 1023.75f);
		if (ret) {
			if (atoi(freq_val) == 0)
				val = 0;
			else {
				printf("Failed to parse frequency: %d\nValid values are 0, or 0.25 - 1023.75\n", ret);
				return ret;
			}
		}
	} else {
		section->flags |= WT_TYPE12_PWLE_REL_FREQ_BIT;

		if (offset == 1) {
			section->flags |= WT_TYPE12_PWLE_PHASE_OFFSET_BIT;

			/* Phase offset is -1.0 - 1.0 */
			ret = parse_float(freq_val, &val, 2047, -1.0f, 1.0f);
			if (ret) {
				printf("Failed to parse frequency: %d\nValid values are -1 - 1\n", ret);
				return ret;
			}
		} else {
			/* Frequency is -512.00 Hz - 511.75 Hz */
			ret = parse_float(freq_val, &val, 4, -512.0f, 511.75f);
			if (ret) {
				printf("Failed to parse relative frequency: %d\nValid values are -512.0 - 511.75\n", ret);
				return ret;
			}
		}
	}

	section->frequency = val;

	return 0;
}

/*
 * wt_type12_pwle_vb_target_entry() - Get value of Back EMF target voltage
 *
 * @pwle: Pointer to struct with PWLE information
 * @token: Portion of string being analyzed
 * @section: Current section of PWLE being populated
 *
 * Converts the Back EMF voltage floating point value provided to an integer
 * that can be interpreted by the device driver.
 *
 * Returns 0 upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_vb_target_entry(struct wt_type12_pwle *pwle,
		char *token, struct wt_type12_pwle_section *section)
{
	int ret, val;

	/*
	 * Don't pass a scale value, since scaling is done locally.
	 * Valid values from spec.: 0 - 1.
	 */
	ret = parse_float(token, &val, 0x7FFFFF, 0.0f, 1.0f);
	if (ret) {
		printf("Failed to parse VB target: %d\n", ret);
		return ret;
	}

	section->vbtarget = val;

	return 0;
}

/*
 * wt_type12_pwle_write() - Populate data buffer with PWLE information
 *
 * @pwle: Pointer to pwle struct being used
 * @buf: Data buffer to be populated with PWLE info
 * @size: Maximum size in bytes of data to be written
 *
 * Write PWLE information to data buffer
 *
 * Returns the number of bytes written to the memory chunk
 *
 */
static int wt_type12_pwle_write(struct wt_type12_pwle *pwle, void *buf,
		int size)
{
	struct dspmem_chunk ch = dspmem_chunk_create(buf, size);
	uint32_t header_words = WT_TYPE12_HEADER_WORDS;
	int i;

	/* Header */
	dspmem_chunk_write(&ch, 16, pwle->feature);
	dspmem_chunk_write(&ch, 8, WT_TYPE12_PWLE);

	if (pwle->feature & WT_TYPE12_PWLE_METADATA_FLAG) {
		if (pwle->svc_metadata.id == WT_SVC_METADATA_ID)
			header_words += (1 + pwle->svc_metadata.length);
		if (pwle->ep_metadata.id == WT_EP_METADATA_ID)
			header_words += (1 + pwle->ep_metadata.length);
	}

	dspmem_chunk_write(&ch, 24, header_words);
	dspmem_chunk_write(&ch, 24, (pwle->nsections * 2) + pwle->nampsections + 3);

	/* Metadata */
	if (pwle->feature & WT_TYPE12_PWLE_METADATA_FLAG) {
		if (pwle->svc_metadata.id == WT_SVC_METADATA_ID) {
			dspmem_chunk_write(&ch, 8, pwle->svc_metadata.id);
			dspmem_chunk_write(&ch, 8, pwle->svc_metadata.length);
			dspmem_chunk_write(&ch, 8, pwle->svc_metadata.mode);
			dspmem_chunk_write(&ch, 24, pwle->svc_metadata.braking_time);
		}

		if (pwle->ep_metadata.id == WT_EP_METADATA_ID) {
			dspmem_chunk_write(&ch, 8, pwle->ep_metadata.id);
			dspmem_chunk_write(&ch, 8, pwle->ep_metadata.length);
			dspmem_chunk_write(&ch, 8, pwle->ep_metadata.payload);

			if (pwle->ep_metadata.length == 1)
				dspmem_chunk_write(&ch, 24, pwle->ep_metadata.custom_threshold);
		}
	}

	/* Terminator */
	dspmem_chunk_write(&ch, 24, WT_TYPE12_TERMINATOR);

	/* Section info */
	dspmem_chunk_write(&ch, 24, pwle->wlength);
	dspmem_chunk_write(&ch, 8, pwle->repeat);
	dspmem_chunk_write(&ch, 12, pwle->wait);
	dspmem_chunk_write(&ch, 8, pwle->nsections);

	/* Data */
	for (i = 0; i < pwle->nsections; i++) {
		dspmem_chunk_write(&ch, 16, pwle->sections[i].time);
		dspmem_chunk_write(&ch, 12, pwle->sections[i].level);
		dspmem_chunk_write(&ch, 12, pwle->sections[i].frequency);
		dspmem_chunk_write(&ch, 8, pwle->sections[i].flags);

		if (pwle->sections[i].flags & WT_TYPE12_PWLE_AMP_REG_BIT)
			dspmem_chunk_write(&ch, 24, pwle->sections[i].vbtarget);
	}

	dspmem_chunk_flush(&ch);

	return dspmem_chunk_bytes(&ch);
}

/*
 * wt_type12_pwle_str_to_bin() - Parse PWLE string to get binary data
 *
 * @full_str: Entire OWT string
 * @data: Data buffer to be populated
 *
 * Converts PWLE string to binary format to be written directly to the
 * device.
 *
 * Returns the number of bytes written to the data buffer upon success.
 * Returns negative errno in error case.
 *
 */
static int wt_type12_pwle_str_to_bin(char *full_str, uint8_t *data)
{
	bool t = false, l = false, f = false, c = false, b = false, a = false, r = false;
	bool v = false, p = false, indef = false;
	unsigned int num_vals = 0, num_segs = 0;
	char delim[] = ",\n";
	int ret = 0, val;
	struct wt_type12_pwle_section *section;
	struct wt_type12_pwle pwle;
	char *str, *freq_val, *phase_offset;

	pwle.wlength = 0;
	section = pwle.sections;
	section->flags = 0;

	str = strtok(full_str, delim);
	while (str != NULL) {
		if (num_vals >= WT_TYPE12_PWLE_TOTAL_VALS)
			return -E2BIG;

		switch (wt_type12_pwle_specifier_get(strsep(&str, ":"))) {
		case PWLE_SPEC_SAVE:
			if (num_vals != 0) {
				printf("Malformed PWLE, missing Save\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_save_entry(str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_FEATURE:
			if (num_vals != 1) {
				printf("Malformed PWLE, missing feature\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_feature_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_REPEAT:
			if (num_vals != 2) {
				printf("Malformed PWLE, missing repeat\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_repeat_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_WAIT:
			if (num_vals != 3) {
				printf("Malformed PWLE, incorrect WT slot\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_wait_time_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_SVC_MODE:
			if (num_vals != 4) {
				printf("Malformed PWLE, missing SVC mode\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_svc_mode_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_SVC_BRAKING_TIME:
			if (num_vals != 5) {
				printf("Malformed PWLE, missing SVC braking time\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_svc_braking_time_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_EP_LENGTH:
			if (num_vals != 6) {
				printf("Malformed PWLE, missing EP threshold mode\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_ep_length_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_EP_PAYLOAD:
			if (num_vals != 7) {
				printf("Malformed PWLE, missing EP payload\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_ep_payload_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_EP_THRESH:
			if (num_vals != 8) {
				printf("Malformed PWLE, missing EP custom threshold\n");
				return -EINVAL;
			}

			ret = wt_type12_pwle_ep_threshold_entry(&pwle, str);
			if (ret)
				return ret;
			break;
		case PWLE_SPEC_TIME:
			if (num_vals > PWLE_SPEC_NUM_VALS) {
				/* Verify complete previous segment */
				if (!t || !l || !f || !c || !b || !a || !v) {
					printf("PWLE Missing entry in seg %d\n",
						(num_segs - 1));
					return -EINVAL;
				}
				t = false;
				l = false;
				f = false;
				c = false;
				b = false;
				a = false;
				v = false;
			}

			ret = wt_type12_pwle_time_entry(&pwle, str, section,
					&indef);
			if (ret)
				return ret;

			t = true;
			break;
		case PWLE_SPEC_LEVEL:
			ret = wt_type12_pwle_level_entry(&pwle, str, section);
			if (ret)
				return ret;

			l = true;
			break;
		case PWLE_SPEC_FREQ:
			freq_val = str;
			f = true;
			break;
		case PWLE_SPEC_CHIRP:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid chirp: 0 or 1\n");
				return -EINVAL;
			}

			if (val)
				section->flags |= WT_TYPE12_PWLE_CHIRP_BIT;

			c = true;
			break;
		case PWLE_SPEC_BRAKE:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid Braking: 0 or 1\n");
				return -EINVAL;
			}

			if (val)
				section->flags |= WT_TYPE12_PWLE_BRAKE_BIT;

			b = true;
			break;
		case PWLE_SPEC_AR:
			val = atoi(str);
			if (val < 0 || val > 1) {
				printf("Valid Amplitude Regulation: 0 or 1\n");
				return -EINVAL;
			}

			if (val) {
				section->flags |= WT_TYPE12_PWLE_AMP_REG_BIT;
				pwle.nampsections++;
			}

			a = true;
			break;
		case PWLE_SPEC_PHASE_OFFSET:
			phase_offset = str;
			p = true;
			break;
		case PWLE_SPEC_RELFREQ:
			ret = wt_type12_pwle_freq_entry(&pwle, str, freq_val, phase_offset, section);
			if (ret)
				return ret;

			r = true;
			break;
		case PWLE_SPEC_VBT:
			if (section->flags & WT_TYPE12_PWLE_AMP_REG_BIT) {
				ret = wt_type12_pwle_vb_target_entry(&pwle,
						str, section);
				if (ret)
					return ret;
			}

			v = true;
			num_segs++;
			section++;
			section->flags = 0;
			break;
		default:
			printf("Invald PWLE input, exiting\n");
			return -EINVAL;
		}

		str = strtok(NULL, delim);
		num_vals++;
	}

	/* Verify last segment was complete */
	if (!t || !l || !f || !c || !b || !a || !v || !r || !p) {
		printf("Malformed PWLE: Missing entry in seg %d\n",
				(num_segs - 1));
		return -EINVAL;
	}

	pwle.nsections = num_segs;
	pwle.str_len = strlen(full_str);

	pwle.wlength *= (pwle.repeat + 1);
	pwle.wlength -= pwle.wait;
	pwle.wlength *= 2;

	if (indef)
		pwle.wlength |= WT_INDEFINITE;

	pwle.wlength |= WT_LEN_CALCD;

	return wt_type12_pwle_write(&pwle, data, WT_TYPE12_PWLE_BYTES_MAX);
}

/*
 * get_owt_data() - Determines the waveform type of the string to decode
 *
 * @full_str: Entire OWT string
 * @data: Data buffer to be populated
 *
 * Determines if the OWT string provided is of Type 10 (Composite) or Type 12
 * (PWLE) format and calls the corresponding function to convert the string
 * appropriately.
 *
 * Returns the number of bytes written to the data buffer upon success.
 * Returns negative errno in error case.
 *
 */
int get_owt_data(char *full_str, uint8_t *data)
{
	int num_bytes;

	printf("Full String = %s\n", full_str);
	/* check if PWLE or Composite via first character */
	if (full_str[0] == 'S')
		num_bytes = wt_type12_pwle_str_to_bin(full_str, data);
	else
		num_bytes =  wt_type10_comp_str_to_bin(full_str, data);

	return num_bytes;
}

/*
 * owt_version_show() - Displays OWT library version information
 *
 * Displays the OWT library version in [MAJOR].[MINOR].[PATCH] format
 *
 */
void owt_version_show(void)
{
	printf("OWT library version: 1.2.4\n");
	printf("Minimum compatible VIBEGEN version: 4.02.00\n");
}
