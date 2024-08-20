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

#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#undef HAVE_ATTR_XATTR_H
#elif defined(HAVE_ATTR_XATTR_H)
#include <attr/xattr.h>
#endif
/*  Sanity check */
#if defined(HAVE_SYS_XATTR_H) &&        \
    defined(HAVE_ATTR_XATTR_H)
#error cannot have both HAVE_SYS_XATTR_H and HAVE_ATTR_XATTR_H
#endif

static const stress_help_t help[] = {
	{ NULL,	"xattr N",	"start N workers stressing file extended attributes" },
	{ NULL,	"xattr-ops N",	"stop after N bogo xattr operations" },
	{ NULL,	NULL,		NULL }
};

#if (defined(HAVE_SYS_XATTR_H) ||	\
     defined(HAVE_ATTR_XATTR_H)) &&	\
    defined(HAVE_FGETXATTR) &&		\
    defined(HAVE_FLISTXATTR) &&		\
    defined(HAVE_FREMOVEXATTR) &&	\
    defined(HAVE_FSETXATTR) &&		\
    defined(HAVE_GETXATTR) &&		\
    defined(HAVE_LISTXATTR) &&		\
    defined(HAVE_SETXATTR)

#define MAX_XATTRS		(4096)

/*
 *  stress_xattr
 *	stress the xattr operations
 */
