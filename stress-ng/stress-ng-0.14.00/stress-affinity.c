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

#define STRESS_AFFINITY_PROCS	(16)

typedef struct {
	volatile uint32_t cpu;		/* Pinned CPU to use, in pin mode */
	uint32_t cpus;			/* Number of CPUs available */
	bool	 affinity_rand;		/* True if --affinity-rand set */
	bool	 affinity_pin;		/* True if --affinity-pin set */
	uint64_t affinity_delay;	/* Affinity nanosecond delay, 0 default */
	uint64_t affinity_sleep;	/* Affinity nanosecond delay, 0 default */
	uint64_t counters[0];		/* Child stressor bogo counters */
} stress_affinity_info_t;

static const stress_help_t help[] = {
	{ NULL,	"affinity N",	  "start N workers that rapidly change CPU affinity" },
	{ NULL,	"affinity-ops N", "stop after N affinity bogo operations" },
	{ NULL,	"affinity-rand",  "change affinity randomly rather than sequentially" },
	{ NULL, "affinity-delay", "delay in nanoseconds between affinity changes" },
	{ NULL, "affinity-pin",   "keep per stressor threads pinned to same CPU" },
	{ NULL,	"affinity-sleep", "sleep in nanoseconds between affinity changes" },
	{ NULL,	NULL,		  NULL }
};

static int stress_set_affinity_delay(const char *opt)
{
	uint64_t affinity_delay;

	affinity_delay = stress_get_uint64(opt);
	stress_check_range("affinity-delay", affinity_delay,
		0, STRESS_NANOSECOND);
	return stress_set_setting("affinity-delay", TYPE_ID_UINT64, &affinity_delay);
}

static int stress_set_affinity_rand(const char *opt)
{
	bool affinity_rand = true;

	(void)opt;
	return stress_set_setting("affinity-rand", TYPE_ID_BOOL, &affinity_rand);
}

static int stress_set_affinity_pin(const char *opt)
{
	bool affinity_pin = true;

	(void)opt;
	return stress_set_setting("affinity-pin", TYPE_ID_BOOL, &affinity_pin);
}

static int stress_set_affinity_sleep(const char *opt)
{
	uint64_t affinity_sleep;

	affinity_sleep = stress_get_uint64(opt);
	stress_check_range("affinity-sleep", affinity_sleep,
		0, STRESS_NANOSECOND);
	return stress_set_setting("affinity-sleep", TYPE_ID_UINT64, &affinity_sleep);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_affinity_delay,	stress_set_affinity_delay },
	{ OPT_affinity_pin,	stress_set_affinity_pin },
	{ OPT_affinity_rand,    stress_set_affinity_rand },
	{ OPT_affinity_sleep,   stress_set_affinity_sleep },
	{ 0,			NULL }
};

/*
 *  stress on sched_affinity()
 *	stress system by changing CPU affinity periodically
 */
#if defined(HAVE_AFFINITY) && \
    defined(HAVE_SCHED_GETAFFINITY)

/*
 *  stress_affinity_supported()
 *      check that we can set affinity
 */
static int stress_affinity_supported(const char *name)
{
	cpu_set_t mask;

	CPU_ZERO(&mask);

	if (sched_getaffinity(0, sizeof(mask), &mask) < 0) {
		pr_inf_skip("%s stressor cannot get CPU affinity, skipping the stressor\n", name);
		return -1;
	}
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
		if (errno == EPERM) {
			pr_inf_skip("%s stressor cannot set CPU affinity, "
			       "process lacks privilege, skipping the stressor\n", name);
			return -1;
		}
	}
	return 0;
}

/*
 *  stress_affinity_reap()
 *	kill and wait on child processes
 */
static void stress_affinity_reap(const pid_t *pids)
{
	size_t i;
	const pid_t mypid = getpid();

	/*
	 *  Kill and reap children
	 */
	for (i = 1; i < STRESS_AFFINITY_PROCS; i++) {
		if ((pids[i] > 1) && (pids[i] != mypid))
			(void)kill(pids[i], SIGKILL);
	}
	for (i = 1; i < STRESS_AFFINITY_PROCS; i++) {
		if ((pids[i] > 1) && (pids[i] != mypid)) {
			int status;

			(void)waitpid(pids[i], &status, 0);
		}
	}
}

/*
 *  stress_affinity_racy_count()
 *	racy bogo op counter, we have a lot of contention
 *	if we lock the args->counter, so sum per-process
 *	counters in a racy way.
 */
static uint64_t stress_affinity_racy_count(const uint64_t *counters)
{
	register uint64_t count = 0;
	register size_t i;

	for (i = 0; i < STRESS_AFFINITY_PROCS; i++)
		count += counters[i];

	return count;
}

/*
 *  affinity_keep_stressing(args)
 *	check if SIGALRM has triggered to the bogo ops count
 *	has been reached, counter is racy, but that's OK
 */
static bool HOT OPTIMIZE3 affinity_keep_stressing(
	const stress_args_t *args,
	uint64_t *counters)
{
	return (LIKELY(g_keep_stressing_flag) &&
		LIKELY(!args->max_ops ||
                (stress_affinity_racy_count(counters) < args->max_ops)));
}

/*
 *  stress_affinity_spin_delay()
 *	delay by delay nanoseconds, spinning on rescheduling
 *	eat cpu cycles.
 */
