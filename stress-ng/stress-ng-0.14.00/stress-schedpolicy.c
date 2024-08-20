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

#if defined(__NR_sched_getattr)
#define HAVE_SCHED_GETATTR
#endif

#if defined(__NR_sched_setattr)
#define HAVE_SCHED_SETATTR
#endif

static const stress_help_t help[] = {
	{ NULL,	"schedpolicy N",	"start N workers that exercise scheduling policy" },
	{ NULL,	"schedpolicy-ops N",	"stop after N scheduling policy bogo operations" },
	{ NULL,	NULL,			NULL }
};

#if (defined(_POSIX_PRIORITY_SCHEDULING) || defined(__linux__)) &&	\
     !defined(__OpenBSD__) &&						\
     !defined(__minix__) &&						\
     !defined(__APPLE__)

static const int policies[] = {
#if defined(SCHED_IDLE)
	SCHED_IDLE,
#endif
#if defined(SCHED_FIFO)
	SCHED_FIFO,
#endif
#if defined(SCHED_RR)
	SCHED_RR,
#endif
#if defined(SCHED_OTHER)
	SCHED_OTHER,
#endif
#if defined(SCHED_BATCH)
	SCHED_BATCH,
#endif
#if defined(SCHED_DEADLINE)
	SCHED_DEADLINE,
#endif
};

