/*
 * Copyright (C)      2021 Canonical, Ltd.
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
#include "core-thermal-zone.h"

#if defined(HAVE_SYS_SYSMACROS_H)
#include <sys/sysmacros.h>
#endif

#if defined(HAVE_SYS_SYSCTL_H) &&       \
    !defined(__linux__)
#include <sys/sysctl.h>
#endif

#if defined(HAVE_SYS_VMMETER_H)
#include <sys/vmmeter.h>
#endif

#if defined(HAVE_MNTENT_H)
#include <mntent.h>
#endif

static int32_t vmstat_delay = 0;
static int32_t thermalstat_delay = 0;
static int32_t iostat_delay = 0;

#if defined(__FreeBSD__)
static int freebsd_getsysctl(const char *name, void *ptr, size_t size)
{
	int ret;
	size_t nsize = size;
	if (!ptr)
		return -1;

	(void)memset(ptr, 0, size);

	ret = sysctlbyname(name, ptr, &nsize, NULL, 0);
	if ((ret < 0) || (nsize != size)) {
		(void)memset(ptr, 0, size);
		return -1;
	}
	return 0;
}

/*
 *  freebsd_getsysctl_uint()
 *	get an unsigned int sysctl value by name
 */
static unsigned int freebsd_getsysctl_uint(const char *name)
{
	unsigned int val;

	freebsd_getsysctl(name, &val, sizeof(val));

	return val;
}
#endif

static int stress_set_generic_stat(
	const char *const opt,
	const char *name,
	int32_t *delay)
{
	*delay = stress_get_int32(opt);
        if ((*delay < 1) || (*delay > 3600)) {
                (void)fprintf(stderr, "%s must in the range 1 to 3600.\n", name);
                _exit(EXIT_FAILURE);
        }
	return 0;
}

int stress_set_vmstat(const char *const opt)
{
	return stress_set_generic_stat(opt, "vmstat", &vmstat_delay);
}

int stress_set_thermalstat(const char *const opt)
{
	return stress_set_generic_stat(opt, "thermalstat", &thermalstat_delay);
}

int stress_set_iostat(const char *const opt)
{
	return stress_set_generic_stat(opt, "iostat", &iostat_delay);
}

/*
 *  stress_find_mount_dev()
 *	find the path of the device that the file is located on
 */
char *stress_find_mount_dev(const char *name)
{
#if defined(__linux__)
	static char dev_path[PATH_MAX];
	struct stat statbuf;
	dev_t dev;
	FILE *mtab_fp;
	struct mntent *mnt;

	if (stat(name, &statbuf) < 0)
		return NULL;

	/* Cater for UBI char mounts */
	if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode))
		dev = statbuf.st_rdev;
	else
		dev = statbuf.st_dev;

	/* Try /proc/mounts then /etc/mtab */
	mtab_fp = setmntent("/proc/mounts", "r");
	if (!mtab_fp) {
		mtab_fp = setmntent("/etc/mtab", "r");
		if (!mtab_fp)
			return NULL;
	}

	while ((mnt = getmntent(mtab_fp))) {
		if ((!strcmp(name, mnt->mnt_dir)) ||
		    (!strcmp(name, mnt->mnt_fsname)))
			break;

		if ((mnt->mnt_fsname[0] == '/') &&
		    (stat(mnt->mnt_fsname, &statbuf) == 0) &&
		    (statbuf.st_rdev == dev))
			break;

		if ((stat(mnt->mnt_dir, &statbuf) == 0) &&
		    (statbuf.st_dev == dev))
			break;
	}
	(void)endmntent(mtab_fp);

	if (!mnt)
		return NULL;
	if (!mnt->mnt_fsname)
		return NULL;

	return realpath(mnt->mnt_fsname, dev_path);
#elif defined(HAVE_SYS_SYSMACROS_H)
	static char dev_path[PATH_MAX];
	struct stat statbuf;
	dev_t dev;
	DIR *dir;
	struct dirent *d;
	dev_t majdev;

	if (stat(name, &statbuf) < 0)
		return NULL;

	/* Cater for UBI char mounts */
	if (S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode))
		dev = statbuf.st_rdev;
	else
		dev = statbuf.st_dev;

	majdev = makedev(major(dev), 0);

	dir = opendir("/dev");
	if (!dir)
		return NULL;

	while ((d = readdir(dir)) != NULL) {
		int ret;
		struct stat stat_buf;

		stress_mk_filename(dev_path, sizeof(dev_path), "/dev", d->d_name);
		ret = stat(dev_path, &stat_buf);
		if ((ret == 0) &&
		    (S_ISBLK(stat_buf.st_mode)) &&
		    (stat_buf.st_rdev == majdev)) {
			(void)closedir(dir);
			return dev_path;
		}
	}
	(void)closedir(dir);

	return NULL;