static inline void stress_affinity_spin_delay(
	const uint64_t delay,
	stress_affinity_info_t *info)
{
	const uint32_t cpu = info->cpu;
	const double end = stress_time_now() +
		((double)delay / (double)STRESS_NANOSECOND);

	while ((stress_time_now() < end) && (cpu == info->cpu))
		shim_sched_yield();
}

/*
 *  stress_affinity_child()
 *	affinity stressor child process
 */
static void stress_affinity_child(
	const stress_args_t *args,
	stress_affinity_info_t *info,
	const pid_t *pids,
	const size_t instance,
	const bool pin_controller)
{
	uint32_t cpu = args->instance;
	cpu_set_t mask0;
	uint64_t *counters = info->counters;

	CPU_ZERO(&mask0);

	do {
		cpu_set_t mask;
		int ret;

		cpu = info->affinity_rand ? (stress_mwc32() >> 4) : cpu + 1;
		cpu %= info->cpus;

		/*
		 *  In pin mode stressor instance 0 controls the CPU
		 *  to use, other instances use that CPU too
		 */
		if (info->affinity_pin) {
			if (pin_controller) {
				info->cpu = cpu;
				shim_mb();
			} else {
				shim_mb();
				cpu = info->cpu;
			}
		}
		CPU_ZERO(&mask);
		CPU_SET(cpu, &mask);
		if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
			if (errno == EINVAL) {
				/*
				 * We get this if CPU is offline'd,
				 * and since that can be dynamically
				 * set, we should just retry
				 */
				continue;
			}
			pr_fail("%s: failed to move to CPU %" PRIu32 ", errno=%d (%s)\n",
				args->name, cpu, errno, strerror(errno));
			(void)shim_sched_yield();
		} else {
			/* Now get and check */
			CPU_ZERO(&mask);
			CPU_SET(cpu, &mask);
			if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
				if ((g_opt_flags & OPT_FLAGS_VERIFY) &&
				    (!CPU_ISSET(cpu, &mask)))
					pr_fail("%s: failed to move " "to CPU %" PRIu32 "\n",
						args->name, cpu);
			}
		}
		/* Exercise getaffinity with invalid pid */
		ret = sched_getaffinity(-1, sizeof(mask), &mask);
		(void)ret;

		/* Exercise getaffinity with mask size */
		ret = sched_getaffinity(0, 0, &mask);
		(void)ret;

		/* Exercise setaffinity with invalid mask size */
		ret = sched_setaffinity(0, 0, &mask);
		(void)ret;

		/* Exercise setaffinity with invalid mask */
		ret = sched_setaffinity(0, sizeof(mask), &mask0);
		(void)ret;

		counters[instance]++;

		if (info->affinity_delay > 0)
			stress_affinity_spin_delay(info->affinity_delay, info);
		if (info->affinity_sleep > 0)
			shim_nanosleep_uint64(info->affinity_sleep);
	} while (affinity_keep_stressing(args, counters));

	stress_affinity_reap(pids);
}

static int stress_affinity(const stress_args_t *args)
{
	pid_t pids[STRESS_AFFINITY_PROCS];
	size_t i;
	stress_affinity_info_t *info;
	const size_t counters_sz = sizeof(info->counters[0]) * STRESS_AFFINITY_PROCS;
	const size_t info_sz = ((sizeof(*info) + counters_sz) + args->page_size) & ~(args->page_size - 1);

	info = (stress_affinity_info_t *)mmap(NULL, info_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (info == MAP_FAILED) {
		pr_inf_skip("%s: cannot mmap %zd bytes for shared counters, skipping stressor\n",
			args->name, info_sz);
		return EXIT_NO_RESOURCE;
	}

	(void)memset(pids, 0, sizeof(pids));

	info->affinity_delay = 0;
	info->affinity_pin = false;
	info->affinity_rand = false;
	info->affinity_sleep = 0;
	info->cpus = (uint32_t)stress_get_processors_configured();

	(void)stress_get_setting("affinity-delay", &info->affinity_delay);
	(void)stress_get_setting("affinity-pin", &info->affinity_pin);
	(void)stress_get_setting("affinity-rand", &info->affinity_rand);
	(void)stress_get_setting("affinity-sleep", &info->affinity_sleep);

	/*
	 *  process slots 1..STRESS_AFFINITY_PROCS are the children,
	 *  slot 0 is the parent.
	 */
	for (i = 1; i < STRESS_AFFINITY_PROCS; i++) {
		pids[i] = fork();

		if (pids[i] == 0) {
			stress_affinity_child(args, info, pids, i, false);
			_exit(EXIT_SUCCESS);
		}
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
	stress_affinity_child(args, info, pids, 0, true);
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	/*
	 *  The first process to hit the bogo op limit or get a SIGALRM
	 *  will have reap'd the processes, but to be safe, reap again
	 *  to ensure all processes are really dead and reaped.
	 */
	stress_affinity_reap(pids);

	/*
	 *  Set counter, this is always going to be >= the bogo_ops
	 *  threshold because it is racy, but that is OK
	 */
	set_counter(args, stress_affinity_racy_count(info->counters));

	(void)munmap((void *)info, info_sz);

	return EXIT_SUCCESS;
}

stressor_info_t stress_affinity_info = {
	.stressor = stress_affinity,
	.class = CLASS_SCHEDULER,
	.supported = stress_affinity_supported,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_OPTIONAL,
	.help = help,
};
#else
stressor_info_t stress_affinity_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_SCHEDULER,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
#endif
