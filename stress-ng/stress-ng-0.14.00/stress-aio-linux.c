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

#if defined(HAVE_LIBAIO_H)
#include <libaio.h>
#endif

#if defined(HAVE_POLL_H)
#include <poll.h>
#endif

#define MIN_AIO_LINUX_REQUESTS		(1)
#define MAX_AIO_LINUX_REQUESTS		(4096)
#define DEFAULT_AIO_LINUX_REQUESTS	(64)

#define BUFFER_SZ			(4096)
#define DEFAULT_AIO_MAX_NR		(65536)

static const stress_help_t help[] = {
	{ NULL,	"aiol N",	   "start N workers that exercise Linux async I/O" },
	{ NULL,	"aiol-ops N",	   "stop after N bogo Linux aio async I/O requests" },
	{ NULL,	"aiol-requests N", "number of Linux aio async I/O requests per worker" },
	{ NULL,	NULL,		   NULL }
};

static int stress_set_aio_linux_requests(const char *opt)
{
	uint32_t aio_linux_requests;

	aio_linux_requests = stress_get_uint32(opt);
	stress_check_range("aiol-requests", aio_linux_requests,
		MIN_AIO_LINUX_REQUESTS, MAX_AIO_LINUX_REQUESTS);
	return stress_set_setting("aiol-requests", TYPE_ID_UINT32, &aio_linux_requests);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_aiol_requests,	stress_set_aio_linux_requests },
	{ 0,			NULL }
};

#if defined(HAVE_LIB_AIO) &&		\
    defined(HAVE_LIBAIO_H) &&		\
    defined(HAVE_CLOCK_GETTIME) &&	\
    defined(__NR_io_setup) &&		\
    defined(__NR_io_destroy) &&		\
    defined(__NR_io_submit) &&		\
    defined(__NR_io_getevents)

#if defined(__NR_io_cancel)
static int shim_io_cancel(
	io_context_t ctx_id,
	struct iocb *iocb,
	struct io_event *result)
{
	return (int)syscall(__NR_io_cancel, ctx_id, iocb, result);
}
#endif

#if defined(__NR_io_pgetevents)
struct shim_aio_sigset {
	const sigset_t *sigmask;
	size_t		sigsetsize;
};

static int shim_io_pgetevents(
	io_context_t ctx_id,
	long min_nr,
	long nr,
	struct io_event *events,
	struct timespec *timeout,
	const struct shim_aio_sigset *usig)
{
	return (int)syscall(__NR_io_pgetevents, ctx_id, min_nr, nr, events, timeout, usig);
}
#endif

static int shim_io_setup(unsigned nr_events, io_context_t *ctx_idp)
{
	return (int)syscall(__NR_io_setup, nr_events, ctx_idp);
}

static int shim_io_destroy(io_context_t ctx_id)
{
	return (int)syscall(__NR_io_destroy, ctx_id);
}

static int shim_io_submit(io_context_t ctx_id, long nr, struct iocb **iocbpp)
{
	return (int)syscall(__NR_io_submit, ctx_id, nr, iocbpp);
}

static int shim_io_getevents(
	io_context_t ctx_id,
	long min_nr,
	long nr,
	struct io_event *events,
	struct timespec *timeout)
{
	return (int)syscall(__NR_io_getevents, ctx_id, min_nr, nr, events, timeout);
}

/*
 *  shim_io_getevents_random()
 *	try to use shim_io_pgetevents or shim_io_getevents based on
 *	random choice. If shim_io_pgetevents does not exist, don't
 *	try it again.
 */
static int shim_io_getevents_random(
	const io_context_t ctx_id,
	const long min_nr,
	const long nr,
	struct io_event *const events,
	struct timespec *const timeout)
{
#if defined(__NR_io_pgetevents)
	static bool try_io_pgetevents = true;

	if (try_io_pgetevents && stress_mwc1()) {
		int ret;

		ret = shim_io_pgetevents(ctx_id, min_nr, nr, events, timeout, NULL);
		if (ret >= 0)
			return ret;
		/* system call not wired up? never try again */
		if (errno == ENOSYS)
			try_io_pgetevents = false;
		else
			return ret;
	}
	/* ..fall through and use vanilla io_getevents */
#else
	UNEXPECTED
#endif
	return shim_io_getevents(ctx_id, min_nr, nr, events, timeout);
}

/*
 *  aio_linux_fill_buffer()
 *	fill buffer with some known pattern
 */
static inline void aio_linux_fill_buffer(
	const uint8_t pattern,
	uint8_t *const buffer,
	const size_t size)
{
	register size_t i;
	register uint8_t pat = pattern;

	for (i = 0; i < size; i++, pat++)
		buffer[i] = (uint8_t)pat;
}

