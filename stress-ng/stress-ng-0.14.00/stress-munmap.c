/*
 * Copyright (C)      2021 Canonical, Ltd.
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

typedef struct {
	const stress_args_t *args;	/* stress-ng arguments */
	size_t page_shift;		/* log2(page_size) */
	char exe_path[PATH_MAX];	/* name of executable */
} munmap_context_t;

static const stress_help_t help[] = {
	{ NULL,	"munmap N",	 "start N workers stressing munmap" },
	{ NULL,	"munmap-ops N",	 "stop after N munmap bogo operations" },
	{ NULL,	NULL,		 NULL }
};

#if defined(__linux__)
/*
 *  stress_munmap_log2()
 *	slow but simple log to the base 2 of n
 */
static inline size_t stress_munmap_log2(size_t n)
{
	register size_t l2;

	for (l2 = 0; n > 1; l2++)
		n >>= 1;

	return l2;
}

/*
 *  stress_munmap_stride()
 *	find a prime that is greater than n and not
 *	a multiple of n for a page unmapping stride
 */
static size_t stress_munmap_stride(const size_t n)
{
	register size_t p;

	for (p = n + 1; ; p++) {
		if ((n % p) && stress_is_prime64((uint64_t)p))
			return p;
	}
}

/*
 *  stress_munmap_range()
 *	unmap a mmap'd region using a prime sized stride across the
 *	mmap'd region to create lots of temporary mapping holes.
 */
static void stress_munmap_range(
	const stress_args_t *args,
	void *start,
	void *end,
	const size_t page_shift)
{
	const size_t page_size = args->page_size;
	const size_t size = (uintptr_t)end - (uintptr_t)start;
	const size_t n_pages = size / page_size;
	const size_t stride = stress_munmap_stride(n_pages + stress_mwc8());
	size_t i, j;

	for (i = 0, j = 0; keep_stressing(args) && (i < n_pages); i++) {
		const size_t offset = j << page_shift;
		void *addr = ((uint8_t *)start) + offset;

		if (munmap(addr, page_size) == 0)
			inc_counter(args);
		j += stride;
		j %= n_pages;
	}
}

/*
 *  stress_munmap_sig_handler()
 *	signal handler to immediately terminates
 */
static void NORETURN MLOCKED_TEXT stress_munmap_sig_handler(int num)
{
	(void)num;

	_exit(0);
}

/*
 *  stress_munmap_child()
 *	child process that attempts to unmap a lot of the
 *	pages mapped into stress-ng without killing itself with
 *	a bus error or segmentation fault.
 */
static int stress_munmap_child(const stress_args_t *args, void *context)
{
	FILE *fp;
	char path[PATH_MAX];
	char buf[4096], prot[5];
	const pid_t pid = getpid();
	munmap_context_t *ctxt = (munmap_context_t *)context;
	void *start, *end, *offset;
	int ret, major, minor, n;
	uint64_t inode;

	ret = stress_sighandler(args->name, SIGSEGV, stress_munmap_sig_handler, NULL);
	(void)ret;
	ret = stress_sighandler(args->name, SIGBUS, stress_munmap_sig_handler, NULL);
	(void)ret;

	(void)snprintf(path, sizeof(path), "/proc/%" PRIdMAX "/maps", (intmax_t)pid);
	fp = fopen(path, "r");
	if (!fp)
		return EXIT_NO_RESOURCE;
#if defined(HAVE_MADVISE) &&	\
    defined(MADV_DONTDUMP)
	/*
	 *  Vainly attempt to reduce any potential core dump size
	 */
	while (keep_stressing(args) && fgets(buf, sizeof(buf), fp)) {
		size_t size;

		*path = '\0';
		n = sscanf(buf, "%p-%p %4s %p %x:%x %" PRIu64 " %s\n",
			&start, &end, prot, &offset, &major, &minor,
			&inode, path);
		if (n < 7)
			continue;	/* bad sscanf data */
		if (start >= end)
			continue;	/* invalid address range */
		size = (uintptr_t)end - (uintptr_t)start;
		(void)madvise(start, size, MADV_DONTDUMP);
	}
	(void)rewind(fp);
#endif
	while (keep_stressing(args) && fgets(buf, sizeof(buf), fp)) {
		*path = '\0';
		n = sscanf(buf, "%p-%p %4s %p %x:%x %" PRIu64 " %s\n",
			&start, &end, prot, &offset, &major, &minor,
			&inode, path);
		/*
		 *  Filter out mappings that we should avoid from
		 *  unmapping
		 */
		if (n < 7)
			continue;	/* bad sscanf data */
		if (start >= end)
			continue;	/* invalid address range */
		if (start == context)
			continue;	/* don't want to unmap shared context */
		if (((const void *)args >= start) && ((const void *)args < end))
			continue;	/* don't want to unmap shard args */
		if (!path[0])
			continue;	/* don't unmap anonymous mappings */
		if (path[0] == '[')
			continue;	/* don't unmap special mappings (stack, vdso etc) */
		if (strstr(path, "libc"))
			continue;	/* don't unmap libc */
		if (strstr(path, "/dev/zero"))
			continue;	/* need this for zero'd page data */
		if (!strcmp(path, ctxt->exe_path))
			continue;	/* don't unmap stress-ng */
		if (prot[0] != 'r')
			continue;	/* don't unmap non-readable pages */
		if (prot[2] == 'x')
			continue;	/* don't unmap executable pages */
		stress_munmap_range(args, start, end, ctxt->page_shift);
	}
	(void)fclose(fp);

	if (keep_stressing(args))
		inc_counter(args);	/* bump per stressor */

	return EXIT_SUCCESS;
}

static inline void stress_munmap_clean_path(char *path)
{
	char *ptr = path;

	while (*ptr) {
		if (isspace((int)*ptr)) {
			*ptr = '\0';
			break;
		}
		ptr++;
	}
}

/*
 *  stress_munmap()
 *	stress munmap
 */
static int stress_munmap(const stress_args_t *args)
{
	ssize_t len;
	munmap_context_t *ctxt;

	ctxt = mmap(NULL, sizeof(*ctxt), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (ctxt == MAP_FAILED) {
		pr_inf("%s: skipping stressor, cannot mmap context buffer, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}
	ctxt->args = args;
	ctxt->page_shift = stress_munmap_log2(args->page_size);

	len = shim_readlink("/proc/self/exe", ctxt->exe_path, sizeof(ctxt->exe_path));
        if ((len < 0) || (len > PATH_MAX)) {
		pr_inf("%s: skipping stressor, cannot determine child executable path\n",
			args->name);
		(void)munmap((void *)ctxt, sizeof(*ctxt));
		return EXIT_NO_RESOURCE;
	}
	stress_munmap_clean_path(ctxt->exe_path);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
	while (keep_stressing(args)) {
		int ret;

		ret = stress_oomable_child(args, (void *)ctxt, stress_munmap_child, STRESS_OOMABLE_QUIET);
		(void)ret;
	}
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)ctxt, sizeof(*ctxt));

	return EXIT_SUCCESS;
}

stressor_info_t stress_munmap_info = {
	.stressor = stress_munmap,
	.class = CLASS_VM | CLASS_OS,
	.help = help
};

#else
stressor_info_t stress_munmap_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_VM | CLASS_OS,
        .help = help
};
#endif