#else
	(void)name;

	return NULL;
#endif
}

static pid_t vmstat_pid;

#if defined(HAVE_SYS_SYSMACROS_H) &&	\
    defined(__linux__)

/*
 *  stress_iostat_iostat_name()
 *	from the stress-ng temp file path try to determine
 *	the iostat file /sys/block/$dev/stat for that file.
 */
static char *stress_iostat_iostat_name(
	char *iostat_name,
	const size_t iostat_name_len)
{
	char *temp_path, *dev;

	/* Resolve links */
	temp_path = realpath(stress_get_temp_path(), NULL);
	if (!temp_path)
		return NULL;

	/* Find device */
	dev = stress_find_mount_dev(temp_path);
	if (!dev)
		return NULL;

	/* Skip over leading /dev */
	if (!strncmp(dev, "/dev", 4))
		dev += 4;

	(void)snprintf(iostat_name, iostat_name_len, "/sys/block/%s/stat", dev);

	return iostat_name;
}

/*
 *  stress_read_iostat()
 *	read the stats from an iostat stat file, linux variant
 */
static void stress_read_iostat(const char *iostat_name, stress_iostat_t *iostat)
{
	FILE *fp;

	fp = fopen(iostat_name, "r");
	if (fp) {
		int ret;

		ret = fscanf(fp,
			    "%" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64 " %" PRIu64
			    " %" PRIu64,
			&iostat->read_io, &iostat->read_merges,
			&iostat->read_sectors, &iostat->read_ticks,
			&iostat->write_io, &iostat->write_merges,
			&iostat->write_sectors, &iostat->write_ticks,
			&iostat->in_flight, &iostat->io_ticks,
			&iostat->time_in_queue,
			&iostat->discard_io, &iostat->discard_merges,
			&iostat->discard_sectors, &iostat->discard_ticks);
		(void)fclose(fp);

		if (ret != 15)
			(void)memset(iostat, 0, sizeof(*iostat));
	}
}

#define STRESS_IOSTAT_DELTA(field)					\
	iostat->field = ((iostat_current.field > iostat_prev.field) ?	\
	(iostat_current.field - iostat_prev.field) : 0)

/*
 *  stress_get_iostat()
 *	read and compute delta since last read of iostats
 */
static void stress_get_iostat(const char *iostat_name, stress_iostat_t *iostat)
{
	static stress_iostat_t iostat_prev;
	stress_iostat_t iostat_current;

	(void)memset(&iostat_current, 0, sizeof(iostat_current));
	stress_read_iostat(iostat_name, &iostat_current);
	STRESS_IOSTAT_DELTA(read_io);
	STRESS_IOSTAT_DELTA(read_merges);
	STRESS_IOSTAT_DELTA(read_sectors);
	STRESS_IOSTAT_DELTA(read_ticks);
	STRESS_IOSTAT_DELTA(write_io);
	STRESS_IOSTAT_DELTA(write_merges);
	STRESS_IOSTAT_DELTA(write_sectors);
	STRESS_IOSTAT_DELTA(write_ticks);
	STRESS_IOSTAT_DELTA(in_flight);
	STRESS_IOSTAT_DELTA(io_ticks);
	STRESS_IOSTAT_DELTA(time_in_queue);
	STRESS_IOSTAT_DELTA(discard_io);
	STRESS_IOSTAT_DELTA(discard_merges);
	STRESS_IOSTAT_DELTA(discard_sectors);
	STRESS_IOSTAT_DELTA(discard_ticks);
	(void)memcpy(&iostat_prev, &iostat_current, sizeof(iostat_prev));
}
#endif

#if defined(__linux__)
/*
 *  stress_next_field()
 *	skip to next field, returns false if end of
 *	string and/or no next field.
 */
static bool stress_next_field(char **str)
{
	char *ptr = *str;

	while (*ptr && *ptr != ' ')
		ptr++;

	if (!*ptr)
		return false;

	while (*ptr == ' ')
		ptr++;

	if (!*ptr)
		return false;

	*str = ptr;
	return true;
}

