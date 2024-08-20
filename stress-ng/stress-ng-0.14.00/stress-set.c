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

#if defined(HAVE_SYS_FSUID_H)
#include <sys/fsuid.h>
#else
UNEXPECTED
#endif

#if defined(HAVE_GRP_H)
#include <grp.h>
#else
UNEXPECTED
#endif

#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#if !defined(_BSD_SOURCE)
#define _BSD_SOURCE 1
#endif

#define GIDS_MAX 	(1024)

typedef struct {
	shim_rlimit_resource_t id;
	int	ret;
	struct rlimit rlim;
} stress_rlimit_info_t;

static stress_rlimit_info_t rlimits[] = {
#if defined(RLIMIT_AS)
	{ RLIMIT_AS, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_CORE)
	{ RLIMIT_CORE, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_CPU)
	{ RLIMIT_CPU, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_DATA)
	{ RLIMIT_DATA, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_FSIZE)
	{ RLIMIT_FSIZE, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_MEMLOCK)
	{ RLIMIT_MEMLOCK, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_MSGQUEUE)
	{ RLIMIT_MSGQUEUE, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_NICE)
	{ RLIMIT_NICE, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_NOFILE)
	{ RLIMIT_NOFILE, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_RSS)
	{ RLIMIT_RSS, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_RTPRIO)
	{ RLIMIT_RTPRIO, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_RTTIME)
	{ RLIMIT_RTTIME, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_SIGPENDING)
	{ RLIMIT_SIGPENDING, 0, { 0, 0 } },
#endif
#if defined(RLIMIT_STACK)
	{ RLIMIT_STACK, 0, { 0, 0 } },
#endif
};

static const stress_help_t help[] = {
	{ NULL,	"set N",	"start N workers exercising the set*() system calls" },
	{ NULL,	"set-ops N",	"stop after N set bogo operations" },
	{ NULL,	NULL,		NULL }
};

/*
 *  stress on set*() calls
 *	stress system by rapid get*() system calls
 */
