/*
 * Copyright (C) 2021-2022 Colin Ian King
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
	{ NULL,	"resched N",		"start N workers that spawn renicing child processes" },
	{ NULL,	"resched-ops N",	"stop after N nice bogo nice'd yield operations" },
	{ NULL,	NULL,			NULL }
};

#if defined(HAVE_NICE)

#if (defined(_POSIX_PRIORITY_SCHEDULING) || defined(__linux__)) &&	     \
    (defined(SCHED_OTHER) || defined(SCHED_BATCH) || defined(SCHED_IDLE)) && \
     !defined(__OpenBSD__) &&						     \
     !defined(__minix__) &&						     \
     !defined(__APPLE__)
#define HAVE_SCHEDULING
#endif

static void stress_resched_child(
	const stress_args_t *args,
	const int niceness,
	const int max_niceness,
	uint64_t *yields)
{
	int i;
#if defined(HAVE_SCHEDULING)
	const pid_t pid = getpid();
#endif

#if defined(HAVE_SCHEDULING)
	/*
	 *  "Normal" non-realtime scheduling policies
	 */
	static const int normal_policies[] = {
#if defined(SCHED_OTHER)
		SCHED_OTHER,
#endif
#if defined(SCHED_BATCH)
		SCHED_BATCH,
#endif
#if defined(SCHED_IDLE)
		SCHED_IDLE,
#endif
	};
#endif

	(void)setpgid(0, g_pgrp);
	stress_parent_died_alarm();
	(void)sched_settings_apply(true);

	for (i = niceness; i < max_niceness; i++) {
		int ret, k;

		for (k = 0; k < 1024; k++) {
#if defined(HAVE_SCHEDULING)
			size_t j;

			for (j = 0; j < SIZEOF_ARRAY(normal_policies); j++) {
				struct sched_param param;

				(void)memset(&param, 0, sizeof(param));
				param.sched_priority = 0;
				ret = sched_setscheduler(pid, normal_policies[j], &param);
				(void)ret;
				ret = shim_sched_yield();
				(void)ret;
				if (yields)
					yields[i]++;
				inc_counter(args);
			}
#else
			ret = shim_sched_yield();
			(void)ret;
			if (yields)
				yields[i]++;
			inc_counter(args);
#endif
		}

		ret = nice(1);
		(void)ret;

		if (!keep_stressing(args))
			break;
	}
	_exit(0);
}

/*
 *  stress_resched_spawn()
 *	start a child process that will re-nice itself
 */
static void stress_resched_spawn(
	const stress_args_t *args,
	pid_t *pids,
	const int index,
	const int max_prio,
	uint64_t *yields)
{
	pid_t pid;

	pids[index] = -1;

	pid = fork();
	if (pid == 0) {
		stress_resched_child(args, index, max_prio, yields);
	} else if (pid > 0) {
		pids[index] = pid;
	}
}

/*
 *  stress on sched_resched()
 *	stress system by sched_resched
 */
static int stress_resched(const stress_args_t *args)
{
	pid_t *pids;

#if defined(HAVE_SETPRIORITY)
	int i, pids_max, max_prio = 19;
	size_t yields_size;
	uint64_t *yields;

#if defined(RLIMIT_NICE)
	{
		struct rlimit rlim;

		if (getrlimit(RLIMIT_NICE, &rlim) == 0) {
			max_prio = 20 - (int)rlim.rlim_cur;
		}
	}
#endif
#endif
	pids_max = max_prio + 1; /* 0.. max_prio */
	pids = calloc(pids_max, sizeof(*pids));
	if (!pids) {
		pr_inf("%s: cannot allocate pids array, skipping stressor, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}
	yields_size = ((sizeof(*yields) * pids_max) + args->page_size - 1) & ~(args->page_size - 1);
	yields = (uint64_t *)mmap(NULL, yields_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (yields == MAP_FAILED)
		yields = NULL;

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	/* Start off one child process per positive nice level */
	for (i = 0; i < pids_max; i++)
		stress_resched_spawn(args, pids, i, max_prio, yields);

	do {
		pid_t pid;
		int status;

		/* Wait for child processes to die */
		pid = wait(&status);
		if (pid >= 0) {
			/*
			 *  Find unstrated process or process that just terminated
			 *  and respawn it at the nice level of the given slot
			 */
			for (i = 0; i < pids_max; i++) {
				if ((pids[i] == -1) || (pid == pids[i]))
					stress_resched_spawn(args, pids, i, max_prio, yields);
			}
		}
	} while (keep_stressing(args));

	/* Kill children */
	for (i = 0; i < pids_max; i++) {
		if (pids[i] != -1)
			(void)kill(pids[i], SIGKILL);
	}

	/* Reap children */
	for (i = 0; i < pids_max; i++) {
		if (pids[i] != -1) {
			int status, ret;

			ret = waitpid(pids[i], &status, 0);
			(void)ret;
		}
	}

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	/*
	 *  Dump stats for just instance 0 to reduce output
	 */
	if ((yields != NULL) && (args->instance == 0)) {
		uint64_t total_yields = 0;
		bool lock = false;

		for (i = 0; i < pids_max; i++)
			total_yields += yields[i];

		pr_lock(&lock);
		for (i = 0; i < pids_max; i++) {
			if (yields[i] > 0) {
				double percent = 100.0 * ((double)yields[i] / (double)total_yields);

				if (i == 0) {
					pr_dbg_lock(&lock, "%s: prio %2d: %5.2f%% yields\n",
						args->name, i, percent);
				} else {
					double scale = (double)yields[i] / (double)yields[i - 1];

					pr_dbg_lock(&lock, "%s: prio %2d: %5.2f%% yields (prio %2d x %f)%s\n",
						args->name, i, percent, i - 1, scale,
						(scale < 1.0) ? " bad" : "");
				}
			}
		}
		pr_unlock(&lock);
	}

	if (yields != NULL)
		(void)munmap((void *)yields, yields_size);
	free(pids);

	return EXIT_SUCCESS;
}

stressor_info_t stress_resched_info = {
	.stressor = stress_resched,
	.class = CLASS_SCHEDULER | CLASS_OS,
	.help = help
};

#else
stressor_info_t stress_resched_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_SCHEDULER | CLASS_OS,
	.help = help
};
#endif