/*
 *  aio_linux_check_buffer()
 *	check buffer contains some known pattern
 */
static inline bool aio_linux_check_buffer(
	const int request,
	uint8_t *const buffer,
	const size_t size)
{
	register size_t i;

	for (i = 0; i < size; i++) {
		if (buffer[i] != (uint8_t)(request + (int)i))
			return false;
	}

	return true;
}

/*
 *  stress_aiol_submit()
 *	submit async I/O requests
 */
static int stress_aiol_submit(
	const stress_args_t *args,
	const io_context_t ctx,
	struct iocb *cbs[],
	const size_t n,
	const bool ignore_einval)
{
	do {
		int ret;

		errno = 0;
		ret = shim_io_submit(ctx, (long)n, cbs);
		if (ret >= 0) {
			break;
		} else {
			if ((errno == EINVAL) && ignore_einval)
				return 0;
			if (errno != EAGAIN) {
				pr_fail("%s: io_submit failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				return ret;
			}
		}
	} while (keep_stressing(args));

	return 0;
}

/*
 *  stress_aiol_wait()
 *	wait for async I/O requests to complete
 */
static ssize_t stress_aiol_wait(
	const stress_args_t *args,
	const io_context_t ctx,
	struct io_event events[],
	const size_t n)
{
	size_t i = 0;

	do {
		struct timespec timeout, *timeout_ptr;
		int ret;

		if (clock_gettime(CLOCK_REALTIME, &timeout) < 0) {
			timeout_ptr = NULL;
		} else {
			timeout.tv_nsec += 1000000;
			if (timeout.tv_nsec > STRESS_NANOSECOND) {
				timeout.tv_nsec -= STRESS_NANOSECOND;
				timeout.tv_sec++;
			}
			timeout_ptr = &timeout;
		}

		ret = shim_io_getevents_random(ctx, 1, (long)(n - i), events, timeout_ptr);
		if (ret < 0) {
			if (errno == EINTR) {
				if (keep_stressing_flag()) {
					continue;
				} else {
					return (ssize_t)i;
				}
			}
			pr_fail("%s: io_getevents failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			return -1;
		} else {
			i += (size_t)ret;
		}
	} while ((i < n) && keep_stressing_flag());

	return (ssize_t)i;
}

/*
 *  stress_aiol_alloc()
 *	allocate various arrays and handle free'ing
 *	and error reporting on out of memory errors.
 */
static int stress_aiol_alloc(
	const stress_args_t *args,
	const size_t n,
	uint8_t **buffer,
	struct iocb **cb,
	struct io_event **events,
	struct iocb ***cbs,
	int **fds)
{
	int ret;

	ret = posix_memalign((void **)buffer, 4096, n * BUFFER_SZ);
	if (ret)
		goto err_msg;
	*cb = calloc(n, sizeof(**cb));
	if (!*cb)
		goto free_buffer;
	*events = calloc(n, sizeof(**events));
	if (!*events)
		goto free_cb;
	*cbs = calloc(n, sizeof(**cbs));
	if (!*cbs)
		goto free_events;
	*fds = calloc(n, sizeof(**fds));
	if (!*fds)
		goto free_cbs;
	return 0;

free_cbs:
	free(*cbs);
free_events:
	free(*events);
free_cb:
	free(*cb);
free_buffer:
	free(*buffer);
err_msg:
	pr_inf("%s: out of memory allocating memory, errno=%d (%s)\n",
		args->name, errno, strerror(errno));

	*buffer = NULL;
	*cb = NULL;
	*events = NULL;
	*cbs = NULL;
	*fds = NULL;

	return -1;
}

/*
 *  stress_aiol_free()
 *	free allocated memory
 */
static void stress_aiol_free(
	uint8_t *buffer,
	struct iocb *cb,
	struct io_event *events,
	struct iocb **cbs,
	int *fds)
{
	free(buffer);
	free(cb);
	free(events);
	free(cbs);
	free(fds);
}

/*
 *  stress_aiol
 *	stress asynchronous I/O using the linux specific aio ABI
 */
static int stress_aiol(const stress_args_t *args)
{
	int ret, rc = EXIT_FAILURE;
	int flags = O_DIRECT;
	char filename[PATH_MAX];
	char buf[1];
	io_context_t ctx = 0;
	uint32_t aio_linux_requests = DEFAULT_AIO_LINUX_REQUESTS;
	uint8_t *buffer;
	struct iocb *cb;
	struct io_event *events;
	struct iocb **cbs;
	int *fds;
	uint32_t aio_max_nr = DEFAULT_AIO_MAX_NR;
	int j = 0;
	size_t i;
	int warnings = 0;
#if defined(__NR_io_cancel)
	int bad_fd;
#endif

	if (!stress_get_setting("aiol-requests", &aio_linux_requests)) {
		if (g_opt_flags & OPT_FLAGS_MAXIMIZE)
			aio_linux_requests = MAX_AIO_LINUX_REQUESTS;
		if (g_opt_flags & OPT_FLAGS_MINIMIZE)
			aio_linux_requests = MIN_AIO_LINUX_REQUESTS;
	}
	if ((aio_linux_requests < MIN_AIO_LINUX_REQUESTS) ||
	    (aio_linux_requests > MAX_AIO_LINUX_REQUESTS)) {
		pr_fail("%s: iol_requests out of range", args->name);
		return EXIT_FAILURE;
	}

	if (system_read("/proc/sys/fs/aio-max-nr", buf, sizeof(buf)) > 0) {
		if (sscanf(buf, "%" SCNu32, &aio_max_nr) != 1) {
			/* Guess max */
			aio_max_nr = DEFAULT_AIO_MAX_NR;
		}
	} else {
		/* Guess max */
		aio_max_nr = DEFAULT_AIO_MAX_NR;
	}

	aio_max_nr /= (args->num_instances == 0) ? 1 : args->num_instances;
	if (aio_max_nr < 1)
		aio_max_nr = 1;
	if (aio_linux_requests > aio_max_nr) {
		aio_linux_requests = aio_max_nr;
		if (args->instance == 0)
			pr_inf("%s: Limiting AIO requests to "
				"%" PRIu32 " per stressor (avoids running out of resources)\n",
				args->name, aio_linux_requests);
	}

	if (stress_aiol_alloc(args, aio_linux_requests, &buffer, &cb, &events, &cbs, &fds)) {
		stress_aiol_free(buffer, cb, events, cbs, fds);
		return EXIT_NO_RESOURCE;
	}

	/*
	 * Exercise invalid io_setup syscall
	 * on invalid(zero) nr_events
	 */
	ret = shim_io_setup(0, &ctx);
	if (ret >= 0)
		(void)shim_io_destroy(ctx);

	ret = shim_io_setup(aio_linux_requests, &ctx);
	if (ret < 0) {
		/*
		 *  The libaio interface returns -errno in the
		 *  return value, so set errno accordingly
		 */
		if ((errno == EAGAIN) || (errno == EACCES)) {
			pr_fail("%s: io_setup failed, ran out of "
				"available events, consider increasing "
				"/proc/sys/fs/aio-max-nr, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			rc = EXIT_NO_RESOURCE;
			goto free_memory;
		} else if (errno == ENOMEM) {
			pr_fail("%s: io_setup failed, ran out of "
				"memory, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			rc = EXIT_NO_RESOURCE;
			goto free_memory;
		} else if (errno == ENOSYS) {
			pr_fail("%s: io_setup failed, no io_setup "
				"system call with this kernel, "
				"errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			rc = EXIT_NO_RESOURCE;
			goto free_memory;
		} else {
			pr_fail("%s: io_setup failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			rc = EXIT_FAILURE;
			goto free_memory;
		}
	}
	ret = stress_temp_dir_mk_args(args);
	if (ret < 0) {
		rc = exit_status(-ret);
		goto free_memory;
	}

	(void)stress_temp_filename_args(args,
		filename, sizeof(filename), stress_mwc32());

retry_open:
	fds[0] = open(filename, O_CREAT | O_RDWR | flags, S_IRUSR | S_IWUSR);
	if (fds[0] < 0) {
		if ((flags & O_DIRECT) && (errno == EINVAL)) {
			flags &= ~O_DIRECT;
			goto retry_open;
		}
		rc = exit_status(errno);
		pr_fail("%s: open %s failed, errno=%d (%s)\n",
			args->name, filename, errno, strerror(errno));
		(void)shim_unlink(filename);
		goto finish;
	}

#if defined(__NR_io_cancel)
	bad_fd = stress_get_bad_fd();
#endif

	/*
	 *  Make aio work harder by using lots of different fds on the
	 *  same file. If we can't open a file (e.g. out of file descriptors)
	 *  then use the same fd as fd[0]
	 */
	for (i = 1; i < aio_linux_requests; i++) {
		fds[i] = open(filename, O_RDWR | flags, S_IRUSR | S_IWUSR);
		if (fds[i] < 0)
			fds[i] = fds[0];
	}
	(void)shim_unlink(filename);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		uint8_t *bufptr;
		ssize_t n;
		int64_t off, offset = stress_mwc16() * BUFFER_SZ;

		/*
		 *  async writes
		 */
		(void)memset(cb, 0, aio_linux_requests * sizeof(*cb));
		for (bufptr = buffer, i = 0, off = offset; i < aio_linux_requests; i++, bufptr += BUFFER_SZ, off += BUFFER_SZ) {
			const uint8_t pattern = j + ((((intptr_t)bufptr) >> 12) & 0xff);

			aio_linux_fill_buffer(pattern, bufptr, BUFFER_SZ);

			cb[i].aio_fildes = fds[i];
			cb[i].aio_lio_opcode = IO_CMD_PWRITE;
			cb[i].u.c.buf = bufptr;
			cb[i].u.c.offset = off;
			cb[i].u.c.nbytes = BUFFER_SZ;
			cbs[i] = &cb[i];
		}
		if (stress_aiol_submit(args, ctx, cbs, aio_linux_requests, false) < 0)
			break;
		if (stress_aiol_wait(args, ctx, events, aio_linux_requests) < 0)
			break;
		inc_counter(args);
		if (!keep_stressing(args))
			break;

		/*
		 *  async reads
		 */
		(void)memset(cb, 0, aio_linux_requests * sizeof(*cb));
		for (bufptr = buffer, i = 0, off = offset; i < aio_linux_requests; i++, bufptr += BUFFER_SZ, off += BUFFER_SZ) {
			(void)memset(bufptr, 0, BUFFER_SZ);

			cb[i].aio_fildes = fds[i];
			cb[i].aio_lio_opcode = IO_CMD_PREAD;
			cb[i].u.c.buf = bufptr;
			cb[i].u.c.offset = off;
			cb[i].u.c.nbytes = BUFFER_SZ;
			cbs[i] = &cb[i];
		}

		if (stress_aiol_submit(args, ctx, cbs, aio_linux_requests, false) < 0)
			break;

		n = stress_aiol_wait(args, ctx, events, aio_linux_requests);
		if (n < 0)
			break;

		for (i = 0; i < (size_t)n; i++) {
			uint8_t pattern;
			struct iocb *obj = events[i].obj;

			if (!obj)
				continue;

			bufptr = obj->u.c.buf;
			pattern = j + ((((intptr_t)bufptr) >> 12) & 0xff);

			if (aio_linux_check_buffer(pattern, bufptr, BUFFER_SZ) != true) {
				if (warnings++ < 5) {
					pr_inf("%s: unexpected data mismatch in buffer %zd (maybe a wait timeout issue)\n",
						args->name, i);
					break;
				}
			}
		}

#if defined(__NR_io_cancel)
		{
			static int cancel;

			cancel++;
			if (cancel >= 127) {
				struct io_event event;
				io_context_t bad_ctx;
				struct iocb bad_iocb;
				struct iocb *bad_iocbs[1];
				struct timespec timeout;

				cancel = 0;

				ret = shim_io_cancel(ctx, &cb[0], &event);
				(void)ret;

				/* Exercise with io_cancel invalid context */
				(void)memset(&bad_ctx, stress_mwc8() | 0x1, sizeof(bad_ctx));
				ret = shim_io_cancel(bad_ctx, &cb[0], &event);
				(void)ret;

				/* Exercise with io_invalid iocb */
				bad_iocb.aio_fildes = bad_fd;
				bad_iocb.aio_lio_opcode = ~0;
				bad_iocb.u.c.buf = NULL;
				bad_iocb.u.c.offset = 0;
				bad_iocb.u.c.nbytes = 0;
				ret = shim_io_cancel(ctx, &bad_iocb, &event);
				(void)ret;

				/* Exercise io_destroy with illegal context, EINVAL */
				ret = shim_io_destroy(bad_ctx);
				(void)ret;
				ret = shim_io_destroy(NULL);
				(void)ret;

				/* Exercise io_getevents with illegal context, EINVAL */
				timeout.tv_sec = 0;
				timeout.tv_nsec = 100000;
				ret = shim_io_getevents(bad_ctx, 1, 1, events, &timeout);
				(void)ret;

				/* Exercise io_getevents with illegal min */
				timeout.tv_sec = 0;
				timeout.tv_nsec = 100000;
				ret = shim_io_getevents(ctx, 1, 0, events, &timeout);
				(void)ret;
				ret = shim_io_getevents(ctx, -1, 0, events, &timeout);
				(void)ret;

				/* Exercise io_getevents with illegal nr */
				timeout.tv_sec = 0;
				timeout.tv_nsec = 100000;
				ret = shim_io_getevents(ctx, 0, -1, events, &timeout);
				(void)ret;

				/* Exercise io_getevents with illegal timeout */
				timeout.tv_sec = 0;
				timeout.tv_nsec = ~0L;
				ret = shim_io_getevents(ctx, 0, 1, events, &timeout);
				(void)ret;

				/* Exercise io_setup with illegal nr_events */
				ret = shim_io_setup(0, &bad_ctx);
				if (ret == 0)
					shim_io_destroy(bad_ctx);
				ret = shim_io_setup(INT_MAX, &bad_ctx);
				if (ret == 0)
					shim_io_destroy(bad_ctx);

				/* Exercise io_submit with illegal context */
				bad_iocb.aio_fildes = bad_fd;
				bad_iocb.aio_lio_opcode = ~0;
				bad_iocb.u.c.buf = NULL;
				bad_iocb.u.c.offset = 0;
				bad_iocb.u.c.nbytes = 0;
				bad_iocbs[0] = &bad_iocb;
				ret = shim_io_submit(bad_ctx, 1, bad_iocbs);
				(void)ret;

				/* Exercise io_submit with useless or illegal nr ios */
				ret = shim_io_submit(ctx, 0, bad_iocbs);
				(void)ret;
				ret = shim_io_submit(ctx, -1, bad_iocbs);
				(void)ret;

				/* Exercise io_submit with illegal iocb */
				ret = shim_io_submit(ctx, 1, bad_iocbs);
				(void)ret;
			}
		}
#else
		UNEXPECTED
#endif
		inc_counter(args);
		if (!keep_stressing(args))
			break;

#if defined(HAVE_POLL_H)
		/*
		 *  Exercise aio_poll with illegal settings
		 */
		(void)memset(cb, 0, aio_linux_requests * sizeof(*cb));
		for (i = 0; i < aio_linux_requests; i++) {
			cb[i].aio_fildes = fds[i];
			cb[i].aio_lio_opcode = IO_CMD_POLL;
			cb[i].u.c.buf = (void *)POLLIN;
			/* Set invalid sizes */
			(void)memset(&cb[i].u.c.offset, 0xff, sizeof(cb[i].u.c.offset));
			(void)memset(&cb[i].u.c.nbytes, 0xff, sizeof(cb[i].u.c.nbytes));
			cbs[i] = &cb[i];
		}
		if (stress_aiol_submit(args, ctx, cbs, aio_linux_requests, true) < 0)
			break;
		if (errno == 0)
			(void)stress_aiol_wait(args, ctx, events, aio_linux_requests);
		inc_counter(args);
		if (!keep_stressing(args))
			break;
#else
		UNEXPECTED
#endif

		/*
		 *  Async fdsync and fsync every 256 iterations, older kernels don't
		 *  support these, so don't fail if EINVAL is returned.
		 */
		if (j++ >= 256) {
			static bool do_sync = true;

			j = 0;
			if (do_sync) {
				(void)memset(cb, 0, aio_linux_requests * sizeof(*cb));

				aio_linux_fill_buffer(0, bufptr, BUFFER_SZ);

				cb[0].aio_fildes = fds[0];
				cb[0].aio_lio_opcode = stress_mwc1() ? IO_CMD_FDSYNC : IO_CMD_FSYNC;
				cb[0].u.c.buf = NULL;
				cb[0].u.c.offset = 0;
				cb[0].u.c.nbytes = 0;
				cbs[0] = &cb[0];
				(void)stress_aiol_submit(args, ctx, cbs, 1, true);
				if (errno == 0) {
					(void)stress_aiol_wait(args, ctx, events, 1);
				} else {
					/* Don't try again */
					do_sync = false;
				}
			}
		}
		inc_counter(args);
	} while (keep_stressing(args));

	rc = EXIT_SUCCESS;

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)close(fds[0]);
	for (i = 1; i < aio_linux_requests; i++) {
		if (fds[i] != fds[0])
			(void)close(fds[i]);
	}
finish:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)shim_io_destroy(ctx);
	(void)stress_temp_dir_rm_args(args);

free_memory:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	stress_aiol_free(buffer, cb, events, cbs, fds);
	return rc;
}

stressor_info_t stress_aiol_info = {
	.stressor = stress_aiol,
	.class = CLASS_IO | CLASS_INTERRUPT | CLASS_OS,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_aiol_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_IO | CLASS_INTERRUPT | CLASS_OS,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
#endif