static int stress_schedpolicy(const stress_args_t *args)
{
	int policy = args->instance % SIZEOF_ARRAY(policies);
#if defined(_POSIX_PRIORITY_SCHEDULING)
	const bool root_or_nice_capability = stress_check_capability(SHIM_CAP_SYS_NICE);
#endif
#if defined(HAVE_SCHED_GETATTR) && \
    defined(HAVE_SCHED_SETATTR)
	uint32_t sched_util_min = ~0U;
	uint32_t sched_util_max = 0;
	uint32_t sched_util_max_value = 0;
	int counter = 0;
#endif

#if defined(_POSIX_PRIORITY_SCHEDULING)
	int n = 0;
#endif

	if (SIZEOF_ARRAY(policies) == (0)) {
		if (args->instance == 0) {
			pr_inf_skip("%s: no scheduling policies "
				"available, skipping test\n",
				args->name);
		}
		return EXIT_NOT_IMPLEMENTED;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
#if defined(HAVE_SCHED_GETATTR) &&	\
    defined(HAVE_SCHED_SETATTR)
		struct shim_sched_attr attr;
#else
		UNEXPECTED
#endif
		struct sched_param param;
		int ret = 0;
		int max_prio, min_prio, rng_prio;
		int new_policy = policies[policy];
		const pid_t pid = stress_mwc1() ? 0 : args->pid;
		const char *new_policy_name = stress_get_sched_name(new_policy);

		if (!keep_stressing(args))
			break;

		shim_sched_yield();
		errno = 0;

		switch (new_policy) {
#if defined(SCHED_DEADLINE) &&		\
    defined(HAVE_SCHED_GETATTR) &&	\
    defined(HAVE_SCHED_SETATTR)
		case SCHED_DEADLINE:
			/*
			 *  Only have 1 RT deadline instance running
			 */
			if (args->instance == 0) {
				(void)memset(&attr, 0, sizeof(attr));
				attr.size = sizeof(attr);
				attr.sched_flags = 0;
				attr.sched_nice = 0;
				attr.sched_priority = 0;
				attr.sched_policy = SCHED_DEADLINE;
				/* runtime <= deadline <= period */
				attr.sched_runtime = 64 * 1000000;
				attr.sched_deadline = 128 * 1000000;
				attr.sched_period = 256 * 1000000;

				ret = shim_sched_setattr(0, &attr, 0);
				break;
			}
			CASE_FALLTHROUGH;
#endif

#if defined(SCHED_IDLE)
		case SCHED_IDLE:
			CASE_FALLTHROUGH;
#endif
#if defined(SCHED_BATCH)
		case SCHED_BATCH:
			CASE_FALLTHROUGH;
#endif
#if defined(SCHED_OTHER)
		case SCHED_OTHER:
#endif
			/* Exercise illegal policy */
			(void)memset(&param, 0, sizeof(param));
			ret = sched_setscheduler(pid, -1, &param);
			(void)ret;

			/* Exercise invalid PID */
			param.sched_priority = 0;
			ret = sched_setscheduler(-1, new_policy, &param);
			(void)ret;

			/* Exercise invalid priority */
			param.sched_priority = ~0;
			ret = sched_setscheduler(pid, new_policy, &param);
			(void)ret;

			param.sched_priority = 0;
			ret = sched_setscheduler(pid, new_policy, &param);

			break;
#if defined(SCHED_RR)
		case SCHED_RR:
#if defined(HAVE_SCHED_RR_GET_INTERVAL)
			{
				struct timespec t;

				ret = sched_rr_get_interval(pid, &t);
				(void)ret;
			}
#endif
			CASE_FALLTHROUGH;
#endif
#if defined(SCHED_FIFO)
		case SCHED_FIFO:
#endif
			min_prio = sched_get_priority_min(new_policy);
			max_prio = sched_get_priority_max(new_policy);

			/* Check if min/max is supported or not */
			if ((min_prio == -1) || (max_prio == -1))
				continue;

			rng_prio = max_prio - min_prio;
			if (rng_prio == 0) {
				pr_err("%s: invalid min/max priority "
					"range for scheduling policy %s "
					"(min=%d, max=%d)\n",
					args->name,
					new_policy_name,
					min_prio, max_prio);
				break;
			}
			param.sched_priority = ((int)stress_mwc32() % rng_prio) +
						min_prio;
			ret = sched_setscheduler(pid, new_policy, &param);
			break;
		default:
			/* Should never get here */
			break;
		}
		if (ret < 0) {
			/*
			 *  Some systems return EINVAL for non-POSIX
			 *  scheduling policies, silently ignore these
			 *  failures.
			 */
			if ((errno != EPERM) &&
			    (errno != EINVAL) &&
			    (errno != EINTR) &&
			    (errno != EBUSY)) {
				pr_fail("%s: sched_setscheduler "
					"failed: errno=%d (%s) "
					"for scheduler policy %s\n",
					args->name, errno, strerror(errno),
					new_policy_name);
			}
		} else {
			ret = sched_getscheduler(pid);
			if (ret < 0) {
				pr_fail("%s: sched_getscheduler failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			} else if (ret != policies[policy]) {
				pr_fail("%s: sched_getscheduler "
					"failed: pid %d has policy %d (%s) "
					"but function returned %d instead\n",
					args->name, (int)pid, new_policy,
					new_policy_name, ret);
			}
		}
#if defined(_POSIX_PRIORITY_SCHEDULING)
		if (n++ >= 1024) {
			n = 0;

			/* Exercise invalid sched_getparam syscall */
			(void)memset(&param, 0, sizeof(param));
			ret = sched_getparam(-1, &param);
			(void)ret;

#if defined(__linux__)
			/* Linux allows NULL param, will return EFAULT */
			(void)memset(&param, 0, sizeof(param));
			ret = sched_getparam(pid, NULL);
			(void)ret;
#endif

			/* Exercise bad pid, ESRCH error */
			(void)memset(&param, 0, sizeof(param));
			ret = sched_getparam(stress_get_unused_pid_racy(false), &param);
			(void)ret;

			/* Exercise invalid sched_setparam syscall */
			(void)memset(&param, 0, sizeof(param));
			ret = sched_setparam(-1, &param);
			(void)ret;

#if defined(__linux__)
			/* Linux allows NULL param, will return EFAULT */
			ret = sched_setparam(pid, NULL);
			(void)ret;
#endif

			/*
			 * Exercise bad pid, ESRCH error only if process does not
			 * root or nice capability (to avoid clobbering processes we
			 * don't own
			 */
			if (!root_or_nice_capability) {
				ret = sched_setparam(stress_get_unused_pid_racy(false), &param);
				(void)ret;
			}
		}
		/* Exercise with invalid PID */
		ret = sched_getscheduler(-1);
		(void)ret;

		/* Exercise with bad pid, ESRCH error */
		ret = sched_getscheduler(stress_get_unused_pid_racy(false));
		(void)ret;

		(void)memset(&param, 0, sizeof(param));
		ret = sched_getparam(pid, &param);
		if ((ret < 0) && ((errno != EINVAL) && (errno != EPERM)))
			pr_fail("%s: sched_getparam failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));

		ret = sched_setparam(pid, &param);
		if ((ret < 0) && ((errno != EINVAL) && (errno != EPERM)))
			pr_fail("%s: sched_setparam failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
#endif

#if defined(HAVE_SCHED_GETATTR) && \
    defined(HAVE_SCHED_SETATTR)
		/* Exercise too large attr > page size */
		{
			char large_attr[args->page_size + 16];

			(void)memset(large_attr, 0, sizeof(large_attr));

			ret = shim_sched_getattr(pid,
				(struct shim_sched_attr *)large_attr,
				(unsigned int)sizeof(large_attr), 0);
			(void)ret;
		}

		/* Exercise invalid sched_getattr syscalls */
		(void)memset(&attr, 0, sizeof(attr));
		ret = shim_sched_getattr(pid, &attr, sizeof(attr), ~0U);
		(void)ret;

		/* Exercise -ve pid */
		(void)memset(&attr, 0, sizeof(attr));
		ret = shim_sched_getattr(-1, &attr, sizeof(attr), 0);
		(void)ret;

		/* Exercise bad pid, ESRCH error */
		(void)memset(&attr, 0, sizeof(attr));
		ret = shim_sched_getattr(stress_get_unused_pid_racy(false),
			&attr, sizeof(attr), 0);
		(void)ret;

		/*
		 *  Nothing too clever here, just get and set for now
		 */
		(void)memset(&attr, 0, sizeof(attr));
		attr.size = sizeof(attr);
		ret = shim_sched_getattr(pid, &attr, sizeof(attr), 0);
		if (ret < 0) {
			if (errno != ENOSYS) {
				pr_fail("%s: sched_getattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			}
		}

		if (attr.sched_util_max != 0) {
			/*
			 *  Newer Linux kernels support a min/max setting,
			 *  and if supported then find the min/max settings
			 */
			if (sched_util_min > attr.sched_util_min)
				sched_util_min = attr.sched_util_min;
			if (sched_util_max < attr.sched_util_max)
				sched_util_max = attr.sched_util_max;
			/* Sanity check */
			if (sched_util_min > sched_util_max)
				sched_util_min = sched_util_max;

			/* Zero value means not initialized yet */
			if (sched_util_max_value == 0)
				sched_util_max_value = sched_util_max;

			attr.sched_util_max = sched_util_max_value;
		}

		/*
		 * Exercise invalid sched_getattr syscalls, even if the
		 * syscalls succeed only correct value will be set,
		 * hence ignoring whether syscall succeeds or fails
		 */
		ret = shim_sched_setattr(pid, &attr, ~0U);
		(void)ret;

		ret = shim_sched_setattr(-1, &attr, 0);
		(void)ret;

		attr.size = sizeof(attr);
		ret = shim_sched_setattr(pid, &attr, 0);
		if (ret < 0) {
			if (errno != ENOSYS) {
				pr_fail("%s: sched_setattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			}
		}

		/*
		 * Cycle down max value to min value to
		 * exercise scheduler
		 */
		if ((counter++ > 256) &&
		    (sched_util_max_value > 0) &&
		    (sched_util_max_value > sched_util_min)) {
			sched_util_max_value--;
			counter = 0;
		}
#else
		UNEXPECTED
#endif
		policy++;
		policy %= SIZEOF_ARRAY(policies);
		inc_counter(args);
	} while (keep_stressing(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	return EXIT_SUCCESS;
}

stressor_info_t stress_schedpolicy_info = {
	.stressor = stress_schedpolicy,
	.class = CLASS_INTERRUPT | CLASS_SCHEDULER | CLASS_OS,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_schedpolicy_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_INTERRUPT | CLASS_SCHEDULER | CLASS_OS,
	.help = help
};
#endif