static int stress_set(const stress_args_t *args)
{
	size_t i;
	int ret_hostname;
	const size_t hostname_len = stress_hostname_length();
	char hostname[hostname_len];
	char longname[hostname_len << 1];
#if defined(HAVE_GETPGID) && 	\
    defined(HAVE_SETPGID)
	const pid_t mypid = getpid();
#endif
	const bool cap_sys_resource = stress_check_capability(SHIM_CAP_SYS_RESOURCE);
#if defined(HAVE_SETREUID)
	const bool cap_setuid = stress_check_capability(SHIM_CAP_SETUID);
#endif
#if defined(HAVE_GETPGID) &&    \
    defined(HAVE_SETPGID)
	const bool cap_root = stress_check_capability(0);
#endif

	for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
		rlimits[i].ret = getrlimit(rlimits[i].id, &rlimits[i].rlim);
	}

	(void)memset(hostname, 0, sizeof(hostname));
	(void)memset(longname, 0, sizeof(longname));
	ret_hostname = gethostname(hostname, sizeof(hostname) - 1);
	if (ret_hostname == 0) {
		hostname[sizeof(hostname) - 1] = '\0';
		shim_strlcpy(longname, hostname, sizeof(longname));
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		int ret;
		pid_t pid;
		gid_t gid;
		uid_t uid;
#if defined(HAVE_SETREUID)
		uid_t bad_uid;
#else
		UNEXPECTED
#endif
		struct rlimit rlim;

		/* setsid will fail, ignore return */
		pid = setsid();
		(void)pid;
		if (!keep_stressing(args))
			break;

		/* getgid always succeeds */
		gid = getgid();
		ret = setgid(gid);
		(void)ret;
		if (!keep_stressing(args))
			break;

		if (*longname) {
			ret = sethostname(longname, sizeof(longname));
			(void)ret;

			ret = sethostname(hostname, strlen(hostname));
			(void)ret;
		}

#if defined(HAVE_GETPGID) &&	\
    defined(HAVE_SETPGID)
		pid = getpgid(mypid);
		if (pid != -1) {
			if (!cap_root) {
				const pid_t bad_pid = stress_get_unused_pid_racy(false);

				/* Exercise invalid pgid */
				ret = setpgid(mypid, bad_pid);
				(void)ret;

				/* Exercise invalid pid */
				ret = setpgid(bad_pid, pid);
				(void)ret;

				/* Exercise invalid pid and pgid */
				ret = setpgid(bad_pid, bad_pid);
				(void)ret;
			}
			ret = setpgid(mypid, pid);
			(void)ret;
			if (!keep_stressing(args))
				break;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETTIMEOFDAY)
		if (!stress_check_capability(SHIM_CAP_SYS_TIME)) {
			struct timeval tv;
			struct timezone tz;

			/* We should not be able to set the time of day */
			ret = gettimeofday(&tv, &tz);
			if (ret == 0) {
				ret = settimeofday(&tv, &tz);
				if (ret != -EPERM) {
					pr_fail("%s: settimeofday failed, did not have privilege to "
						"set time, expected -EPERM, instead got errno=%d (%s)\n",
						args->name, errno, strerror(errno));
				}
			}
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETPGRP) &&	\
    defined(HAVE_SETPGRP)
		/* getpgrp always succeeds */
		pid = getpgrp();
		if (pid != -1) {
			ret = setpgrp();
			(void)ret;
			if (!keep_stressing(args))
				break;
		}
#else
		UNEXPECTED
#endif

		/* getuid always succeeds */
		uid = getuid();
		ret = setuid(uid);
		(void)ret;
		if (!keep_stressing(args))
			break;

#if defined(HAVE_GRP_H)
		ret = getgroups(0, NULL);
		if (ret > 0) {
			gid_t groups[GIDS_MAX];
			int n;

			(void)memset(groups, 0, sizeof(groups));
			ret = STRESS_MINIMUM(ret, (int)SIZEOF_ARRAY(groups));
			n = getgroups(ret, groups);
			if (n > 0) {
				gid_t bad_groups[1] = { (gid_t)-1 };

				/* Exercise invalid groups */
				ret = setgroups(-1, groups);
				(void)ret;

				ret = setgroups(0, groups);
				(void)ret;

				ret = setgroups(1, bad_groups);
				(void)ret;

				ret = setgroups((size_t)n, groups);
				(void)ret;
			}
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETREUID)
		ret = setreuid((uid_t)-1, (uid_t)-1);
		(void)ret;

		/*
		 *  Validate setreuid syscalls exercised to increase the current
		 *  ruid and euid without CAP_SETUID capability cannot succeed
		 */
		if ((!cap_setuid) && (stress_get_unused_uid(&bad_uid) >= 0)) {
			if (setreuid(bad_uid, bad_uid) == 0) {
				pr_fail("%s: setreuid failed, did not have privilege to set "
					"ruid and euid, expected -EPERM, instead got errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			}
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETREGID)
#if defined(HAVE_GETRESGID)
		{
			gid_t rgid = -1;
			gid_t egid = -1;
			gid_t sgid = -1;

			ret = getresgid(&rgid, &egid, &sgid);
			if (ret == 0) {
				ret = setregid(rgid, egid);
				(void)ret;
				ret = setregid((gid_t)-1, egid);
				(void)ret;
				ret = setregid(rgid, (gid_t)-1);
				(void)ret;

				if (geteuid() != 0) {
					ret = setregid((gid_t)-2, egid);
					(void)ret;
					ret = setregid(rgid, (gid_t)-2);
					(void)ret;
				}
			}
		}
#else
		UNEXPECTED
#endif
		ret = setregid((gid_t)-1, (gid_t)-1);
		(void)ret;
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETRESUID)
#if defined(HAVE_GETRESUID)
		{
			uid_t ruid = -1;
			uid_t euid = -1;
			uid_t suid = -1;

			ret = getresuid(&ruid, &euid, &suid);
			if (ret == 0) {
				ret = setresuid(ruid, euid, suid);
				(void)ret;
				ret = setresuid(ruid, euid, (uid_t)-1);
				(void)ret;
				ret = setresuid(ruid, (uid_t)-1, suid);
				(void)ret;
				ret = setresuid(ruid, (uid_t)-1, (uid_t)-1);
				(void)ret;
				ret = setresuid((uid_t)-1, euid, suid);
				(void)ret;
				ret = setresuid((uid_t)-1, euid, (uid_t)-1);
				(void)ret;
				ret = setresuid((uid_t)-1, (uid_t)-1, suid);
				(void)ret;

				if (geteuid() != 0) {
					ret = setresuid((uid_t)-2, euid, suid);
					(void)ret;
					ret = setresuid(ruid, (uid_t)-2, suid);
					(void)ret;
					ret = setresuid(ruid, euid, (uid_t)-2);
					(void)ret;
				}
				ret = setresuid(ruid, euid, suid);
				(void)ret;
			}
		}
#else
		UNEXPECTED
#endif
		ret = setresuid((uid_t)-1, (uid_t)-1, (uid_t)-1);
		(void)ret;
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETRESGID)
#if defined(HAVE_GETRESGID)
		{
			gid_t rgid = -1;
			gid_t egid = -1;
			gid_t sgid = -1;

			ret = getresgid(&rgid, &egid, &sgid);
			if (ret == 0) {
				ret = setresgid(rgid, egid, sgid);
				(void)ret;
				ret = setresgid(rgid, egid, (gid_t)-1);
				(void)ret;
				ret = setresgid(rgid, (gid_t)-1, sgid);
				(void)ret;
				ret = setresgid(rgid, (gid_t)-1, (gid_t)-1);
				(void)ret;
				ret = setresgid((gid_t)-1, egid, sgid);
				(void)ret;
				ret = setresgid((gid_t)-1, egid, (gid_t)-1);
				(void)ret;
				ret = setresgid((gid_t)-1, (gid_t)-1, sgid);
				(void)ret;
				if (geteuid() != 0) {
					ret = setresgid((gid_t)-2, egid, sgid);
					(void)ret;
					ret = setresgid(rgid, (gid_t)-2, sgid);
					(void)ret;
					ret = setresgid(rgid, egid, (gid_t)-2);
					(void)ret;
				}
				ret = setresgid(rgid, egid, sgid);
				(void)ret;
			}
		}
#else
		UNEXPECTED
#endif
		ret = setresgid((gid_t)-1, (gid_t)-1, (gid_t)-1);
		(void)ret;
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETFSGID) && 	\
    defined(HAVE_SYS_FSUID_H)
		{
			int fsgid;

			/* Passing -1 will return the current fsgid */
			fsgid = setfsgid((uid_t)-1);
			if (fsgid >= 0) {
				/* Set the current fsgid, should work */
				ret = setfsgid((uid_t)fsgid);
				if (ret == fsgid) {
					/*
					 * we can try changing it to
					 * something else knowing it can
					 * be restored successfully
					 */
					ret = setfsgid(gid);
					(void)ret;

					ret = setfsgid((uid_t)getegid());
					(void)ret;

					/* And restore */
					ret = setfsgid((uid_t)fsgid);
					(void)ret;
				}
			}
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SETFSUID) && 	\
    defined(HAVE_SYS_FSUID_H)
		{
			int fsuid;

			/* Passing -1 will return the current fsuid */
			fsuid = setfsuid((uid_t)-1);
			if (fsuid >= 0) {
				/* Set the current fsuid, should work */
				ret = setfsuid((uid_t)fsuid);
				if (ret == fsuid) {
					/*
					 * we can try changing it to
					 * something else knowing it can
					 * be restored successfully
					 */
					ret = setfsuid(uid);
					(void)ret;

					ret = setfsuid((uid_t)geteuid());
					(void)ret;

					/* And restore */
					ret = setfsuid((uid_t)fsuid);
					(void)ret;
				}
			}
		}