/*
 *  stress_read_vmstat()
 *	read vmstat statistics
 */
static void stress_read_vmstat(stress_vmstat_t *vmstat)
{
	FILE *fp;
	char buffer[1024];

	fp = fopen("/proc/stat", "r");
	if (fp) {
		while (fgets(buffer, sizeof(buffer), fp)) {
			char *ptr = buffer;

			if (!strncmp(buffer, "cpu ", 4))
				continue;
			if (!strncmp(buffer, "cpu", 3)) {
				if (!stress_next_field(&ptr))
					continue;
				/* user time */
				vmstat->user_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* user time nice */
				vmstat->user_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* system time */
				vmstat->system_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* idle time */
				vmstat->idle_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* iowait time */
				vmstat->wait_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* irq time, account in system time */
				vmstat->system_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* soft time, account in system time */
				vmstat->system_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* stolen time */
				vmstat->stolen_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* guest time, add to stolen stats */
				vmstat->stolen_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;

				/* guest_nice time, add to stolen stats */
				vmstat->stolen_time += (uint64_t)atoll(ptr);
				if (!stress_next_field(&ptr))
					continue;
			}

			if (!strncmp(buffer, "intr", 4)) {
				if (!stress_next_field(&ptr))
					continue;
				/* interrupts */
				vmstat->interrupt = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "ctxt", 4)) {
				if (!stress_next_field(&ptr))
					continue;
				/* context switches */
				vmstat->context_switch = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "procs_running", 13)) {
				if (!stress_next_field(&ptr))
					continue;
				/* processes running */
				vmstat->procs_running = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "procs_blocked", 13)) {
				if (!stress_next_field(&ptr))
					continue;
				/* procesess blocked */
				vmstat->procs_blocked = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "swap", 4)) {
				if (!stress_next_field(&ptr))
					continue;
				/* swap in */
				vmstat->swap_in = (uint64_t)atoll(ptr);

				if (!stress_next_field(&ptr))
					continue;
				/* swap out */
				vmstat->swap_out = (uint64_t)atoll(ptr);
			}
		}
		(void)fclose(fp);
	}

	fp = fopen("/proc/meminfo", "r");
	if (fp) {
		while (fgets(buffer, sizeof(buffer), fp)) {
			char *ptr = buffer;

			if (!strncmp(buffer, "MemFree", 7)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->memory_free = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "Buffers", 7)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->memory_buff = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "Cached", 6)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->memory_cache = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "SwapTotal", 9)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->swap_total = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "SwapUsed", 8)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->swap_used = (uint64_t)atoll(ptr);
			}
		}
		(void)fclose(fp);
	}

	fp = fopen("/proc/vmstat", "r");
	if (fp) {
		while (fgets(buffer, sizeof(buffer), fp)) {
			char *ptr = buffer;

			if (!strncmp(buffer, "pgpgin", 6)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->block_in = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "pgpgout", 7)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->block_out = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "pswpin", 6)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->swap_in = (uint64_t)atoll(ptr);
			}
			if (!strncmp(buffer, "pswpout", 7)) {
				if (!stress_next_field(&ptr))
					continue;
				vmstat->swap_out = (uint64_t)atoll(ptr);
			}
		}
		(void)fclose(fp);
	}
}
#elif defined(__FreeBSD__)
/*
 *  stress_read_vmstat()
 *	read vmstat statistics, FreeBSD variant, partially implemented
 */
static void stress_read_vmstat(stress_vmstat_t *vmstat)
{
	struct vmtotal t;

	vmstat->interrupt = freebsd_getsysctl_uint("vm.stats.sys.v_intr");
	vmstat->context_switch = freebsd_getsysctl_uint("vm.stats.sys.v_swtch");
	vmstat->swap_in = freebsd_getsysctl_uint("vm.stats.vm.v_swapin");
	vmstat->swap_out = freebsd_getsysctl_uint("vm.stats.vm.v_swapout");
	vmstat->block_in = freebsd_getsysctl_uint("vm.stats.vm.v_vnodepgsin");
	vmstat->block_out = freebsd_getsysctl_uint("vm.stats.vm.v_vnodepgsin");
	vmstat->memory_free = freebsd_getsysctl_uint("vm.stats.vm.v_free_count");

	freebsd_getsysctl("vm.vmtotal", &t, sizeof(t));
	vmstat->procs_running = t.t_rq - 1;
	vmstat->procs_blocked = t.t_dw + t.t_pw;
}
#else
/*
 *  stress_read_vmstat()
 *	read vmstat statistics, no-op
 */
