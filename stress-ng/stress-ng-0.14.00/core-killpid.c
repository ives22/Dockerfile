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

/*
 *  stress_killpid()
 *	kill a process with SIGKILL. Try and release memory
 *	as soon as possible using process_mrelease for the
 *	Linux case.
 */
int stress_killpid(const pid_t pid)
{
#if defined(__linux__) && 		\
    defined(__NR_process_release)
	int pidfd, ret;

	pidfd = shim_pidfd_open(pid, 0);
	ret = kill(pid, SIGKILL);

	if (pidfd >= 0) {
		int saved_errno = errno;

		if (ret == 0)
			(void)shim_process_mrelease(pidfd, 0);
		(void)close(pidfd);

		errno = saved_errno;
	}
	return ret;
#else
	return kill(pid, SIGKILL);
#endif
}
