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

static const stress_help_t help[] = {
	{ NULL,	"mincore N",	  "start N workers exercising mincore" },
	{ NULL,	"mincore-ops N",  "stop after N mincore bogo operations" },
	{ NULL,	"mincore-random", "randomly select pages rather than linear scan" },
	{ NULL,	NULL,		  NULL }
};

static int stress_set_mincore_rand(const char *opt)
{
	bool mincore_rand = true;

	(void)opt;
	return stress_set_setting("mincore-rand", TYPE_ID_BOOL, &mincore_rand);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_mincore_rand,     stress_set_mincore_rand },
	{ 0,			NULL }
};

#if defined(HAVE_MINCORE) &&	\
    NEED_GLIBC(2,2,0)

#define VEC_MAX_SIZE 	(64)

/*
 *  stress_mincore_file()
 *	create a temp file for file-back mmap'd page, return
 *	fd if successful, -1 if failed.
 */
static int stress_mincore_file(const stress_args_t *args)
{
	int ret, fd;
	char filename[PATH_MAX];
	ret = stress_temp_dir_mk_args(args);
	if (ret != 0)
		return -1;

	(void)stress_temp_filename_args(args, filename,
					sizeof(filename), stress_mwc32());
	fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	(void)shim_unlink(filename);
	if (fd < 0) {
		(void)stress_temp_dir_rm_args(args);
		return -1;
	}
	ret = shim_fallocate(fd, 0, (off_t)0, (off_t)args->page_size);
	if (ret < 0) {
		(void)close(fd);
		(void)stress_temp_dir_rm_args(args);
		return -1;
	}
	return fd;
}

/*
 *  stress_mincore()
 *	stress mincore system call
 */
static int stress_mincore(const stress_args_t *args)
{
	uint8_t *addr = 0, *prev_addr = 0;
	const size_t page_size = args->page_size;
	const intptr_t mask = ~((intptr_t)page_size - 1);
	bool mincore_rand = false;
	int fd, rc = EXIT_SUCCESS;
	uint8_t *mapped, *unmapped, *fdmapped;

	(void)stress_get_setting("mincore-rand", &mincore_rand);

	/* Don't worry if we can't map a page, it is not critical */
	mapped = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	/* Map a file backed page, silently ignore failure */
	fd = stress_mincore_file(args);
	if (fd >= 0) {
		fdmapped = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE, fd, 0);
	} else {
		fdmapped = MAP_FAILED;
	}

	/* Map then unmap a page to get an unmapped page address */
	unmapped = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (unmapped != MAP_FAILED) {
		if (munmap(unmapped, page_size) < 0)
			unmapped = MAP_FAILED;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		int i;

		for (i = 0; (i < 100) && keep_stressing_flag(); i++) {
			int ret, redo = 0;
			unsigned char vec[1];

redo: 			errno = 0;
			ret = shim_mincore((void *)addr, page_size, vec);
			if (ret < 0) {
				switch (errno) {
				case ENOMEM:
					/* Page not mapped */
					break;
				case EAGAIN:
					if (++redo < 100)
						goto redo;
					break;
				case ENOSYS:
					if (args->instance == 0)
						pr_inf_skip("%s: mincore no not implemented, skipping stressor\n",
							args->name);
					rc = EXIT_NOT_IMPLEMENTED;
					goto err;
				default:
					pr_fail("%s: mincore on address %p errno=%d %s\n",
						args->name, (void *)addr, errno,
						strerror(errno));
					rc = EXIT_FAILURE;
					break;
				}
			}
			if (mapped != MAP_FAILED) {
				/* Force page to be resident */
				*mapped = 0xff;
				ret = shim_mincore((void *)mapped, page_size, vec);
				if (ret < 0) {
					/* Should no return ENOMEM on a mapped page */
					if (errno == ENOMEM) {
						pr_fail("%s: mincore on address %p failed, errno=$%d (%s)\n",
							args->name, (void *)mapped, errno,
							strerror(errno));
						rc = EXIT_FAILURE;
					}
				}
			}
			if (fdmapped != MAP_FAILED) {
				/* Force page to be resident */
				*fdmapped = stress_mwc8();
#if defined(MS_ASYNC)
				ret = shim_msync((void *)fdmapped, page_size, MS_ASYNC);
				(void)ret;
#endif
				ret = shim_mincore((void *)fdmapped, page_size, vec);
				if (ret < 0) {
					/* Should no return ENOMEM on a mapped page */
					if (errno == ENOMEM) {
						pr_fail("%s: mincore on address %p failed, errno=$%d (%s)\n",
							args->name, (void *)fdmapped, errno,
							strerror(errno));
						rc = EXIT_FAILURE;
					}
				}
			}
			if (unmapped != MAP_FAILED) {
				/* mincore on unmapped page should fail */
				ret = shim_mincore((void *)unmapped, page_size, vec);
				if (ret == 0) {
					pr_fail("%s: mincore on unmapped address %p should have failed but did not\n",
						args->name, (void *)unmapped);
					rc = EXIT_FAILURE;
				}
			}
			if (mincore_rand) {
				addr = (uint8_t *)(intptr_t)
					(((intptr_t)addr >> 1) & mask);
				if (addr == prev_addr)
					addr = (uint8_t *)(((intptr_t)stress_mwc64()) & mask);
				prev_addr = addr;
			} else {
				addr += page_size;
			}

			/*
			 *  Exercise with zero length, ignore return
			 */
			(void)shim_mincore((void *)addr, 0, vec);

			/*
			 *  Exercise with NULL vec, ignore return
			 */
			(void)shim_mincore((void *)addr, page_size, NULL);

			/*
			 *  Exercise with NULL address
			 */
			(void)shim_mincore(NULL, page_size, vec);

			/*
			 *  Exercise with zero arguments
			 */
			(void)shim_mincore(NULL, 0, NULL);

			/*
			 *  Exercise with invalid page
			 */
			(void)shim_mincore(NULL, page_size, args->mapped->page_none);
		}
		inc_counter(args);
	} while (keep_stressing(args));

err:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	if (fdmapped != MAP_FAILED)
		(void)munmap((void *)fdmapped, page_size);
	if (mapped != MAP_FAILED)
		(void)munmap((void *)mapped, page_size);

	if (fd >= 0) {
		(void)close(fd);
		(void)stress_temp_dir_rm_args(args);
	}
	return rc;
}

stressor_info_t stress_mincore_info = {
	.stressor = stress_mincore,
	.class = CLASS_OS | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_mincore_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_OS | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
#endif
