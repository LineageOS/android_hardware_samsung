/*
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * Using DTBH compilation code by Ketut Putu Kumajaya, June 2014
** Source: https://github.com/kumajaya/mkdtbhbootimg
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
	   * Redistributions of source code must retain the above copyright
		 notice, this list of conditions and the following disclaimer.
	   * Redistributions in binary form must reproduce the above
		 copyright notice, this list of conditions and the following
		 disclaimer in the documentation and/or other materials provided
		 with the distribution.
	   * Neither the name of The Linux Foundation nor the names of its
		 contributors may be used to endorse or promote products derived
		 from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <stdint.h>

#include "libfdt.h"

#define PAGE_SIZE_DEF  2048

#define DTBH_MAGIC		"DTBH"
#define DTBH_VERSION	2
#define DTBH_PLATFORM	"android"
#define DTBH_SUBTYPE	"samsung"
/* Hardcoded entry */
#define DTBH_PLATFORM_CODE 0x50a6
#define DTBH_SUBTYPE_CODE  0x217584da

struct dt_blob;

/* DTBH_MAGIC + DTBH_VERSION + DTB counts */
#define DT_HEADER_PHYS_SIZE 12

/*
 * keep the eight uint32_t entries first in this struct so we can memcpy them to the file
 */
#define DT_ENTRY_PHYS_SIZE (sizeof(uint32_t) * 8)
struct dt_entry {
	uint32_t chip;
	uint32_t platform;
	uint32_t subtype;
	uint32_t hw_rev;
	uint32_t hw_rev_end;
	uint32_t offset;
	uint32_t size; /* including padding */
	uint32_t space;

	struct dt_blob *blob;
};

/*
 * In memory representation of a dtb blob
 */
struct dt_blob {
	uint32_t size;
	uint32_t offset;

	void *payload;
	struct dt_blob *next;
};

static void *load_file(const char *fn, unsigned *_sz)
{
	char *data;
	int sz;
	int fd;

	data = 0;
	fd = open(fn, O_RDONLY);
	if(fd < 0) return 0;

	sz = lseek(fd, 0, SEEK_END);
	if(sz < 0) goto oops;

	if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

	data = (char*) malloc(sz);
	if(data == 0) goto oops;

	if(read(fd, data, sz) != sz) goto oops;
	close(fd);

	if(_sz) *_sz = sz;
	return data;

oops:
	close(fd);
	if(data != 0) free(data);
	return 0;
}

