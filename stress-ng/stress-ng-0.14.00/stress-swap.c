/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 * Copyright (C)      2022 Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-capabilities.h"

#if defined(__sun__)
/* Disable for SunOs/Solaris because */
#undef HAVE_SYS_SWAP_H
#endif
#if defined(HAVE_SYS_SWAP_H)
#include <sys/swap.h>
#endif

#define SHIM_EXT2_IOC_GETFLAGS		_IOR('f', 1, long)
#define SHIM_EXT2_IOC_SETFLAGS		_IOW('f', 2, long)
#define SHIM_FS_NOCOW_FL		0x00800000 /* No Copy-on-Write file */

static const stress_help_t help[] = {
	{ NULL,	"swap N",	"start N workers exercising swapon/swapoff" },
	{ NULL,	"swap-ops N",	"stop after N swapon/swapoff operations" },
	{ NULL,	NULL,		NULL }
};

#if defined(HAVE_SYS_SWAP_H) &&	\
    defined(HAVE_SWAP)

#define SWAP_VERSION		(1)
#define SWAP_UUID_LENGTH	(16)
#define SWAP_LABEL_LENGTH	(16)
#define SWAP_SIGNATURE 		"SWAPSPACE2"
#define SWAP_SIGNATURE_SZ	(sizeof(SWAP_SIGNATURE) - 1)

#define MIN_SWAP_PAGES		(32)
#define MAX_SWAP_PAGES		(256)

#if !defined(SWAP_FLAG_PRIO_SHIFT)
#define SWAP_FLAG_PRIO_SHIFT	(0)
#endif
#if !defined(SWAP_FLAG_PRIO_MASK)
#define SWAP_FLAG_PRIO_MASK	(0x7fff)
#endif

#define SWAP_HDR_SANE		(0x01)
#define SWAP_HDR_BAD_SIGNATURE	(0x02)
#define SWAP_HDR_BAD_VERSION	(0x04)
#define SWAP_HDR_ZERO_LAST_PAGE (0x08)
#define SWAP_HDR_BAD_LAST_PAGE	(0x10)
#define SWAP_HDR_BAD_NR_BAD	(0x20)

static const int bad_header_flags[] = {
	SWAP_HDR_BAD_SIGNATURE,
	SWAP_HDR_BAD_VERSION,
	SWAP_HDR_ZERO_LAST_PAGE,
	SWAP_HDR_BAD_LAST_PAGE,
	SWAP_HDR_BAD_NR_BAD,
};

typedef struct {
	uint8_t		bootbits[1024];	/* cppcheck-suppress unusedStructMember */
	uint32_t	version;
	uint32_t	last_page;
	uint32_t	nr_badpages;
	uint8_t		sws_uuid[SWAP_UUID_LENGTH];
	uint8_t		sws_volume[SWAP_LABEL_LENGTH];
	uint32_t	padding[117];	/* cppcheck-suppress unusedStructMember */
	uint32_t	badpages[1];	/* cppcheck-suppress unusedStructMember */
} stress_swap_info_t;

/*
 *  stress_swap_supported()
 *      check if we can run this with SHIM_CAP_SYS_ADMIN capability
 */
static int stress_swap_supported(const char *name)
{
	if (!stress_check_capability(SHIM_CAP_SYS_ADMIN)) {
		pr_inf_skip("%s stressor will be skipped, "
			"need to be running with CAP_SYS_ADMIN "
			"rights for this stressor\n", name);
		return -1;
	}
	return 0;
}

/*
 *  stress_swapoff()
 *	swapoff and retry if EINTR occurs
 */
static int stress_swapoff(const char *path)
{
	int ret, i;

	for (i = 0; i < 25; i++) {
		errno = 0;
		ret = swapoff(path);
		if (ret == 0)
			break;
		if ((ret < 0) && (errno != EINTR))
			break;
	}
	return ret;
}

