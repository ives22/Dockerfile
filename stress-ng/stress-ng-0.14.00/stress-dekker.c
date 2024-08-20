/*
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
#include "core-cache.h"

static const stress_help_t help[] = {
	{ NULL,	"dekker N",		"start N workers that exercise ther Dekker algorithm" },
	{ NULL,	"dekker-ops N",		"stop after N dekker mutex bogo operations" },
	{ NULL,	NULL,		NULL }
};

#if defined(HAVE_SHIM_MFENCE)

typedef struct dekker {
	volatile bool	wants_to_enter[2];
	volatile int	turn;
	volatile int check;
} dekker_t;

dekker_t *dekker;

static void stress_dekker_p0(const stress_args_t *args)
{
	int check0, check1;

	dekker->wants_to_enter[0] = true;
	shim_mfence();
	while (LIKELY(dekker->wants_to_enter[1])) {
		if (dekker->turn != 0) {
			dekker->wants_to_enter[0] = false;
			shim_mfence();
			while (dekker->turn != 0) {
			}
			dekker->wants_to_enter[0] = true;
			shim_mfence();
		}
	}

	/* Critical section */
	check0 = dekker->check;
	dekker->check++;
	check1 = dekker->check;

	dekker->turn = 1;
	dekker->wants_to_enter[0] = false;
	shim_mfence();

	if (check0 + 1 != check1) {
		pr_fail("%s p0: dekker mutex check failed %d vs %d\n",
			args->name, check0 + 1, check1);
	}
}

static void stress_dekker_p1(const stress_args_t *args)
{
	int check0, check1;

	dekker->wants_to_enter[1] = true;
	shim_mfence();
	while (LIKELY(dekker->wants_to_enter[0])) {
		if (dekker->turn != 1) {
			dekker->wants_to_enter[1] = false;
			shim_mfence();
			while (dekker->turn != 1) {
			}
			dekker->wants_to_enter[1] = true;
			shim_mfence();
		}
	}

	/* Critical section */
	check0 = dekker->check;
	dekker->check--;
	check1 = dekker->check;
	inc_counter(args);

	dekker->turn = 0;
	dekker->wants_to_enter[1] = false;
	shim_mfence();

	if (check0 - 1 != check1) {
		pr_fail("%s p1: dekker mutex check failed %d vs %d\n",
			args->name, check0 - 1, check1);
	}
}

/*
 *  stress_dekker()
 *	stress dekker algorithm
 */
static int stress_dekker(const stress_args_t *args)
{
	const size_t sz = STRESS_MAXIMUM(args->page_size, sizeof(*dekker));
	pid_t pid;

	dekker = (dekker_t *)mmap(NULL, sz, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (dekker == MAP_FAILED) {
		pr_inf_skip("%s: cannot mmap %zd bytes for bekker shared struct, skipping stressor\n",
			args->name, sz);
		return EXIT_NO_RESOURCE;
	}
	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	pid = fork();
	if (pid < 0) {
		pr_inf_skip("%s: cannot create child process, skipping stressor\n", args->name);
		return EXIT_NO_RESOURCE;
	} else if (pid == 0) {
		/* Child */
		while (keep_stressing(args))
			stress_dekker_p0(args);
		_exit(0);
	} else {
		int status;

		/* Parent */
		while (keep_stressing(args))
			stress_dekker_p1(args);
		(void)kill(pid, SIGKILL);
		(void)waitpid(pid, &status, 0);
	}

	(void)munmap((void *)dekker, 4096);

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)munmap((void *)dekker, sz);

	return EXIT_SUCCESS;
}

stressor_info_t stress_dekker_info = {
	.stressor = stress_dekker,
	.class = CLASS_CPU_CACHE,
	.verify = VERIFY_ALWAYS,
	.help = help
};

#else

stressor_info_t stress_dekker_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_CPU,
	.verify = VERIFY_ALWAYS,
	.help = help
};

#endif