static int stress_xattr(const stress_args_t *args)
{
	int ret, fd, rc = EXIT_FAILURE;
	const int bad_fd = stress_get_bad_fd();
	char filename[PATH_MAX];
	char bad_filename[PATH_MAX + 4];
	char *hugevalue = NULL;
#if defined(XATTR_SIZE_MAX)
	const size_t hugevalue_sz = XATTR_SIZE_MAX + 16;
	char *large_tmp;
#else
	const size_t hugevalue_sz = 256 * KB;
#endif

#if defined(XATTR_SIZE_MAX)
	large_tmp = calloc(XATTR_SIZE_MAX + 2, sizeof(*large_tmp));
	if (!large_tmp) {
		pr_inf("%s: failed to allocate large xattr buffer\n", args->name);
		return EXIT_NO_RESOURCE;
	}
#endif

	ret = stress_temp_dir_mk_args(args);
	if (ret < 0) {
		rc = exit_status(-ret);
		goto out_free;
	}

	(void)stress_temp_filename_args(args, filename, sizeof(filename), stress_mwc32());
	if ((fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
		rc = exit_status(errno);
		pr_fail("%s: open %s failed, errno=%d (%s)\n",
			args->name, filename, errno, strerror(errno));
		goto out;
	}
	(void)snprintf(bad_filename, sizeof(bad_filename), "%s_bad", filename);

	hugevalue = calloc(1, hugevalue_sz);
	if (hugevalue)
		(void)memset(hugevalue, 'X', hugevalue_sz - 1);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		int i, j;
		char attrname[32];
		char value[32];
		char tmp[sizeof(value)];
		char small_tmp[1];
		ssize_t sret;
		char *buffer;
		char bad_attrname[32];

		for (i = 0; i < MAX_XATTRS; i++) {
			(void)snprintf(attrname, sizeof(attrname), "user.var_%d", i);
			(void)snprintf(value, sizeof(value), "orig-value-%d", i);

			ret = shim_fsetxattr(fd, attrname, value, strlen(value), XATTR_CREATE);
			if (ret < 0) {
				if ((errno == ENOTSUP) || (errno == ENOSYS)) {
					if (args->instance == 0)
						pr_inf_skip("%s stressor will be "
							"skipped, filesystem does not "
							"support xattr.\n", args->name);
					rc = EXIT_NO_RESOURCE;
					goto out_close;
				}
				if ((errno == ENOSPC) || (errno == EDQUOT) || (errno == E2BIG))
					break;
				pr_fail("%s: fsetxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
			if (!keep_stressing(args))
				goto out_finished;
		}

		(void)snprintf(attrname, sizeof(attrname), "user.var_%d", MAX_XATTRS);
		(void)snprintf(value, sizeof(value), "orig-value-%d", MAX_XATTRS);

		/*
		 *  Exercise bad/invalid fd
		 */
		ret = shim_fsetxattr(bad_fd, attrname, value, strlen(value), XATTR_CREATE);
		(void)ret;

		/*
		 *  Exercise invalid flags
		 */
		ret = shim_fsetxattr(fd, attrname, value, strlen(value), ~0);
		if (ret >= 0) {
			pr_fail("%s: fsetxattr unexpectedly succeeded on invalid flags, "
				"errno=%d (%s)\n", args->name, errno, strerror(errno));
			goto out_close;
		}

#if defined(HAVE_LSETXATTR)
		ret = shim_lsetxattr(filename, attrname, value, strlen(value), ~0);
		if (ret >= 0) {
			pr_fail("%s: lsetxattr unexpectedly succeeded on invalid flags, "
				"errno=%d (%s)\n", args->name, errno, strerror(errno));
			goto out_close;
		}
#endif
		ret = shim_setxattr(filename, attrname, value, strlen(value), ~0);
		if (ret >= 0) {
			pr_fail("%s: setxattr unexpectedly succeeded on invalid flags, "
				"errno=%d (%s)\n", args->name, errno, strerror(errno));
			goto out_close;
		}
		/* Exercise invalid filename, ENOENT */
		ret = shim_setxattr("", attrname, value, strlen(value), 0);
		(void)ret;

		/* Exercise invalid attrname, ERANGE */
		ret = shim_setxattr(filename, "", value, strlen(value), 0);
		(void)ret;

		/* Exercise huge value length, E2BIG */
		if (hugevalue) {
			ret = shim_setxattr(filename, "hugevalue", hugevalue, hugevalue_sz, 0);
			(void)ret;
		}

		/*
		 * Check fsetxattr syscall cannot succeed in replacing
		 * attribute name and value pair which doesn't exist
		 */
		ret = shim_fsetxattr(fd, attrname, value, strlen(value),
			XATTR_REPLACE);
		if (ret >= 0) {
			pr_fail("%s: fsetxattr succeeded unexpectedly, "
				"replaced attribute which "
				"doesn't exist, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}

#if defined(HAVE_LSETXATTR)
		ret = shim_lsetxattr(filename, attrname, value, strlen(value),
			XATTR_REPLACE);
		if (ret >= 0) {
			pr_fail("%s: lsetxattr succeeded unexpectedly, "
				"replaced attribute which "
				"doesn't exist, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif
		ret = shim_setxattr(filename, attrname, value, strlen(value),
			XATTR_REPLACE);
		if (ret >= 0) {
			pr_fail("%s: setxattr succeeded unexpectedly, "
				"replaced attribute which "
				"doesn't exist, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}

#if defined(XATTR_SIZE_MAX)
		/* Exercise invalid size argument fsetxattr syscall */
		(void)memset(large_tmp, 'f', XATTR_SIZE_MAX + 1);
		ret = shim_fsetxattr(fd, attrname, large_tmp, XATTR_SIZE_MAX + 1,
			XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: fsetxattr succeeded unexpectedly, "
				"created attribute with size greater "
				"than permitted size, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif

#if defined(HAVE_LSETXATTR) && \
    defined(XATTR_SIZE_MAX)
		(void)memset(large_tmp, 'l', XATTR_SIZE_MAX + 1);
		ret = shim_lsetxattr(filename, attrname, large_tmp,
			XATTR_SIZE_MAX + 1, XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: lsetxattr succeeded unexpectedly, "
				"created attribute with size greater "
				"than permitted size, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif

#if defined(XATTR_SIZE_MAX)
		(void)memset(large_tmp, 's', XATTR_SIZE_MAX + 1);
		ret = shim_setxattr(filename, attrname, large_tmp,
			XATTR_SIZE_MAX + 1, XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: setxattr succeeded unexpectedly, "
				"created attribute with size greater "
				"than permitted size, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif

		/*
		 * Check fsetxattr syscall cannot succeed in creating
		 * attribute name and value pair which already exist
		 */
		(void)snprintf(attrname, sizeof(attrname), "user.var_%d", 0);
		(void)snprintf(value, sizeof(value), "orig-value-%d", 0);
		ret = shim_fsetxattr(fd, attrname, value, strlen(value),
			XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: fsetxattr succeeded unexpectedly, "
				"created attribute which "
				"already exists, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}

#if defined(HAVE_LSETXATTR)
		ret = shim_lsetxattr(filename, attrname, value, strlen(value),
			XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: lsetxattr succeeded unexpectedly, "
				"created attribute which "
				"already exists, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif
		ret = shim_setxattr(filename, attrname, value, strlen(value),
			XATTR_CREATE);
		if (ret >= 0) {
			pr_fail("%s: setxattr succeeded unexpectedly, "
				"created attribute which "
				"already exists, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}

		for (j = 0; j < i; j++) {
			(void)snprintf(attrname, sizeof(attrname), "user.var_%d", j);
			(void)snprintf(value, sizeof(value), "value-%d", j);

			ret = shim_fsetxattr(fd, attrname, value, strlen(value),
				XATTR_REPLACE);
			if (ret < 0) {
				if ((errno == ENOSPC) || (errno == EDQUOT) || (errno == E2BIG))
					break;
				pr_fail("%s: fsetxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}

			/* ..and do it again using setxattr */
			ret = shim_setxattr(filename, attrname, value, strlen(value),
				XATTR_REPLACE);
			if (ret < 0) {
				if ((errno == ENOSPC) || (errno == EDQUOT) || (errno == E2BIG))
					break;
				pr_fail("%s: setxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}

#if defined(HAVE_LSETXATTR)
			/* Although not a link, it's good to exercise this call */
			ret = shim_lsetxattr(filename, attrname, value, strlen(value),
				XATTR_REPLACE);
			if (ret < 0) {
				if ((errno == ENOSPC) || (errno == EDQUOT) || (errno == E2BIG))
					break;
				pr_fail("%s: lsetxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
#endif
			if (!keep_stressing(args))
				goto out_finished;
		}
		for (j = 0; j < i; j++) {
			(void)snprintf(attrname, sizeof(attrname), "user.var_%d", j);
			(void)snprintf(value, sizeof(value), "value-%d", j);

			(void)memset(tmp, 0, sizeof(tmp));
			sret = shim_fgetxattr(fd, attrname, tmp, sizeof(tmp));
			if (sret < 0) {
				pr_fail("%s: fgetxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
			if (strncmp(value, tmp, (size_t)sret)) {
				pr_fail("%s: fgetxattr values different %.*s vs %.*s\n",
					args->name, ret, value, ret, tmp);
				goto out_close;
			}

			/* Exercise getxattr syscall having small value buffer */
			sret = shim_getxattr(filename, attrname, small_tmp, sizeof(small_tmp));
			(void)sret;
			sret = shim_getxattr(filename, "", small_tmp, 0);
			(void)sret;
			sret = shim_getxattr(filename, "", small_tmp, sizeof(small_tmp));
			(void)sret;

			sret = shim_getxattr(filename, attrname, tmp, sizeof(tmp));
			if (sret < 0) {
				pr_fail("%s: getxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
			if (strncmp(value, tmp, (size_t)sret)) {
				pr_fail("%s: getxattr values different %.*s vs %.*s\n",
					args->name, ret, value, ret, tmp);
				goto out_close;
			}

#if defined(HAVE_LGETXATTR)
			sret = shim_lgetxattr(filename, attrname, tmp, sizeof(tmp));
			if (sret < 0) {
				pr_fail("%s: lgetxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
			if (strncmp(value, tmp, (size_t)sret)) {
				pr_fail("%s: lgetxattr values different %.*s vs %.*s\n",
					args->name, ret, value, ret, tmp);
				goto out_close;
			}

			/* Invalid attribute name */
			(void)memset(&bad_attrname, 0, sizeof(bad_attrname));
			sret = shim_lgetxattr(filename, bad_attrname, tmp, sizeof(tmp));
			(void)sret;
#endif
			if (!keep_stressing(args))
				goto out_finished;
		}

		/*
		 *  Exercise bad/invalid fd
		 */
		sret = shim_fgetxattr(bad_fd, "user.var_bad", tmp, sizeof(tmp));
		(void)sret;

		/* Invalid attribute name */
		(void)memset(&bad_attrname, 0, sizeof(bad_attrname));
		sret = shim_fgetxattr(fd, bad_attrname, tmp, sizeof(tmp));
		(void)sret;

		/* Exercise fgetxattr syscall having small value buffer */
		sret = shim_fgetxattr(fd, attrname, small_tmp, sizeof(small_tmp));
		(void)sret;

		/* Determine how large a buffer we required... */
		sret = shim_flistxattr(fd, NULL, 0);
		if (sret < 0) {
			pr_fail("%s: flistxattr failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
		buffer = malloc((size_t)sret);
		if (buffer) {
			/* ...and fetch */
			sret = shim_listxattr(filename, buffer, (size_t)sret);
			free(buffer);

			if (sret < 0) {
				pr_fail("%s: listxattr failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto out_close;
			}
		}

		/*
		 *  Exercise bad/invalid fd
		 */
		sret = shim_flistxattr(bad_fd, NULL, 0);
		(void)sret;

		/*
		 *  Exercise invalid path, ENOENT
		 */
		sret = shim_listxattr(bad_filename, NULL, 0);
		(void)sret;
#if defined(HAVE_LLISTXATTR)
		sret = shim_llistxattr(bad_filename, NULL, 0);
		(void)sret;
#endif

		for (j = 0; j < i; j++) {
			char *errmsg;

			(void)snprintf(attrname, sizeof(attrname), "user.var_%d", j);

			switch (j % 3) {
			case 0:
				ret = shim_fremovexattr(fd, attrname);
				errmsg = "fremovexattr";
				break;
#if defined(HAVE_LREMOVEXATTR)
			case 1:
				ret = shim_lremovexattr(filename, attrname);
				errmsg = "lremovexattr";
				break;
#endif
			default:
				ret = shim_removexattr(filename, attrname);
				errmsg = "removexattr";
				break;
			}
			if (ret < 0) {
				pr_fail("%s: %s failed, errno=%d (%s)\n",
					args->name, errmsg, errno, strerror(errno));
				goto out_close;
			}
			if (!keep_stressing(args))
				goto out_finished;
		}
		/*
		 *  Exercise invalid filename, ENOENT
		 */
		ret = shim_removexattr("", "user.var_1234");
		(void)ret;
		ret = shim_lremovexattr("", "user.var_1234");
		(void)ret;

#if defined(XATTR_SIZE_MAX)
		/*
		 *  Exercise long attribute, ERANGE
		 */
		(void)memset(large_tmp, 'X', XATTR_SIZE_MAX + 1);
		ret = shim_removexattr(filename, large_tmp);
		(void)ret;
		ret = shim_lremovexattr(filename, large_tmp);
		(void)ret;
		ret = shim_fremovexattr(fd, large_tmp);
		(void)ret;
#endif

		/*
		 *  Exercise bad/invalid fd
		 */
		ret = shim_fremovexattr(bad_fd, "user.var_bad");
		(void)ret;

#if defined(HAVE_LLISTXATTR)
		sret = shim_llistxattr(filename, NULL, 0);
		if (sret < 0) {
			pr_fail("%s: llistxattr failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			goto out_close;
		}
#endif
		inc_counter(args);
	} while (keep_stressing(args));

out_finished:
	rc = EXIT_SUCCESS;
out_close:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)close(fd);
out:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	free(hugevalue);
	(void)shim_unlink(filename);
	(void)stress_temp_dir_rm_args(args);
out_free:
#if defined(XATTR_SIZE_MAX)
	free(large_tmp);
#endif
	return rc;
}

stressor_info_t stress_xattr_info = {
	.stressor = stress_xattr,
	.class = CLASS_FILESYSTEM | CLASS_OS,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_xattr_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_FILESYSTEM | CLASS_OS,
	.help = help
};
#endif