static void stress_read_vmstat(stress_vmstat_t *vmstat)
{
	(void)vmstat;
}
#endif

#define STRESS_VMSTAT_COPY(field)	vmstat->field = (vmstat_current.field)
#define STRESS_VMSTAT_DELTA(field)					\
	vmstat->field = ((vmstat_current.field > vmstat_prev.field) ?	\
	(vmstat_current.field - vmstat_prev.field) : 0)

/*
 *  stress_get_vmstat()
 *	collect vmstat data, zero for initial read
 */
static void stress_get_vmstat(stress_vmstat_t *vmstat)
{
	static stress_vmstat_t vmstat_prev;
	stress_vmstat_t vmstat_current;

	(void)memset(&vmstat_current, 0, sizeof(vmstat_current));
	(void)memset(vmstat, 0, sizeof(*vmstat));
	stress_read_vmstat(&vmstat_current);
	STRESS_VMSTAT_COPY(procs_running);
	STRESS_VMSTAT_COPY(procs_blocked);
	STRESS_VMSTAT_COPY(swap_total);
	STRESS_VMSTAT_COPY(swap_used);
	STRESS_VMSTAT_COPY(memory_free);
	STRESS_VMSTAT_COPY(memory_buff);
	STRESS_VMSTAT_COPY(memory_cache);
	STRESS_VMSTAT_DELTA(swap_in);
	STRESS_VMSTAT_DELTA(swap_out);
	STRESS_VMSTAT_DELTA(block_in);
	STRESS_VMSTAT_DELTA(block_out);
	STRESS_VMSTAT_DELTA(interrupt);
	STRESS_VMSTAT_DELTA(context_switch);
	STRESS_VMSTAT_DELTA(user_time);
	STRESS_VMSTAT_DELTA(system_time);
	STRESS_VMSTAT_DELTA(idle_time);
	STRESS_VMSTAT_DELTA(wait_time);
	STRESS_VMSTAT_DELTA(stolen_time);
	(void)memcpy(&vmstat_prev, &vmstat_current, sizeof(vmstat_prev));
}

#if defined(__linux__)
/*
 *  stress_get_tz_info()
 *	get temperature in degrees C from a thermal zone
 */
static double stress_get_tz_info(stress_tz_info_t *tz_info)
{
	double temp = 0.0;
	FILE *fp;
	char path[PATH_MAX];

	(void)snprintf(path, sizeof(path),
		"/sys/class/thermal/%s/temp",
		tz_info->path);

	if ((fp = fopen(path, "r")) != NULL) {
		if (fscanf(fp, "%lf", &temp) == 1)
			temp /= 1000.0;
		(void)fclose(fp);
	}
	return temp;
}
#endif

#if defined(__linux__)
/*
 *  stress_get_cpu_ghz_average()
 *	compute average CPU frequencies in GHz
 */
static double stress_get_cpu_ghz_average(void)
{
	struct dirent **cpu_list = NULL;
	int i, n_cpus, n = 0;
	double total_freq = 0.0;

	n_cpus = scandir("/sys/devices/system/cpu", &cpu_list, NULL, alphasort);
	for (i = 0; i < n_cpus; i++) {
		char *name = cpu_list[i]->d_name;

		if (!strncmp(name, "cpu", 3) && isdigit(name[3])) {
			char path[PATH_MAX];
			double freq;
			FILE *fp;

			(void)snprintf(path, sizeof(path),
				"/sys/devices/system/cpu/%s/cpufreq/scaling_cur_freq",
				name);
			if ((fp = fopen(path, "r")) != NULL) {
				if (fscanf(fp, "%lf", &freq) == 1) {
					total_freq += freq;
					n++;
				}
				(void)fclose(fp);
			}
		}
		free(cpu_list[i]);
	}
	if (n_cpus > -1)
		free(cpu_list);

	return (n == 0) ? 0.0 : (total_freq / n) * ONE_MILLIONTH;
}
#elif defined(__FreeBSD__)
static double stress_get_cpu_ghz_average(void)
{
	const int32_t ncpus = stress_get_processors_configured();
	int32_t i;
	double total = 0.0;

	for (i = 0; i < ncpus; i++) {
		char name[32];

		(void)snprintf(name, sizeof(name), "dev.cpu.%" PRIi32 ".freq", i);
		total += (double)freebsd_getsysctl_uint(name);
	}
	total /= 1000.0;
	if (ncpus > 0)
		return total / (double)ncpus;

	return 0.0;
}
#else
static double stress_get_cpu_ghz_average(void)
{
	return 0.0;
}
#endif

