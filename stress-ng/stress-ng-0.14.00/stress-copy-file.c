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

#define MIN_COPY_FILE_BYTES	(128 * MB)
#define MAX_COPY_FILE_BYTES	(MAX_FILE_LIMIT)
#define DEFAULT_COPY_FILE_BYTES	(256 * MB)
#define DEFAULT_COPY_FILE_SIZE	(2 * MB)

static const stress_help_t help[] = {
	{ NULL,	"copy-file N",		"start N workers that copy file data" },
	{ NULL,	"copy-file-ops N",	"stop after N copy bogo operations" },
	{ NULL,	"copy-file-bytes N",	"specify size of file to be copied" },
	{ NULL,	NULL,			NULL }

};

static int stress_set_copy_file_bytes(const char *opt)
{
	uint64_t copy_file_bytes;

	copy_file_bytes = stress_get_uint64_byte_filesystem(opt, 1);
	stress_check_range_bytes("copy-file-bytes", copy_file_bytes,
		MIN_COPY_FILE_BYTES, MAX_COPY_FILE_BYTES);
	return stress_set_setting("copy-file-bytes", TYPE_ID_UINT64, &copy_file_bytes);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_copy_file_bytes,	stress_set_copy_file_bytes },
	{ 0,			NULL }
};

#if defined(HAVE_COPY_FILE_RANGE)

/*
 *  stress_copy_file_range_verify()
 *	verify copy file from fd_in to fd_out worked correctly for
 *	--verify option
 */
static int stress_copy_file_range_verify(
	const int fd_in,
	off_t *off_in,
	const int fd_out,
	off_t *off_out,
	const ssize_t bytes)
{
	ssize_t n, bytes_in, bytes_out, bytes_left = bytes;
	ssize_t buf_sz = STRESS_MINIMUM(bytes, 1024);
	off_t off_ret;


	while (bytes_left >= 0) {
		char buf_in[buf_sz], buf_out[buf_sz];

		off_ret = lseek(fd_in, *off_in, SEEK_SET);
		if (off_ret != *off_in)
			return -1;
		off_ret = lseek(fd_out, *off_out, SEEK_SET);
		if (off_ret != *off_out)
			return -1;

		bytes_in = read(fd_in, buf_in, sizeof(buf_in));
		if (bytes_in <= 0)
			break;
		bytes_out = read(fd_out, buf_out, sizeof(buf_out));
		if (bytes_out <= 0)
			break;

		n = STRESS_MINIMUM(bytes_in, bytes_out);
		if (memcmp(buf_in, buf_out, n) != 0)
			return -1;
		bytes_left -= n;
		*off_in += n;
		*off_out += n;
	}
	if (bytes_left > 0)
		return -1;
	return 0;
}

/*
 *  stress_copy_file
 *	stress reading chunks of file using copy_file_range()
 */