static void *load_dtbh_block(const char *dtb_path, unsigned pagesize, unsigned *_sz)
{
	const unsigned pagemask = pagesize - 1;
	struct dt_entry *new_entries;
	struct dt_entry *entries = NULL;
	struct dt_entry *entry;
	struct dt_blob *blob;
	struct dt_blob *blob_list = NULL;
	struct dt_blob *last_blob = NULL;
	struct dirent **de;
	unsigned new_count;
	unsigned entry_count = 0;
	unsigned offset;
	unsigned dtb_sz;
	unsigned hdr_sz = DT_HEADER_PHYS_SIZE;
	uint32_t version = DTBH_VERSION;
	unsigned blob_sz = 0;
	char fname[PATH_MAX];
	const unsigned *prop_chip;
	const unsigned *prop_platform;
	const unsigned *prop_subtype;
	const unsigned *prop_hw_rev;
	const unsigned *prop_hw_rev_end;
	const unsigned *prop_compatible;
	int namlen;
	int len;
	int f, files;
	void *dtb;
	char *dtbh;
	unsigned c;

	files = scandir(dtb_path, &de, NULL, alphasort);
	if (files < 0)
		err(1, "failed to open '%s'", dtb_path);

	for (f = 0; f < files; f++) {
		namlen = strlen(de[f]->d_name);
		if (namlen < 4 || strcmp(&de[f]->d_name[namlen - 4], ".dtb"))
			continue;

		snprintf(fname, sizeof(fname), "%s/%s", dtb_path, de[f]->d_name);

		free(de[f]);

		dtb = load_file(fname, &dtb_sz);
		if (dtb == NULL)
			err(1, "failed to read dtb '%s'", fname);

		if (fdt_check_header(dtb) != 0) {
			warnx("'%s' is not a valid dtb, skipping", fname);
			free(dtb);
			continue;
		}

		offset = fdt_path_offset(dtb, "/");

		prop_chip = fdt_getprop(dtb, offset, "model_info-chip", &len);
		if (len % (sizeof(uint32_t)) != 0) {
			warnx("model_info-chip of %s is of invalid size, skipping", fname);
			free(dtb);
			continue;
		}

		prop_platform = fdt_getprop(dtb, offset, "model_info-platform", &len);
		if (strcmp((char *)&prop_platform[0], DTBH_PLATFORM)) {
			warnx("model_info-platform of %s is invalid, skipping", fname);
			free(dtb);
			continue;
		}

		prop_subtype = fdt_getprop(dtb, offset, "model_info-subtype", &len);
		if (strcmp((char *)&prop_subtype[0], DTBH_SUBTYPE)) {
			warnx("model_info-subtype of %s is invalid, skipping", fname);
			free(dtb);
			continue;
		}

		prop_hw_rev = fdt_getprop(dtb, offset, "model_info-hw_rev", &len);
		if (len % (sizeof(uint32_t)) != 0) {
			warnx("model_info-hw_rev of %s is of invalid size, skipping", fname);
			free(dtb);
			continue;
		}

		prop_hw_rev_end = fdt_getprop(dtb, offset, "model_info-hw_rev_end", &len);
		if (len % (sizeof(uint32_t)) != 0) {
			warnx("model_info-hw_rev_end of %s is of invalid size, skipping", fname);
			free(dtb);
			continue;
		}

		prop_compatible = fdt_getprop(dtb, offset, "compatible", NULL);
		if (!prop_compatible) {
			warnx("compatible field of %s is missing, skipping", fname);
			free(dtb);
			continue;
		}

		printf("=> Found compatible entry: %s\n"
			  "    (chip: %u, hw_rev: %u, hw_rev_end: %u)\n",
				(char *)&prop_compatible[0], ntohl(prop_chip[0]),
				ntohl(prop_hw_rev[0]), ntohl(prop_hw_rev_end[0]));

		blob = calloc(1, sizeof(struct dt_blob));
		if (blob == NULL)
			err(1, "failed to allocate memory");

		blob->payload = dtb;
		blob->size = dtb_sz;
		if (blob_list == NULL) {
			blob_list = blob;
			last_blob = blob;
		} else {
			last_blob->next = blob;
			last_blob = blob;
		}

		blob_sz += (blob->size + pagemask) & ~pagemask;
		new_count = entry_count + 1;
		new_entries = realloc(entries, new_count * sizeof(struct dt_entry));
		if (new_entries == NULL)
			err(1, "failed to allocate memory");

		entries = new_entries;
		entry = &entries[entry_count];
		memset(entry, 0, sizeof(*entry));
		entry->chip = ntohl(prop_chip[0]);
		entry->platform = DTBH_PLATFORM_CODE;
		entry->subtype = DTBH_SUBTYPE_CODE;
		entry->hw_rev = ntohl(prop_hw_rev[0]);
		entry->hw_rev_end = ntohl(prop_hw_rev_end[0]);
		entry->space = 0x20; /* space delimiter */
		entry->blob = blob;

		entry_count++;

		hdr_sz += entry_count * DT_ENTRY_PHYS_SIZE;
	}

	free(de);

	if (entry_count == 0) {
		warnx("unable to locate any dtbs in the given path");
		return NULL;
	}

	hdr_sz += sizeof(uint32_t); /* eot marker */
	hdr_sz = (hdr_sz + pagemask) & ~pagemask;

	/* The size of the dt header is now known, calculate the blob offsets... */
	offset = hdr_sz;
	for (blob = blob_list; blob; blob = blob->next) {
		blob->offset = offset;
		offset += (blob->size + pagemask) & ~pagemask;
	}

	/* ...and update the entries */
	for (c = 0; c < entry_count; c++) {
		entry = &entries[c];

		entry->offset = entry->blob->offset;
		entry->size = (entry->blob->size + pagemask) & ~pagemask;
	}

	/*
	 * All parts are now gathered, so build the dt block
	 */
	dtbh = calloc(hdr_sz + blob_sz, 1);
	if (dtbh == NULL)
			err(1, "failed to allocate memory");

	offset = 0;

	memcpy(dtbh, DTBH_MAGIC, sizeof(uint32_t));
	memcpy(dtbh + sizeof(uint32_t), &version, sizeof(uint32_t));
	memcpy(dtbh + (sizeof(uint32_t) * 2), &entry_count, sizeof(uint32_t));

	offset += DT_HEADER_PHYS_SIZE;

	/* add dtbh entries */
	for (c = 0; c < entry_count; c++) {
		entry = &entries[c];
		memcpy(dtbh + offset, entry, DT_ENTRY_PHYS_SIZE);
		offset += DT_ENTRY_PHYS_SIZE;
	}

	/* add padding after dt header */
	offset += pagesize - (offset & pagemask);

	for (blob = blob_list; blob; blob = blob->next) {
		memcpy(dtbh + offset, blob->payload, blob->size);
		offset += (blob->size + pagemask) & ~pagemask;
	}

	*_sz = hdr_sz + blob_sz;

	return dtbh;
}

