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

#if defined(HAVE_LINUX_SYSCTL_H)
#include <linux/sysctl.h>
#endif

#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#endif

#if defined(HAVE_SYS_TIMEX_H)
#include <sys/timex.h>
#endif

#if defined(HAVE_SYS_UTSNAME_H)
#include <sys/utsname.h>
#endif

#if defined(HAVE_SYS_VFS_H)
#include <sys/vfs.h>
#endif

#if defined(HAVE_UCONTEXT_H)
#include <ucontext.h>
#endif

#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#if !defined(_BSD_SOURCE)
#define _BSD_SOURCE 1
#endif

#define GIDS_MAX 	(1024)
#define MOUNTS_MAX	(256)

static const stress_help_t help[] = {
	{ NULL,	"get N",	"start N workers exercising the get*() system calls" },
	{ NULL,	"get-ops N",	"stop after N get bogo operations" },
	{ NULL, NULL,		NULL }
};

typedef struct {
	const int who;		/* rusage who field */
	const char *name;	/* textual name of who value */
	const bool verify;	/* check for valid rusage return */
} stress_rusage_t;

static const stress_rusage_t rusages[] = {
#if defined(RUSAGE_SELF)
	{ RUSAGE_SELF, 		"RUSAGE_SELF",		true },
#endif
#if defined(RUSAGE_CHILDREN)
	{ RUSAGE_CHILDREN, 	"RUSAGE_CHILDREN",	true },
#endif
#if defined(RUSAGE_THREAD)
	{ RUSAGE_THREAD,	"RUSAGE_THREAD",	true },
#endif
#if defined(RUSAGE_BOTH)
	{ RUSAGE_BOTH,		"RUSAGE_BOTH",		true },
#endif
	{ INT_MIN, 		"INT_MIN",		false },
	{ INT_MAX,		"INT_MAX",		false },
};

/*
 *  The following produce -EINVAL for Haiku, so
 *  disable them
 */
#if defined(__HAIKU__)
#undef RLIMIT_CPU
#undef RLIMIT_FSIZE
#endif

static const shim_rlimit_resource_t rlimits[] = {
#if defined(RLIMIT_AS)
	RLIMIT_AS,
#endif
#if defined(RLIMIT_CORE)
	RLIMIT_CORE,
#endif
#if defined(RLIMIT_CPU)
	RLIMIT_CPU,
#endif
#if defined(RLIMIT_DATA)
	RLIMIT_DATA,
#endif
#if defined(RLIMIT_FSIZE)
	RLIMIT_FSIZE,
#endif
#if defined(RLIMIT_MEMLOCK)
	RLIMIT_MEMLOCK,
#endif
#if defined(RLIMIT_MSGQUEUE)
	RLIMIT_MSGQUEUE,
#endif
#if defined(RLIMIT_NICE)
	RLIMIT_NICE,
#endif
#if defined(RLIMIT_NOFILE)
	RLIMIT_NOFILE,
#endif
#if defined(RLIMIT_RSS)
	RLIMIT_RSS,
#endif
#if defined(RLIMIT_RTPRIO)
	RLIMIT_RTPRIO,
#endif
#if defined(RLIMIT_RTTIME)
	RLIMIT_RTTIME,
#endif
#if defined(RLIMIT_SIGPENDING)
	RLIMIT_SIGPENDING,
#endif
#if defined(RLIMIT_STACK)
	RLIMIT_STACK
#endif
};

#if defined(HAVE_GETPRIORITY)
static const shim_priority_which_t priorities[] = {
#if defined(PRIO_PROCESS)
	PRIO_PROCESS,
#endif
#if defined(PRIO_PGRP)
	PRIO_PGRP,
#endif
#if defined(PRIO_USER)
	PRIO_USER
#endif
};
#endif

static sigjmp_buf jmp_env;

static void NORETURN MLOCKED_TEXT stress_segv_handler(int num)
{
	(void)num;

	siglongjmp(jmp_env, 1);
}

/*
 *  stress on get*() calls
 *	stress system by rapid get*() system calls
 */