static int stress_swap_zero(
	const stress_args_t *args,
	const int fd,
	const uint32_t npages,
	const uint8_t *page)
{
	uint32_t i;

	if (lseek(fd, 0, SEEK_SET) < 0) {
		pr_fail("%s: lseek failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return -1;
	}

	for (i = 0; i < npages; i++) {
		if (write(fd, page, args->page_size) < 0) {
			pr_fail("%s: write failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			return -1;
		}
	}
	return 0;
}

static int stress_swap_set_size(
	const stress_args_t *args,
	const int fd,
	const size_t npages,
	const int bad_flags)
{
	char signature[] = SWAP_SIGNATURE;
	stress_swap_info_t swap_info;
	size_t i;

	if (npages < MIN_SWAP_PAGES) {
		pr_fail("%s: incorrect swap size, must be > 16\n", args->name);
		return -1;
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
		pr_fail("%s: lseek failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return -1;
	}

	if (bad_flags & SWAP_HDR_BAD_SIGNATURE)
		signature[0]++;	/* Invalid */

	(void)memset(&swap_info, 0, sizeof(swap_info));
	for (i = 0; i < sizeof(swap_info.sws_uuid); i++)
		swap_info.sws_uuid[i] = stress_mwc8();
	(void)snprintf((char *)swap_info.sws_volume,
		sizeof(swap_info.sws_volume),
		"SNG-SWP-%" PRIx32, args->instance);

	if (bad_flags & SWAP_HDR_BAD_VERSION)
		swap_info.version = ~SWAP_VERSION;	/* Invalid */
	else
		swap_info.version = SWAP_VERSION;

	if (bad_flags & SWAP_HDR_ZERO_LAST_PAGE)
		swap_info.last_page = 0;		/* Invalid */
	else
		swap_info.last_page = npages - 1;

	if (bad_flags & SWAP_HDR_BAD_LAST_PAGE)
		swap_info.last_page = npages + 1;	/* Invalid */
	else
		swap_info.last_page = npages - 1;

	if (bad_flags & SWAP_HDR_BAD_NR_BAD)
		swap_info.nr_badpages = ~0;		/* Dire */
	else
		swap_info.nr_badpages = 0;

	if (write(fd, &swap_info, sizeof(swap_info)) < 0) {
		pr_fail("%s: write of swap info failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return -1;
	}
	if (lseek(fd, (off_t)(args->page_size - SWAP_SIGNATURE_SZ), SEEK_SET) < 0) {
		pr_fail("%s: lseek failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return -1;
	}
	if (write(fd, signature, SWAP_SIGNATURE_SZ) < 0) {
		pr_fail("%s: write of swap signature failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return -1;
	}
	return 0;
}

static void stress_swap_check_swapped(
	void *addr,
	const size_t page_size,
	const size_t npages,
	uint64_t *swapped_out,
	uint64_t *swapped_total)
{
	unsigned char *vec;
	register size_t n = 0;
	register size_t i;

	*swapped_total += npages;

	vec = calloc(npages, sizeof(*vec));
	if (!vec)
		return;

	shim_mincore(addr, page_size * npages, vec);
	for (i = 0; i < npages; i++)
		n += ((vec[i] & 1) == 0);

	*swapped_out += n;
	free(vec);
}

/*
 *  stress_swap()
 *	stress swap operations
 */
static int stress_swap(const stress_args_t *args)
{
	char filename[PATH_MAX];
	int fd, ret;
	uint8_t *page;
	uint64_t swapped_out = 0;
	uint64_t swapped_total = 0;
	const size_t page_size = args->page_size;
	double swapped_percent;

	page = calloc(1, page_size);
	if (!page) {
		pr_err("%s: failed to allocate 1 page: errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		ret = EXIT_NO_RESOURCE;
		goto tidy_ret;
	}

	ret = stress_temp_dir_mk_args(args);
	if (ret < 0) {
		ret = exit_status(-ret);
		goto tidy_free;
	}

	(void)stress_temp_filename_args(args,
		filename, sizeof(filename), stress_mwc32());
	fd = open(filename, O_CREAT | O_RDWR, S_IRUSR);
	if (fd < 0) {
		ret = exit_status(errno);
		pr_fail("%s: open swap file %s failed, errno=%d (%s)\n",
			args->name, filename, errno, strerror(errno));
		goto tidy_rm;
	}

#if defined(__linux__)
	{
		unsigned long flags;

		/*
		 *  Disable Copy-on-Write on file where possible, since
		 *  file systems such as btrfs have CoW enabled by default
		 *  and we swap does not support this feature.
		 */
		if (ioctl(fd, SHIM_EXT2_IOC_GETFLAGS, &flags) == 0) {
			flags |= SHIM_FS_NOCOW_FL;
			ret = ioctl(fd, SHIM_EXT2_IOC_SETFLAGS, &flags);
			(void)ret;
		}
	}
#endif

	if (stress_swap_zero(args, fd, MAX_SWAP_PAGES, page) < 0) {
		ret = EXIT_FAILURE;
		goto tidy_close;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		int swapflags = 0;
		int bad_flags;
		char *ptr;
		size_t npages = (size_t)(stress_mwc32() % (MAX_SWAP_PAGES - MIN_SWAP_PAGES)) +
				  MIN_SWAP_PAGES;
		const size_t mmap_size = npages * page_size;

#if defined(SWAP_FLAG_PREFER)
		if (stress_mwc1()) {
			swapflags = (stress_mwc8() << SWAP_FLAG_PRIO_SHIFT) & SWAP_FLAG_PRIO_MASK;
			swapflags |= SWAP_FLAG_PREFER;
		}
#endif
#if defined(SWAP_FLAG_DISCARD)
		if (stress_mwc1())
			swapflags |= SWAP_FLAG_DISCARD;
#endif
		/* Periodically create bad swap header */
		if (stress_mwc8() < 16) {
			const size_t idx = stress_mwc8() % SIZEOF_ARRAY(bad_header_flags);
			bad_flags = bad_header_flags[idx];
		} else {
			bad_flags = SWAP_HDR_SANE;	/* No bad header */
		}

		if (stress_swap_set_size(args, fd, npages, bad_flags) < 0) {
			ret = EXIT_FAILURE;
			goto tidy_close;
		}
		ret = swapon(filename, swapflags);
		if ((bad_flags == SWAP_HDR_SANE) && (ret < 0)) {
			switch (errno) {
			case EPERM:
			case EINVAL:
				/*
				 * We may hit EPERM if we request
				 * too many swap files
				 */
				pr_inf_skip("%s: cannot enable swap file on the filesystem, skipping test\n",
					args->name);
				ret = EXIT_NO_RESOURCE;
				break;
			case EBUSY:
				continue;
			default:
				pr_fail("%s: swapon failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				ret = EXIT_FAILURE;
				break;
			}
			goto tidy_close;
		}

		ptr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (ptr != MAP_FAILED) {
			size_t i;
			const char *p_end = ptr + mmap_size;
			char *p;

			/* Add simple check value to start of each page */
			for (i = 0, p = ptr; p < p_end; p += page_size, i++) {
				uintptr_t *up = (uintptr_t *)p;

				(void)memset(p, i, page_size);
				*up = (uintptr_t)p;
			}
#if defined(MADV_PAGEOUT)
			(void)shim_madvise(ptr, mmap_size, MADV_PAGEOUT);
#endif
			stress_swap_check_swapped(ptr, page_size, npages,
				&swapped_out, &swapped_total);

			/* Check page has check address value */
			for (i = 0, p = ptr; p < p_end; p += page_size, i++) {
				uintptr_t *up = (uintptr_t *)p;

				if (*up != (uintptr_t)p) {
					pr_fail("%s: failed: address %p contains "
						"%" PRIuPTR " and not %" PRIuPTR "\n",
						args->name, p, *up, (uintptr_t)p);
				}
			}
			(void)munmap(ptr, mmap_size);
		}

		ret = stress_swapoff(filename);
		if ((bad_flags == SWAP_HDR_SANE) && (ret < 0)) {
			pr_fail("%s: swapoff failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			ret = EXIT_FAILURE;
			goto tidy_close;
		}

		/* Exercise illegal swapon filename */
		ret = swapon("", swapflags);
		if (ret == 0)
			ret = stress_swapoff("");	/* Should never happen */
		(void)ret;

		/* Exercise illegal swapoff filename */
		ret = stress_swapoff("");
		if (ret == 0)
			ret = swapon("", swapflags);	/* Should never happen */
		(void)ret;

		/* Exercise illegal swapon flags */
		ret = swapon(filename, ~0);
		if (ret == 0)
			ret = stress_swapoff(filename);	/* Should never happen */
		(void)ret;

		inc_counter(args);
	} while (keep_stressing(args));

	swapped_percent = (swapped_total == 0) ?
		0.0 : (100.0 * (double)swapped_out) / (double)swapped_total;
	pr_inf("%s: %" PRIu64 " of %" PRIu64 " (%.2f%%) pages were swapped out\n",
		args->name, swapped_out, swapped_total, swapped_percent);

	ret = EXIT_SUCCESS;
tidy_close:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)close(fd);
tidy_rm:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)shim_unlink(filename);
	(void)stress_temp_dir_rm_args(args);
tidy_free:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	free(page);
tidy_ret:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	return ret;
}

stressor_info_t stress_swap_info = {
	.stressor = stress_swap,
	.supported = stress_swap_supported,
	.class = CLASS_VM | CLASS_OS,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_swap_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_VM | CLASS_OS,
	.help = help
};
#endif