int usage(char *exec_name)
{
	fprintf(stderr,"usage: %s\n"
			"      [ -s|--pagesize <pagesize> ]\n"
			"      -d|--dtb <dtb path>\n"
			"      -o|--output <filename>\n",
			exec_name);
	return 1;
}

int main(int argc, char **argv)
{
	char *exec_name = argv[0];
	char *dt_img = 0;
	char *dt_dir = 0;
	void *dt_data = 0;
	unsigned pagesize = PAGE_SIZE_DEF;
	int fd;
	unsigned dt_size;

	argc--;
	argv++;

	while(argc > 0){
		char *arg = argv[0];
		char *val = argv[1];
		if(argc < 2) {
			return usage(exec_name);
		}
		argc -= 2;
		argv += 2;

		if (!strcmp(arg,"--pagesize") || !strcmp(arg,"-s")) {
			pagesize = strtoul(val, 0, 10);
			if ((pagesize != 2048) && (pagesize != 4096) && (pagesize != 8192) && (pagesize != 16384) && (pagesize != 32768) && (pagesize != 65536) && (pagesize != 131072)) {
				fprintf(stderr,"error: unsupported page size %d\n", pagesize);
				return -1;
			}
		}
		else if (!strcmp(arg, "--dtb") || !strcmp(arg, "-d"))
			dt_dir = val;
		else if (!strcmp(arg, "--output") || !strcmp(arg, "-o"))
			dt_img = val;
		else
			return usage(exec_name);
	}

	if (dt_img == 0) {
		fprintf(stderr,"error: no output filename specified\n");
		return usage(exec_name);
	}

	if (dt_dir == 0) {
		fprintf(stderr,"error: no dtb input directory specified\n");
		return usage(exec_name);
	}

	if (dt_dir) {
		dt_data = load_dtbh_block(dt_dir, pagesize, &dt_size);
		if (dt_data == 0) {
			fprintf(stderr, "error: could not load device tree blobs '%s'\n", dt_dir);
			return 1;
		}
	}

	fd = open(dt_img, O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if(fd < 0) {
		fprintf(stderr,"error: could not create '%s'\n", dt_img);
		return 1;
	}

	if (write(fd, dt_data, dt_size) != dt_size) goto fail;

	close(fd);

	return 0;

fail:
	unlink(dt_img);
	close(fd);
	fprintf(stderr,"error: failed writing '%s': %s\n", dt_img, strerror(errno));

	return 1;
}