static int stress_get(const stress_args_t *args)
{
	char *mnts[MOUNTS_MAX];
	int mounts_max;
	const bool verify = !!(g_opt_flags & OPT_FLAGS_VERIFY);
#if defined(HAVE_SYS_TIMEX_H)
#if defined(HAVE_ADJTIMEX) || defined(HAVE_ADJTIME)
	const bool cap_sys_time = stress_check_capability(SHIM_CAP_SYS_TIME);
#endif
#endif

	if (stress_sighandler(args->name, SIGSEGV, stress_segv_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;

	mounts_max = stress_mount_get(mnts, MOUNTS_MAX);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		char path[PATH_MAX];
		char *ptr;
		gid_t gids[GIDS_MAX];
		unsigned cpu, node;
		const pid_t mypid = getpid();
		int ret, n, fs_index;
		size_t i;
#if defined(HAVE_SYS_TIMEX_H) &&	\
    defined(HAVE_ADJTIME)
		struct timeval delta;
#endif
		struct timeval tv;
		struct timezone tz;
		struct rlimit rlim;
		time_t t, t1, t2;
		pid_t pid;
		gid_t gid;
		uid_t uid;

		(void)mypid;

		if (!keep_stressing_flag())
			break;

		pid = getppid();
		(void)pid;

		if (!keep_stressing_flag())
			break;

#if defined(HAVE_SWAPCONTEXT) &&        \
    defined(HAVE_UCONTEXT_H)
		{
			ucontext_t context;

			ret = getcontext(&context);
			(void)ret;
			if (!keep_stressing_flag())
				break;
		}

#if defined(HAVE_SWAPCONTEXT) &&        \
    defined(HAVE_UCONTEXT_H)
		{
			ucontext_t context;

			ret = getcontext(&context);
			(void)ret;
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETDOMAINNAME)
		{
			char name[128];

			ret = getdomainname(name, sizeof(name));
			(void)ret;
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETHOSTID)
		{
			long id;

			id = gethostid();
			(void)id;
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETHOSTNAME)
		{
			char name[128];

			ret = gethostname(name, sizeof(name));
			(void)ret;
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif

		ptr = getcwd(path, sizeof path);
		if (verify) {
			if (!ptr) {
				pr_fail("%s: getcwd %s failed, errno=%d (%s)\n",
					args->name, path, errno, strerror(errno));
			} else {
				/* getcwd returned a string: is it the same as path? */
				if (strncmp(ptr, path, sizeof(path))) {
					pr_fail("%s: getcwd returned a string that "
						"is different from the expected path\n",
						args->name);
				}
			}
		}
		if (!keep_stressing_flag())
			break;

		gid = getgid();
		(void)gid;

		gid = getegid();
		(void)gid;

		uid = getuid();
		(void)uid;

		uid = geteuid();
		(void)uid;

		if (!keep_stressing_flag())
			break;

		/*
		 *  Zero size should return number of gids to fetch
		 */
		ret = getgroups(0, gids);
		(void)ret;

		/*
		 *  Try to get GIDS_MAX number of gids
		 */
		ret = getgroups(GIDS_MAX, gids);
		if (verify && (ret < 0) && (errno != EINVAL))
			pr_fail("%s: getgroups failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		/*
		 *  Exercise invalid getgroups calls
		 */
		ret = getgroups(1, gids);
		(void)ret;
#if defined(__NR_getgroups)
		/*
		 *  libc may detect a -ve gidsetsize argument and not call
		 *  the system call. Override this by directly calling the
		 *  system call if possible. Memset gids to zero to keep
		 *  valgrind happy.
		 */
		(void)memset(gids, 0, sizeof(gids));
		ret = (int)syscall(__NR_getgroups, -1, gids);
		(void)ret;
#endif
		if (!keep_stressing_flag())
			break;

#if defined(HAVE_GETPGRP)
		pid = getpgrp();
		(void)pid;
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETPGID)
		pid = getpgid(mypid);
		(void)pid;

		/*
		 *  Exercise with an possibly invalid pid
		 */
		pid = stress_get_unused_pid_racy(false);
		pid = getpgid(pid);
		(void)pid;

		if (!keep_stressing_flag())
			break;
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETPRIORITY)
		/*
		 *  Exercise getpriority calls that use illegal
		 *  arguments to get more kernel test coverage
		 */
		(void)getpriority((shim_priority_which_t)INT_MIN, 0);
		(void)getpriority((shim_priority_which_t)INT_MAX, 0);
		pid = stress_get_unused_pid_racy(false);
		(void)getpriority((shim_priority_which_t)0, pid);

		for (i = 0; i < SIZEOF_ARRAY(priorities); i++) {
			errno = 0;
			ret = getpriority(priorities[i], 0);
			if (verify && errno && (errno != EINVAL) && (ret < 0))
				pr_fail("%s: getpriority failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			if (!keep_stressing_flag())
				break;
		}
		/* Exercise getpriority calls using non-zero who argument */
		for (i = 0; i < SIZEOF_ARRAY(priorities); i++){
			ret = getpriority(priorities[i], ~(id_t)0);
			(void)ret;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETRESGID)
		{
			gid_t rgid, egid, sgid;

			ret = getresgid(&rgid, &egid, &sgid);
			if (verify && (ret < 0))
				pr_fail("%s: getresgid failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETRESUID)
		{
			uid_t ruid, euid, suid;

			ret = getresuid(&ruid, &euid, &suid);
			if (verify && (ret < 0))
				pr_fail("%s: getresuid failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif
		/* Invalid getrlimit syscall and ignoring failure */
		(void)getrlimit((shim_rlimit_resource_t)INT_MAX, &rlim);

		for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
			ret = getrlimit(rlimits[i], &rlim);
			if (verify && (ret < 0))
				pr_fail("%s: getrlimit(%zu, ..) failed, errno=%d (%s)\n",
					args->name, i, errno, strerror(errno));
			if (!keep_stressing_flag())
				break;
		}

#if defined(__NR_ugetrlimit)
		/*
		 * Legacy system call, most probably __NR_ugetrlimit
		 * is not defined, ignore return as it's most probably
		 * ENOSYS too
		 */
		(void)syscall(__NR_ugetrlimit, INT_MAX, &rlim);
		(void)syscall(__NR_ugetrlimit, INT_MIN, &rlim);
		(void)syscall(__NR_ugetrlimit, -1, &rlim);

		for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
			ret = syscall(__NR_ugetrlimit, rlimits[i], &rlim);
			(void)ret;
		}
#endif

#if defined(HAVE_PRLIMIT) &&	\
    NEED_GLIBC(2,13,0) &&	\
    defined(EOVERFLOW)
		/* Invalid prlimit syscall and ignoring failure */
		(void)prlimit(mypid, (shim_rlimit_resource_t)INT_MAX, NULL, &rlim);
		pid = stress_get_unused_pid_racy(false);
		(void)prlimit(pid, (shim_rlimit_resource_t)INT_MAX, NULL, &rlim);

		for (i = 0; i < SIZEOF_ARRAY(rlimits); i++) {
			struct rlimit rlims[2];

			ret = prlimit(mypid, rlimits[i], NULL, &rlims[0]);
			if (verify && (ret < 0) && (errno != EOVERFLOW))
				pr_fail("%s: prlimit(%" PRIdMAX ", %zu, ..) failed, errno=%d (%s)\n",
					args->name, (intmax_t)mypid, i, errno, strerror(errno));
			if (!ret) {
				ret = prlimit(mypid, rlimits[i], &rlims[0], NULL);
				if (verify && (ret < 0) && (errno != EOVERFLOW))
					pr_fail("%s: prlimit(%" PRIdMAX ", %zu, ..) failed, errno=%d (%s)\n",
						args->name, (intmax_t)mypid, i, errno, strerror(errno));
				ret = prlimit(mypid, rlimits[i], &rlims[0], &rlims[1]);
				if (verify && (ret < 0) && (errno != EOVERFLOW))
					pr_fail("%s: prlimit(%" PRIdMAX", %zu, ..) failed, errno=%d (%s)\n",
						args->name, (intmax_t)mypid, i, errno, strerror(errno));
			}

			/* Exercise invalid pids */
			(void)prlimit(pid, rlimits[i], NULL, &rlims[0]);
			if (!keep_stressing_flag())
				break;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_LINUX_SYSCTL_H) &&	\
    defined(__NR__sysctl)
		{
			/*
			 *  _sysctl is a deprecated API, so it
			 *  probably will return -ENOSYS
			 */
			int name[] = { KERN_VERSION };
			char kern_version[64];
			size_t kern_version_len = 0;
			struct __sysctl_args sysctl_args;

			(void)memset(&sysctl_args, 0, sizeof(sysctl_args));
			sysctl_args.name = name;
			sysctl_args.nlen = SIZEOF_ARRAY(name);
			sysctl_args.oldval = kern_version;
			sysctl_args.oldlenp = &kern_version_len;
			sysctl_args.newval = NULL;
			sysctl_args.newlen = 0;

			ret = (int)syscall(__NR__sysctl, &sysctl_args);
			(void)ret;
		}
#endif

		for (i = 0; i < SIZEOF_ARRAY(rusages); i++) {
			struct rusage usage;

			ret = shim_getrusage(rusages[i].who, &usage);
			if (rusages[i].verify && verify && (ret < 0) && (errno != ENOSYS))
				pr_fail("%s: getrusage(%s, ..) failed, errno=%d (%s)\n",
					args->name, rusages[i].name, errno, strerror(errno));
			if (!keep_stressing_flag())
				break;
		}

#if defined(HAVE_GETSID)
		ret = getsid(mypid);
		if (verify && (ret < 0))
			pr_fail("%s: getsid failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		pid = stress_get_unused_pid_racy(false);
		ret = getsid(pid);
		(void)ret;
		if (!keep_stressing_flag())
			break;
#else
		UNEXPECTED
#endif

		(void)shim_gettid();

		(void)shim_getcpu(&cpu, &node, NULL);
		(void)shim_getcpu(NULL, &node, NULL);
		(void)shim_getcpu(&cpu, NULL, NULL);
		(void)shim_getcpu(NULL, NULL, NULL);
		if (!keep_stressing_flag())
			break;

		t = time(NULL);
		if (t == (time_t)-1) {
			pr_fail("%s: time failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
		/*
		 *  Exercise time calls with a pointer to time_t
		 *  variable and check equality with returned value
		 *  with it to increase kernel test coverage
		 */
		t1 = time(&t2);
		if (memcmp(&t1, &t2, sizeof(t1))) {
			pr_fail("%s: time failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));

		}
		/*
		 *  Exercise the time system call using the syscall()
		 *  function to increase kernel test coverage
		 */
		t = shim_time(NULL);
		if ((t == (time_t)-1) && (errno != ENOSYS))
			pr_fail("%s: time failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));

		t1 = shim_time(&t2);
		if ((t == (time_t)-1) && (errno != ENOSYS)) {
			if (memcmp(&t1, &t2, sizeof(t1))) {
				pr_fail("%s: time failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
			}
		}


		/*
		 *  The following gettimeofday calls probably use the VDSO
		 *  on Linux
		 */
		ret = gettimeofday(&tv, NULL);
		if (ret < 0) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
		ret = gettimeofday(&tv, &tz);
		if (ret < 0) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
#if 0
		/*
		 * gettimeofday with NULL args works fine on x86-64
		 * linux, but on s390 will segfault. Disable this
		 * for now.
		 */
		ret = gettimeofday(NULL, NULL);
		if ((ret < 0) && (errno != ENOSYS)) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
#endif

		/*
		 *  Exercise the gettimeofday by force using the syscall
		 *  via the shim to force non-use of libc wrapper and/or
		 *  the vdso
		 */
		ret = shim_gettimeofday(&tv, NULL);
		if ((ret < 0) && (errno != ENOSYS)) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
		ret = shim_gettimeofday(&tv, &tz);
		if ((ret < 0) && (errno != ENOSYS)) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}
		ret = shim_gettimeofday(NULL, NULL);
		if ((ret < 0) && (errno != ENOSYS)) {
			pr_fail("%s: gettimeval failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
		}

#if defined(HAVE_UNAME) &&	\
    defined(HAVE_SYS_UTSNAME_H)
		{
			struct utsname utsbuf;
			static bool uname_segv = false;

			if (!uname_segv) {
				ret = sigsetjmp(jmp_env, 1);
				if (!keep_stressing(args))
					break;
				if (ret != 0) {
					uname_segv = true;
				} else {
					ret = uname(args->mapped->page_none);
					if (ret == 0) {
						pr_fail("%s: uname unexpectedly succeeded with read only utsbuf, "
							"expected -EFAULT, instead got errno=%d (%s)\n",
							args->name, errno, strerror(errno));
					}
				}
			}

			ret = uname(&utsbuf);
			if (verify && (ret < 0))
				pr_fail("%s: uname failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETPAGESIZE)
		ret = getpagesize();
		(void)ret;
#else
		UNEXPECTED
#endif

#if defined(HAVE_GETDTABLESIZE)
		ret = getdtablesize();
		(void)ret;
#else
		UNEXPECTED
#endif

#if defined(HAVE_LOOKUP_DCOOKIE)
		{
			char buf[PATH_MAX];

			ret = (int)syscall(__NR_lookup_dcookie, buf, sizeof(buf));
			(void)ret;
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SYS_TIMEX_H) &&	\
    defined(HAVE_ADJTIMEX)
		{
			struct timex timexbuf;

			timexbuf.modes = 0;
			ret = adjtimex(&timexbuf);
			if (cap_sys_time && verify && (ret < 0) && (errno != EPERM))
				pr_fail("%s: adjtimex failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
		}
#else
		UNEXPECTED
#endif

#if defined(HAVE_SYS_TIMEX_H) &&	\
    defined(HAVE_ADJTIME)
		(void)memset(&delta, 0, sizeof(delta));
		ret = adjtime(&delta, &tv);
		if (cap_sys_time && verify && (ret < 0) && (errno != EPERM))
			pr_fail("%s: adjtime failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
#else
		UNEXPECTED
#endif

#if defined(__NR_lookup_dcookie)
		/*
		 *  Exercise some random cookie lookups, really likely
		 *  to fail.
		 */
		ret = shim_lookup_dcookie(stress_mwc64(), path, sizeof(path));
		(void)ret;
#endif

#if defined(__NR_sgetmask)
		ret = shim_sgetmask();
		(void)ret;
#endif

		/* Get number of file system types */
		n = shim_sysfs(3);
		for (fs_index = 0; fs_index < n; fs_index++) {
			char buf[4096];

			ret = shim_sysfs(2, fs_index, buf);
			if (!ret) {
				ret = shim_sysfs(1, buf);
				if (verify && (ret != fs_index)) {
					pr_fail("%s: sysfs(1, %s) failed, errno=%d (%s)\n",
						args->name, buf, errno, strerror(errno));
				}
			} else {
				if (verify) {
					pr_fail("%s: sysfs(2, %d, buf) failed, errno=%d (%s)\n",
						args->name, fs_index, errno, strerror(errno));
				}
			}
		}
#if defined(HAVE_SYS_VFS_H) &&	\
    defined(HAVE_STATFS) &&	\
    defined(__linux__)
		for (i = 0; i < (size_t)mounts_max; i++) {
			struct statfs buf;
			int fd;

			(void)statfs(mnts[i], &buf);

			fd = open(mnts[i], O_RDONLY);
			if (fd >= 0) {
				(void)fstatfs(fd, &buf);
				(void)close(fd);
			}
		}
#endif

#if defined(HAVE_SYS_STATVFS_H)
		for (i = 0; i < (size_t)mounts_max; i++) {
			struct statvfs buf;

			statvfs(mnts[i], &buf);
		}
#else
		UNEXPECTED
#endif

		inc_counter(args);
	} while (keep_stressing(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	stress_mount_free(mnts, mounts_max);

	return EXIT_SUCCESS;
}

stressor_info_t stress_get_info = {
	.stressor = stress_get,
	.class = CLASS_OS,
	.verify = VERIFY_OPTIONAL,
	.help = help
};