static int stress_copy_file(const stress_args_t *args)
{
	int fd_in, fd_out, rc = EXIT_FAILURE, ret;
	const int fd_bad = stress_get_bad_fd();
	char filename[PATH_MAX - 5], tmp[PATH_MAX];
	uint64_t copy_file_bytes = DEFAULT_COPY_FILE_BYTES;

	if (!stress_get_setting("copy-file-bytes", &copy_file_bytes)) {
		if (g_opt_flags & OPT_FLAGS_MAXIMIZE)
			copy_file_bytes = MAX_COPY_FILE_BYTES;
		if (g_opt_flags & OPT_FLAGS_MINIMIZE)
			copy_file_bytes = MIN_COPY_FILE_BYTES;
	}

	copy_file_bytes /= args->num_instances;
	if (copy_file_bytes < DEFAULT_COPY_FILE_SIZE)
		copy_file_bytes = DEFAULT_COPY_FILE_SIZE * 2;
	if (copy_file_bytes < MIN_COPY_FILE_BYTES)
		copy_file_bytes = MIN_COPY_FILE_BYTES;

        ret = stress_temp_dir_mk_args(args);
        if (ret < 0)
                return exit_status(-ret);

	(void)stress_temp_filename_args(args,
			filename, sizeof(filename), stress_mwc32());
	(void)snprintf(tmp, sizeof(tmp), "%s-orig", filename);
	if ((fd_in = open(tmp, O_CREAT | O_RDWR,  S_IRUSR | S_IWUSR)) < 0) {
		rc = exit_status(errno);
		pr_fail("%s: open %s failed, errno=%d (%s)\n",
			args->name, tmp, errno, strerror(errno));
		goto tidy_dir;
	}
	(void)shim_unlink(tmp);
	if (ftruncate(fd_in, (off_t)copy_file_bytes) < 0) {
		rc = exit_status(errno);
		pr_fail("%s: ftruncated failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		goto tidy_in;
	}
	if (shim_fsync(fd_in) < 0) {
		pr_fail("%s: fsync failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		goto tidy_in;
	}

	(void)snprintf(tmp, sizeof(tmp), "%s-copy", filename);
	if ((fd_out = open(tmp, O_CREAT | O_RDWR,  S_IRUSR | S_IWUSR)) < 0) {
		rc = exit_status(errno);
		pr_fail("%s: open %s failed, errno=%d (%s)\n",
			args->name, tmp, errno, strerror(errno));
		goto tidy_in;
	}
	(void)shim_unlink(tmp);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		ssize_t copy_ret;
		shim_loff_t off_in, off_out, off_in_orig, off_out_orig;

		off_in_orig = (shim_loff_t)(stress_mwc64() % (copy_file_bytes - DEFAULT_COPY_FILE_SIZE));
		off_in = off_in_orig;
		off_out_orig = (shim_loff_t)(stress_mwc64() % (copy_file_bytes - DEFAULT_COPY_FILE_SIZE));
		off_out = off_out_orig;

		copy_ret = shim_copy_file_range(fd_in, &off_in, fd_out,
						&off_out, DEFAULT_COPY_FILE_SIZE, 0);
		if (copy_ret < 0) {
			if ((errno == EAGAIN) ||
			    (errno == EINTR) ||
			    (errno == ENOSPC))
				continue;
			if (errno == EINVAL) {
				pr_inf_skip("%s: copy_file_range failed, the "
					"kernel splice may be not implemented "
					"for the file system, skipping stressor.\n",
					args->name);
				rc = EXIT_NO_RESOURCE;
				goto tidy_out;
			}
			pr_fail("%s: copy_file_range failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto tidy_out;
		}
		if (g_opt_flags & OPT_FLAGS_VERIFY) {
			copy_ret = stress_copy_file_range_verify(fd_in, &off_in_orig,
					fd_out, &off_out_orig, DEFAULT_COPY_FILE_SIZE);
			if (copy_ret < 0) {
				pr_fail("%s: copy_file_range verify failed, input offset=%jd, output offset=%jd\n",
					args->name, (intmax_t)off_in_orig, off_out_orig);
			}
		}

		/*
		 *  Exercise with bad fds
		 */
		copy_ret = shim_copy_file_range(fd_bad, &off_in, fd_out,
						&off_out, DEFAULT_COPY_FILE_SIZE, 0);
		(void)copy_ret;
		copy_ret = shim_copy_file_range(fd_in, &off_in, fd_bad,
						&off_out, DEFAULT_COPY_FILE_SIZE, 0);
		(void)copy_ret;
		copy_ret = shim_copy_file_range(fd_out, &off_in, fd_in,
						&off_out, DEFAULT_COPY_FILE_SIZE, 0);
		(void)copy_ret;
		/*
		 *  Exercise with bad flags
		 */
		copy_ret = shim_copy_file_range(fd_in, &off_in, fd_out,
						&off_out, DEFAULT_COPY_FILE_SIZE, ~0);
		(void)copy_ret;

		(void)shim_fsync(fd_out);
		inc_counter(args);
	} while (keep_stressing(args));
	rc = EXIT_SUCCESS;

tidy_out:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)close(fd_out);
tidy_in:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)close(fd_in);
tidy_dir:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)stress_temp_dir_rm_args(args);

	return rc;
}

stressor_info_t stress_copy_file_info = {
	.stressor = stress_copy_file,
	.class = CLASS_FILESYSTEM | CLASS_OS,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_OPTIONAL,
	.help = help
};
#else
stressor_info_t stress_copy_file_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_FILESYSTEM | CLASS_OS,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
#endif