#else
		UNEXPECTED
#endif

#if defined(__NR_sgetmask) &&	\
    defined(__NR_ssetmask)
		{
			long mask;

			mask = shim_sgetmask();
			shim_ssetmask(mask);
		}
#endif

#if defined(HAVE_GETDOMAINNAME) &&	\
    defined(HAVE_SETDOMAINNAME)
		{
			char name[2048];

			ret = getdomainname(name, sizeof(name));
			if (ret == 0) {
				/* Exercise zero length name (OK-ish) */
				ret = setdomainname(name, 0);
				(void)ret;

				/* Exercise long name (-EINVAL) */
				ret = setdomainname(name, sizeof(name));
				(void)ret;

				/* Set name back */
				(void)ret;
				ret = setdomainname(name, strlen(name));
			}
			(void)ret;
			if (!keep_stressing(args))
				break;
		}
#else
		UNEXPECTED
#endif
		/*
		 *  Invalid setrlimit syscall with invalid
		 *  resource attribute resulting in EINVAL error
		 */
		(void)memset(&rlim, 0, sizeof(rlim));
		if ((getrlimit((shim_rlimit_resource_t)INT_MAX, &rlim) < 0) && (errno == EINVAL)) {
			(void)setrlimit((shim_rlimit_resource_t)INT_MAX, &rlim);
		}

		for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
			if (rlimits[i].ret == 0) {
				/* Bogus setrlimit syscall with invalid rlim attribute */
				rlim.rlim_cur = rlimits[i].rlim.rlim_cur;
				if (rlim.rlim_cur > 1) {
					rlim.rlim_max = rlim.rlim_cur - 1;
					(void)setrlimit(rlimits[i].id, &rlim);
				}

				/* Valid setrlimit syscall and ignoring failure */
				ret = setrlimit(rlimits[i].id, &rlimits[i].rlim);
				(void)ret;
			}
		}

		/*
		 *  Invalid setrlimit syscalls exercised
		 *  to increase the current hard limit
		 *  without CAP_SYS_RESOURCE capability
		 */
		if (!cap_sys_resource) {
			for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
				if ((rlimits[i].ret == 0) && (rlimits[i].rlim.rlim_max < RLIM_INFINITY)) {
					rlim.rlim_cur = rlimits[i].rlim.rlim_cur;
					rlim.rlim_max = RLIM_INFINITY;
					ret = setrlimit(rlimits[i].id, &rlim);
					if (ret != -EPERM) {
						pr_fail("%s: setrlimit failed, did not have privilege to set "
							"hard limit, expected -EPERM, instead got errno=%d (%s)\n",
							args->name, errno, strerror(errno));
					}
					if (ret == 0) {
						(void)setrlimit(rlimits[i].id, &rlimits[i].rlim);
					}
				}
			}
		}

		{
			/*
			 *  Exercise stime if it is available and
			 *  ignore CAP_SYS_TIME requirement to call
			 *  it.  Exercise this just once as constantly
			 *  setting the time may cause excessive
			 *  time drift.
			 */
			time_t t;
			static bool test_stime = true;

			if (test_stime && (time(&t) != ((time_t)-1))) {
				ret = shim_stime(&t);
				(void)ret;
				test_stime = false;
			}
		}
		inc_counter(args);
	} while (keep_stressing(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	return EXIT_SUCCESS;
}

stressor_info_t stress_set_info = {
	.stressor = stress_set,
	.class = CLASS_OS,
	.verify = VERIFY_ALWAYS,
	.help = help
};
