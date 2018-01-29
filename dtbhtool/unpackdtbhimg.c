#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_PAGE_SIZE 2048
#define DT_NAME_LEN 64

struct dt_entry {
    uint32_t chip;
    uint32_t platform;
    uint32_t subtype;
    uint32_t hw_rev;
    uint32_t hw_rev_end;
    uint32_t offset;
    uint32_t size; /* including padding */
    uint32_t space;
};

struct dtbh_header {
	uint32_t magic;
	uint32_t version;
	uint32_t entry_count;

	struct dt_entry entries[];
};

static void show_dt_entry(struct dt_entry *ent) {
	printf("chip: %u, platform: 0x%x, subtype: 0x%x, hw_rev: %u, hw_rev_end: %u, offset: %u, "
			"size: %u, space: %u\n", ent->chip, ent->platform, ent->subtype, ent->hw_rev,
			ent->hw_rev_end, ent->offset, ent->size, ent->space);
}

int main(int argc, char *argv[]) {
	int fd, outfd;
	ssize_t len;
	int page_size = DEFAULT_PAGE_SIZE;
	uint32_t i;
	struct dtbh_header *header;
	void *dt_blob;
	char buf[DT_NAME_LEN];
	if (argc < 2) {
		fprintf(stderr, "Usage: %s /path/to/dtb.img [page_size]\n", argv[0]);
		fprintf(stderr, "page_size defaults to %d\n", DEFAULT_PAGE_SIZE);
		return 1;
	}

	if (argc >= 3)
		page_size = atoi(argv[2]);

	printf("using page_size: %d\n", page_size);

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Couldn't open '%s': %s\n", argv, strerror(errno));
		return 1;
	}

	header = calloc(page_size, 1);	
	if (header == NULL) {
		goto err_calloc;
	}

	len = read(fd, header, page_size);
	if (len < page_size) {
		if (len >= 0)
			fprintf(stderr, "Failed to read DTBH header (file not long enough)\n");
		else
			fprintf(stderr, "Failed to read DTBH header: %s\n", strerror(errno));
		goto err_read;
	}

	printf("DTBH_MAGIC = '%.4s'\n", &header->magic);

	printf("DTBH_VERSION = %u\n", header->version);

	printf("Number of entries: %u\n", header->entry_count);

	for (i = 0; i < header->entry_count; i++) {
		printf("DTB %d: ", i);
		show_dt_entry(&header->entries[i]);
		dt_blob = malloc(header->entries[i].size);
		if (lseek(fd, header->entries[i].offset, SEEK_SET) < 1) {
			fprintf(stderr, "failed to set offset to 0x%x: %s\n", header->entries[i].offset,
					strerror(errno));
			goto err_loop;
		}
		len = read(fd, dt_blob, header->entries[i].size);
		if (len < header->entries[i].size) {
			if (len >= 0)
				fprintf(stderr, "Failed to read DTB - file too short!\n");
			else
				fprintf(stderr, "Failed to read DTB: %s\n", strerror(errno));
		}

		snprintf(buf, DT_NAME_LEN, "chip%u-0x%x-0x%x_rev%u-%u.dtb", header->entries[i].chip,
				header->entries[i].platform, header->entries[i].subtype, header->entries[i].hw_rev,
				header->entries[i].hw_rev_end);
		outfd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
		if (outfd < 0) {
			fprintf(stderr, "Failed to open '%s': %s\n", buf, strerror(errno));
			free(dt_blob);
			continue;
		}

		if (write(outfd, dt_blob, header->entries[i].size) < 0) {
			fprintf(stderr, "Failed to write to '%s': %s\n", buf, strerror(errno));
			close(outfd);
			free(dt_blob);
			continue;
		}

		close(outfd);
		free(dt_blob);
		printf("Dumped to '%s'\n", buf);
	}

	free(header);
	close(fd);

	return 0;
err_loop:
	free(dt_blob);
err_read:
	free(header);
err_calloc:
	close(fd);
	return 1;
}