/*
 *  stress_vmstat_start()
 *	start vmstat statistics (1 per second)
 */
void stress_vmstat_start(void)
{
	stress_vmstat_t vmstat;
	size_t tz_num = 0;
	stress_tz_info_t *tz_info, *tz_info_list;
	int32_t vmstat_sleep, thermalstat_sleep, iostat_sleep;
#if defined(HAVE_SYS_SYSMACROS_H) &&	\
    defined(__linux__)
	char iostat_name[PATH_MAX];
	stress_iostat_t iostat;
#endif

	if ((vmstat_delay == 0) &&
	    (thermalstat_delay == 0) &&
	    (iostat_delay == 0))
		return;

	tz_info_list = NULL;
	vmstat_sleep = vmstat_delay;
	thermalstat_sleep = thermalstat_delay;
	iostat_sleep = iostat_delay;

	vmstat_pid = fork();
	if ((vmstat_pid < 0) || (vmstat_pid > 0))
		return;

	if (vmstat_delay)
		stress_get_vmstat(&vmstat);

	if (thermalstat_delay) {
		stress_tz_init(&tz_info_list);

		for (tz_info = tz_info_list; tz_info; tz_info = tz_info->next)
			tz_num++;
	}

#if defined(HAVE_SYS_SYSMACROS_H) &&	\
    defined(__linux__)
	if (stress_iostat_iostat_name(iostat_name, sizeof(iostat_name)) == NULL)
		iostat_sleep = 0;
	if (iostat_delay)
		stress_get_iostat(iostat_name, &iostat);
#endif

	while (keep_stressing_flag()) {
		int32_t sleep_delay = INT_MAX;
		long clk_tick;

		if (vmstat_delay > 0)
			sleep_delay = STRESS_MINIMUM(vmstat_delay, sleep_delay);
		if (thermalstat_delay > 0)
			sleep_delay = STRESS_MINIMUM(thermalstat_delay, sleep_delay);
#if defined(HAVE_SYS_SYSMACROS_H) &&	\
    defined(__linux__)
		if (iostat_delay > 0)
			sleep_delay = STRESS_MINIMUM(iostat_delay, sleep_delay);
#endif

		(void)sleep((unsigned int)sleep_delay);

		/* This may change each time we get stats */
		clk_tick = sysconf(_SC_CLK_TCK) * sysconf(_SC_NPROCESSORS_ONLN);

		vmstat_sleep -= sleep_delay;
		thermalstat_sleep -= sleep_delay;
		iostat_sleep -= sleep_delay;

		if ((vmstat_delay > 0) && (vmstat_sleep <= 0))
			vmstat_sleep = vmstat_delay;
		if ((thermalstat_delay > 0) && (thermalstat_sleep <= 0))
			thermalstat_sleep = thermalstat_delay;
		if ((iostat_delay > 0) && (iostat_sleep <= 0))
			iostat_sleep = iostat_delay;

		if (vmstat_sleep == vmstat_delay) {
			double clk_tick_vmstat_delay = (double)clk_tick * (double)vmstat_delay;
			static uint32_t vmstat_count = 0;

			if ((vmstat_count++ % 25) == 0)
				pr_inf("vmstat %2s %2s %9s %9s %9s %9s "
					"%4s %4s %6s %6s %4s %4s %2s %2s "
					"%2s %2s %2s\n",
					"r", "b", "swpd", "free", "buff",
					"cache", "si", "so", "bi", "bo",
					"in", "cs", "us", "sy", "id",
					"wa", "st");

			stress_get_vmstat(&vmstat);
			pr_inf("vmstat %2" PRIu64 " %2" PRIu64 /* procs */
			       " %9" PRIu64 " %9" PRIu64	/* vm used */
			       " %9" PRIu64 " %9" PRIu64	/* memory_buff */
			       " %4" PRIu64 " %4" PRIu64	/* si, so*/
			       " %6" PRIu64 " %6" PRIu64	/* bi, bo*/
			       " %4" PRIu64 " %4" PRIu64	/* int, cs*/
			       " %2.0f %2.0f" 			/* us, sy */
			       " %2.0f %2.0f" 			/* id, wa */
			       " %2.0f\n",			/* st */
				vmstat.procs_running,
				vmstat.procs_blocked,
				vmstat.swap_total - vmstat.swap_used,
				vmstat.memory_free,
				vmstat.memory_buff,
				vmstat.memory_cache,
				vmstat.swap_in / (uint64_t)vmstat_delay,
				vmstat.swap_out / (uint64_t)vmstat_delay,
				vmstat.block_in / (uint64_t)vmstat_delay,
				vmstat.block_out / (uint64_t)vmstat_delay,
				vmstat.interrupt / (uint64_t)vmstat_delay,
				vmstat.context_switch / (uint64_t)vmstat_delay,
				100.0 * (double)vmstat.user_time / clk_tick_vmstat_delay,
				100.0 * (double)vmstat.system_time / clk_tick_vmstat_delay,
				100.0 * (double)vmstat.idle_time / clk_tick_vmstat_delay,
				100.0 * (double)vmstat.wait_time / clk_tick_vmstat_delay,
				100.0 * (double)vmstat.stolen_time / clk_tick_vmstat_delay);
		}

		if (thermalstat_delay == thermalstat_sleep) {
			double min1, min5, min15, ghz;
			char therms[1 + (tz_num * 7)];
			char cpuspeed[6];
#if defined(__linux__)
			char *ptr;
#endif
			static uint32_t thermalstat_count = 0;

			(void)memset(therms, 0, sizeof(therms));

#if defined(__linux__)
			for (ptr = therms, tz_info = tz_info_list; tz_info; tz_info = tz_info->next) {
				(void)snprintf(ptr, 8, " %6.6s", tz_info->type);
				ptr += 7;
			}
#endif
			if ((thermalstat_count++ % 25) == 0)
				pr_inf("therm:   GHz  LdA1  LdA5 LdA15 %s\n", therms);

#if defined(__linux__)
			for (ptr = therms, tz_info = tz_info_list; tz_info; tz_info = tz_info->next) {
				(void)snprintf(ptr, 8, " %6.2f", stress_get_tz_info(tz_info));
				ptr += 7;
			}
#endif
			ghz = stress_get_cpu_ghz_average();
			if (ghz > 0.0)
				(void)snprintf(cpuspeed, sizeof(cpuspeed), "%5.2f", ghz);
			else
				(void)shim_strlcpy(cpuspeed, "n/a", sizeof(cpuspeed));

			if (stress_get_load_avg(&min1, &min5, &min15) < 0)  {
				pr_inf("therm: %5s %5.5s %5.5s %5.5s %s\n",
					cpuspeed, "n/a", "n/a", "n/a", therms);
			} else {
				pr_inf("therm: %5s %5.2f %5.2f %5.2f %s\n",
					cpuspeed, min1, min5, min15, therms);
			}
		}

#if defined(HAVE_SYS_SYSMACROS_H) &&	\
    defined(__linux__)
		if (iostat_delay == iostat_sleep) {
			double clk_scale = (iostat_delay > 0) ? 1.0 / iostat_delay : 0.0;
			static uint32_t iostat_count = 0;

			if ((iostat_count++ % 25) == 0)
				pr_inf("iostat: Inflght  Rd K/s   Wr K/s Dscd K/s     Rd/s     Wr/s   Dscd/s\n");

			stress_get_iostat(iostat_name, &iostat);
			/* sectors are 512 bytes, so >> 1 to get stats in 1024 bytes */
			pr_inf("iostat %7.0f %8.0f %8.0f %8.0f %8.0f %8.0f %8.0f\n",
				(double)iostat.in_flight * clk_scale,
				(double)(iostat.read_sectors >> 1) * clk_scale,
				(double)(iostat.write_sectors >> 1) * clk_scale,
				(double)(iostat.discard_sectors >> 1) * clk_scale,
				(double)iostat.read_io * clk_scale,
				(double)iostat.write_io * clk_scale,
				(double)iostat.discard_io * clk_scale);
		}
#endif
	}
}

/*
 *  stress_vmstat_stop()
 *	stop vmstat statistics
 */
void stress_vmstat_stop(void)
{
	if (vmstat_pid > 0) {
		int status;

		(void)kill(vmstat_pid, SIGKILL);
		(void)waitpid(vmstat_pid, &status, 0);
	}
}
