#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#define BTRFS_IOCTL_MAGIC 0x94

#define BTRFS_IOC_FILE_EXTENT_SAME _IOWR(BTRFS_IOCTL_MAGIC, 54, \
					 struct btrfs_ioctl_same_args)

#define BTRFS_SAME_DATA_DIFFERS	1
/* For extent-same ioctl */
struct btrfs_ioctl_same_extent_info {
	int64_t fd;			/* in - destination file */
	uint64_t logical_offset;	/* in - start of extent in destination */
	uint64_t bytes_deduped;		/* out - total # of bytes we
					 * were able to dedupe from
					 * this file */
	/* status of this dedupe operation:
	 * 0 if dedup succeeds
	 * < 0 for error
	 * == BTRFS_SAME_DATA_DIFFERS if data differs
	 */
	int32_t status;			/* out - see above description */
	uint32_t reserved;
};

struct btrfs_ioctl_same_args {
	uint64_t logical_offset;	/* in - start of extent in source */
	uint64_t length;		/* in - length of extent */
	uint16_t total_files;		/* in - total elements in info array */
	uint16_t files_deduped;		/* out - number of files that got deduped */
	uint32_t reserved;
	struct btrfs_ioctl_same_extent_info info[0];
};

static void usage(const char *prog)
{
	printf("Usage: %s len file1 loff1 file2 loff2\n", prog);
}

int main(int argc, char **argv)
{
	int ret, src_fd, i, numfiles;
	char *srcf, *destf;
	struct btrfs_ioctl_same_args *same;
	struct btrfs_ioctl_same_extent_info *info;
	unsigned long long bytes = 0ULL;

	if (argc < 6 || (argc % 2)) {
		usage(argv[0]);
		return 1;
	}

	numfiles = (argc / 2) - 2;

	printf("Deduping %d total files\n", numfiles + 1);

	same = calloc(1,
		      sizeof(struct btrfs_ioctl_same_args) +
		      sizeof(struct btrfs_ioctl_same_extent_info) * numfiles);
	if (!same)
		return -ENOMEM;

	srcf = argv[2];
	same->length = atoll(argv[1]);
	same->logical_offset = atoll(argv[3]);
	same->total_files = numfiles;

	ret = open(srcf, O_RDONLY);
	if (ret < 0) {
		ret = errno;
		fprintf(stderr, "Could not open file %s: (%d) %s\n", srcf, ret,
			strerror(ret));
		return -ret;
	}
	src_fd = ret;

	printf("(%llu, %llu): %s\n", (unsigned long long)same->logical_offset,
	       (unsigned long long)same->length, srcf);

	for (i = 0; i < same->total_files; i++) {
		destf = argv[4 + (i * 2)];

		ret = open(destf, O_RDONLY);
		if (ret < 0) {
			ret = errno;
			fprintf(stderr, "Could not open file %s: (%d) %s\n",
				destf, ret, strerror(ret));
			return -ret;
		}

		same->info[i].fd = ret;
		same->info[i].logical_offset = atoll(argv[5 + (i * 2)]);
		printf("(%llu, %llu): %s\n",
		       (unsigned long long)same->info[i].logical_offset,
		       (unsigned long long)same->length, destf);

	}

	ret = ioctl(src_fd, BTRFS_IOC_FILE_EXTENT_SAME, same);
	if (ret < 0) {
		ret = errno;
		fprintf(stderr, "btrfs_same returned error: (%d) %s\n", ret,
			strerror(ret));
		return -ret;
	}

	printf("%u of %u files deduped\n", same->files_deduped, same->total_files);

	for (i = 0; i < same->total_files; i++) {
		info = &same->info[i];

		printf("i: %d, status: %d, bytes_deduped: %llu\n", i,
		       info->status, (unsigned long long)info->bytes_deduped);

		bytes += info->bytes_deduped;
	}

	printf("%llu total bytes deduped in this operation\n", bytes);

	return ret;
}