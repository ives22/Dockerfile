/*
 * Copyright (C)      2022 Colin Ian King
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
#ifndef CORE_NET_H
#define CORE_NET_H

/* Network domains flags */
#define DOMAIN_INET		(0x00000001)	/* AF_INET */
#define DOMAIN_INET6		(0x00000002)	/* AF_INET6 */
#define DOMAIN_UNIX		(0x00000004)	/* AF_UNIX */

#define DOMAIN_INET_ALL		(DOMAIN_INET | DOMAIN_INET6)
#define DOMAIN_ALL		(DOMAIN_INET | DOMAIN_INET6 | DOMAIN_UNIX)

#define NET_ADDR_ANY		(0)
#define NET_ADDR_LOOPBACK	(1)

/* Network helpers */
extern void stress_set_net_port(const char *optname, const char *opt,
	const int min_port, const int max_port, int *port);
extern WARN_UNUSED int stress_set_net_domain(const int domain_mask,
	const char *name, const char *domain_name, int *domain);
extern void stress_set_sockaddr(const char *name, const uint32_t instance,
	const pid_t ppid, const int domain, const int port,
	struct sockaddr **sockaddr, socklen_t *len, const int net_addr);
extern void stress_set_sockaddr_port(const int domain, const int port,
	struct sockaddr *sockaddr);

#endif
