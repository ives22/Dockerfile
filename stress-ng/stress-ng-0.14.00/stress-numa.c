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

#if defined(HAVE_LINUX_MEMPOLICY_H)
#include <linux/mempolicy.h>
#endif

static const stress_help_t help[] = {
	{ NULL,	"numa N",	"start N workers stressing NUMA interfaces" },
	{ NULL,	"numa-ops N",	"stop after N NUMA bogo operations" },
	{ NULL,	NULL,		NULL }
};

#if defined(__NR_get_mempolicy) &&	\
    defined(__NR_mbind) &&		\
    defined(__NR_migrate_pages) &&	\
    defined(__NR_move_pages) &&		\
    defined(__NR_set_mempolicy)

#define BITS_PER_BYTE		(8)
#define NUMA_LONG_BITS		(sizeof(unsigned long) * BITS_PER_BYTE)

#if !defined(MPOL_DEFAULT)
#define MPOL_DEFAULT		(0)
#endif
#if !defined(MPOL_PREFERRED)
#define MPOL_PREFERRED		(1)
#endif
#if !defined(MPOL_BIND)
#define MPOL_BIND		(2)
#endif
#if !defined(MPOL_INTERLEAVE)
#define MPOL_INTERLEAVE		(3)
#endif
#if !defined(MPOL_LOCAL)
#define MPOL_LOCAL		(4)
#endif
#if !defined(MPOL_PREFERRED_MANY)
#define MPOL_PREFERRED_MANY	(5)
#endif

#if !defined(MPOL_F_NODE)
#define MPOL_F_NODE		(1 << 0)
#endif
#if !defined(MPOL_F_ADDR)
#define MPOL_F_ADDR		(1 << 1)
#endif
#if !defined(MPOL_F_MEMS_ALLOWED)
#define MPOL_F_MEMS_ALLOWED	(1 << 2)
#endif

#if !defined(MPOL_MF_STRICT)
#define MPOL_MF_STRICT		(1 << 0)
#endif
#if !defined(MPOL_MF_MOVE)
#define MPOL_MF_MOVE		(1 << 1)
#endif
#if !defined(MPOL_MF_MOVE_ALL)
#define MPOL_MF_MOVE_ALL	(1 << 2)
#endif

#if !defined(MPOL_F_NUMA_BALANCING)
#define MPOL_F_NUMA_BALANCING	(1 << 13)
#endif
#if !defined(MPOL_F_RELATIVE_NODES)
#define MPOL_F_RELATIVE_NODES	(1 << 14)
#endif
#if !defined(MPOL_F_STATIC_NODES)
#define MPOL_F_STATIC_NODES	(1 << 15)
#endif

#define MMAP_SZ			(4 * MB)

typedef struct stress_node {
	struct stress_node	*next;
	unsigned long		node_id;
} stress_node_t;

/*
 *  stress_numa_free_nodes()
 *	free circular list of node info
 */
static void stress_numa_free_nodes(stress_node_t *nodes)
{
	stress_node_t *n = nodes;

	while (n) {
		stress_node_t *next = n->next;

		free(n);
		n = next;

		if (n == nodes)
			break;
	}
}

/*
 *  hex_to_int()
 *	convert ASCII hex digit to integer
 */
static inline int hex_to_int(const char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'F' + 10;
	return -1;
}

/*
 *  stress_numa_get_mem_nodes(void)
 *	collect number of NUMA memory nodes, add them to a
 *	circular linked list - also, return maximum number
 *	of nodes
 */
static long stress_numa_get_mem_nodes(
	stress_node_t **node_ptr,
	unsigned long *max_nodes)
{
	FILE *fp;
	long n = 0;
	unsigned long node_id = 0;
	stress_node_t *tail = NULL;
	char buffer[8192], *str = NULL, *ptr;

	*node_ptr = NULL;
	fp = fopen("/proc/self/status", "r");
	if (!fp)
		return -1;

	while (fgets(buffer, sizeof(buffer), fp)) {
		if (!strncmp(buffer, "Mems_allowed:", 13)) {
			str = buffer + 13;
			break;
		}
	}
	(void)fclose(fp);

	if (!str)
		return -1;

	ptr = buffer + strlen(buffer) - 2;

	/*
	 *  Parse hex digits into NUMA node ids, these
	 *  are listed with least significant node last
	 *  so we need to scan backwards from the end of
	 *  the string back to the start.
	 */
	while ((*ptr != ' ') && (ptr > str)) {
		int val, i;

		/* Skip commas */
		if (*ptr == ',') {
			ptr--;
			continue;
		}

		val = hex_to_int(*ptr);
		if (val < 0)
			return -1;

		/* Each hex digit represent 4 memory nodes */
		for (i = 0; i < 4; i++) {
			if (val & (1 << i)) {
				stress_node_t *node = calloc(1, sizeof(*node));
				if (!node)
					return -1;
				node->node_id = node_id;
				node->next = *node_ptr;
				*node_ptr = node;
				if (!tail)
					tail = node;
				tail->next = node;
				n++;
			}
			node_id++;
		}
		ptr--;
	}

	*max_nodes = node_id;
	return n;
}

/*
 *  stress_numa()
 *	stress the Linux NUMA interfaces
 */
static int stress_numa(const stress_args_t *args)
{
	long numa_nodes;
	unsigned long max_nodes;
	const unsigned long lbits = NUMA_LONG_BITS;
	const unsigned long page_size = args->page_size;
	const unsigned long num_pages = MMAP_SZ / page_size;
	uint8_t *buf;
	stress_node_t *n;
	int rc = EXIT_FAILURE;
	const bool cap_sys_nice = stress_check_capability(SHIM_CAP_SYS_NICE);

	numa_nodes = stress_numa_get_mem_nodes(&n, &max_nodes);
	if (numa_nodes < 1) {
		pr_inf("%s: no NUMA nodes found, "
			"aborting test\n", args->name);
		rc = EXIT_NO_RESOURCE;
		goto numa_free;
	}
	if (!args->instance) {
		pr_inf("%s: system has %lu of a maximum %lu memory NUMA nodes\n",
			args->name, numa_nodes, max_nodes);
	}

	/*
	 *  We need a buffer to migrate around NUMA nodes
	 */
	buf = mmap(NULL, MMAP_SZ, PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	if (buf == MAP_FAILED) {
		rc = exit_status(errno);
		pr_fail("%s: mmap'd region of %zu bytes failed\n",
			args->name, (size_t)MMAP_SZ);
		goto numa_free;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		int j, mode, ret, status[num_pages], dest_nodes[num_pages];
		long lret;
		unsigned long i, node_mask[lbits], old_node_mask[lbits];
		unsigned long max_node_id_count;
		void *pages[num_pages];
		uint8_t *ptr;
		stress_node_t *n_tmp;
		unsigned cpu, curr_node;
		struct shim_getcpu_cache cache;

		(void)memset(node_mask, 0, sizeof(node_mask));

		/*
		 *  Fetch memory policy
		 */
		ret = shim_get_mempolicy(&mode, node_mask, max_nodes,
					 buf, MPOL_F_ADDR);
		if (ret < 0) {
			if (errno != ENOSYS) {
				pr_fail("%s: get_mempolicy failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto err;
			}
		}

		/* Exercise invalid max_nodes */
		ret = shim_get_mempolicy(&mode, node_mask, 0,
					 buf, MPOL_F_NODE);
		(void)ret;

		/* Exercise invalid flag */
		ret = shim_get_mempolicy(&mode, node_mask, max_nodes, buf, ~0UL);
		(void)ret;

		/* Exercise invalid NULL addr condition */
		ret = shim_get_mempolicy(&mode, node_mask, max_nodes,
					 NULL, MPOL_F_ADDR);
		(void)ret;

		ret = shim_get_mempolicy(&mode, node_mask, max_nodes,
					 buf, MPOL_F_NODE);
		(void)ret;

		/* Exercise MPOL_F_MEMS_ALLOWED flag syscalls */
		ret = shim_get_mempolicy(&mode, node_mask, max_nodes,
					 buf, MPOL_F_MEMS_ALLOWED);
		(void)ret;

		ret = shim_get_mempolicy(&mode, node_mask, max_nodes,
					 buf, MPOL_F_MEMS_ALLOWED | MPOL_F_NODE);
		(void)ret;

		if (!keep_stressing_flag())
			break;

		ret = shim_set_mempolicy(MPOL_PREFERRED, NULL, max_nodes);
		if (ret < 0) {
			if (errno != ENOSYS) {
				pr_fail("%s: set_mempolicy failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto err;
			}
		}

		(void)memset(buf, 0xff, MMAP_SZ);
		if (!keep_stressing_flag())
			break;

		/* Create a mix of _NONES options, invalid ones too */
		mode = 0;
#if defined(MPOL_F_STATIC_NODES)
		if (stress_mwc1())
			mode |= MPOL_F_STATIC_NODES;
#endif
#if defined(MPOL_F_RELATIVE_NODES)
		if (stress_mwc1())
			mode |= MPOL_F_RELATIVE_NODES;
#endif

		switch (stress_mwc8() % 11) {
		case 0:
#if defined(MPOL_DEFAULT)
			ret = shim_set_mempolicy(MPOL_DEFAULT | mode, NULL, max_nodes);
			break;
#endif
		case 1:
#if defined(MPOL_BIND)
			ret = shim_set_mempolicy(MPOL_BIND | mode, node_mask, max_nodes);
			break;
#endif
		case 2:
#if defined(MPOL_INTERLEAVE)
			ret = shim_set_mempolicy(MPOL_INTERLEAVE | mode, node_mask, max_nodes);
			break;
#endif
		case 3:
#if defined(MPOL_PREFERRED)
			ret = shim_set_mempolicy(MPOL_PREFERRED | mode, node_mask, max_nodes);
			break;
#endif
		case 4:
#if defined(MPOL_LOCAL)
			ret = shim_set_mempolicy(MPOL_LOCAL | mode, node_mask, max_nodes);
			break;
#endif
		case 5:
#if defined(MPOL_PREFERRED_MANY)
			ret = shim_set_mempolicy(MPOL_PREFERRED_MANY | mode, node_mask, max_nodes);
			break;
#endif
		case 6:
			ret = shim_set_mempolicy(0, node_mask, max_nodes);
			break;
		case 7:
			ret = shim_set_mempolicy(mode, node_mask, max_nodes);
			break;
		case 8:
			/* Invalid mode */
			ret = shim_set_mempolicy(mode | MPOL_F_STATIC_NODES | MPOL_F_RELATIVE_NODES, node_mask, max_nodes);
			break;
#if defined(MPOL_F_NUMA_BALANCING) &&	\
    defined(MPOL_LOCAL)
		case 9:
			/* Invalid  MPOL_F_NUMA_BALANCING | MPOL_LOCAL */
			ret = shim_set_mempolicy(MPOL_F_NUMA_BALANCING | MPOL_LOCAL, node_mask, max_nodes);
			break;
#endif
		default:
			/* Intentionally invalid mode */
			ret = shim_set_mempolicy(~0, node_mask, max_nodes);
		}
		(void)ret;


		/*
		 *  Fetch CPU and node, we just waste some cycled
		 *  doing this for stress reasons only
		 */
		(void)shim_getcpu(&cpu, &curr_node, NULL);

		/* Initialised cache to be safe */
		(void)memset(&cache, 0, sizeof(cache));

		/*
		 * tcache argument is unused in getcpu currently.
		 * Exercise getcpu syscall with non-null tcache
		 * pointer to ensure kernel doesn't break even
		 * when this argument is used in future.
		 */
		(void)shim_getcpu(&cpu, &curr_node, &cache);

		/*
		 *  mbind the buffer, first try MPOL_STRICT which
		 *  may fail with EIO
		 */
		(void)memset(node_mask, 0, sizeof(node_mask));
		STRESS_SETBIT(node_mask, n->node_id);
		lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
			max_nodes, MPOL_MF_STRICT);
		if (lret < 0) {
			if ((errno != EIO) && (errno != ENOSYS)) {
				pr_fail("%s: mbind failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto err;
			}
		} else {
			(void)memset(buf, 0xaa, MMAP_SZ);
		}
		if (!keep_stressing_flag())
			break;

		/*
		 *  mbind the buffer, now try MPOL_DEFAULT
		 */
		(void)memset(node_mask, 0, sizeof(node_mask));
		STRESS_SETBIT(node_mask, n->node_id);
		lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
			max_nodes, MPOL_DEFAULT);
		if (lret < 0) {
			if ((errno != EIO) && (errno != ENOSYS)) {
				pr_fail("%s: mbind failed, errno=%d (%s)\n",
					args->name, errno, strerror(errno));
				goto err;
			}
		} else {
			(void)memset(buf, 0x5c, MMAP_SZ);
		}
		if (!keep_stressing_flag())
			break;

		/* Exercise invalid start address */
		lret = shim_mbind((void *)(buf + 7), MMAP_SZ, MPOL_BIND, node_mask,
			max_nodes, MPOL_MF_STRICT);
		(void)lret;

		/* Exercise wrap around */
		lret = shim_mbind((void *)(~(uintptr_t)0 & ~(page_size - 1)), page_size * 2,
			MPOL_BIND, node_mask, max_nodes, MPOL_MF_STRICT);
		(void)lret;

		/* Exercise invalid length */
		lret = shim_mbind((void *)buf, ~0UL, MPOL_BIND, node_mask,
			max_nodes, MPOL_MF_STRICT);
		(void)lret;

		/* Exercise zero length, allowed, but is a no-op */
		lret = shim_mbind((void *)buf, 0, MPOL_BIND, node_mask,
			max_nodes, MPOL_MF_STRICT);
		(void)lret;

		/* Exercise invalid max_nodes */
		lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
			0, MPOL_MF_STRICT);
		(void)lret;
		lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
			0xffffffff, MPOL_MF_STRICT);
		(void)lret;

		/* Exercise invalid flags */
		lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
			max_nodes, ~0U);
		(void)lret;

		/* Check mbind syscall cannot succeed without capability */
		if (!cap_sys_nice) {
			lret = shim_mbind((void *)buf, MMAP_SZ, MPOL_BIND, node_mask,
				max_nodes, MPOL_MF_MOVE_ALL);
			if (lret >= 0) {
				pr_fail("%s: mbind without capability CAP_SYS_NICE unexpectedly succeeded, "
						"errno=%d (%s)\n", args->name, errno, strerror(errno));
			}
		}

		/* Move to next node */
		n = n->next;

		/*
		 *  Migrate all this processes pages to the current new node
		 */
		(void)memset(old_node_mask, 0, sizeof(old_node_mask));
		max_node_id_count = max_nodes;
		for (i = 0; max_node_id_count >= lbits && i < lbits; i++) {
			old_node_mask[i] = ULONG_MAX;
			max_node_id_count -= lbits;
		}
		if (i < lbits)
			old_node_mask[i] = (1UL << max_node_id_count) - 1;
		(void)memset(node_mask, 0, sizeof(node_mask));
		STRESS_SETBIT(node_mask, n->node_id);

		/*
	 	 *  Ignore any failures, this is not strictly important
		 */
		lret = shim_migrate_pages(args->pid, max_nodes,
			old_node_mask, node_mask);
		(void)lret;

		/*
		 *  Exercise illegal pid
		 */
		lret = shim_migrate_pages(~0, max_nodes, old_node_mask,
			node_mask);
		(void)lret;

		/*
		 *  Exercise illegal max_nodes
		 */
		lret = shim_migrate_pages(args->pid, ~0UL, old_node_mask,
			node_mask);
		(void)lret;
		lret = shim_migrate_pages(args->pid, 0, old_node_mask,
			node_mask);
		(void)lret;

		if (!keep_stressing_flag())
			break;

		n_tmp = n;
		for (j = 0; j < 16; j++) {
			/*
			 *  Now move pages to lots of different numa nodes
			 */
			for (ptr = buf, i = 0; i < num_pages; i++, ptr += page_size, n_tmp = n_tmp->next) {
				pages[i] = ptr;
				dest_nodes[i] = (int)n_tmp->node_id;
			}
			(void)memset(status, 0, sizeof(status));
			lret = shim_move_pages(args->pid, num_pages, pages,
				dest_nodes, status, MPOL_MF_MOVE);
			if (lret < 0) {
				if (errno != ENOSYS) {
					pr_fail("%s: move_pages failed, errno=%d (%s)\n",
						args->name, errno, strerror(errno));
					goto err;
				}
			}
			(void)memset(buf, j, MMAP_SZ);
			if (!keep_stressing_flag())
				break;
		}

#if defined(MPOL_MF_MOVE_ALL)
		/* Exercise MPOL_MF_MOVE_ALL, this needs privilege, ignore failure */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(args->pid, num_pages, pages, dest_nodes, status, MPOL_MF_MOVE_ALL);
		(void)lret;
#endif

		/* Exercise invalid pid on move pages */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(~0, 1, pages, dest_nodes, status, MPOL_MF_MOVE);
		(void)lret;

		/* Exercise 0 nr_pages */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(args->pid, 0, pages, dest_nodes, status, MPOL_MF_MOVE);
		(void)lret;

		/* Exercise invalid move flags */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(args->pid, 1, pages, dest_nodes, status, ~0);
		(void)lret;

		/* Exercise zero flag, should succeed */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(args->pid, 1, pages, dest_nodes, status, 0);
		(void)lret;

		/* Exercise invalid address */
		(void)memset(status, 0, sizeof(status));
		pages[0] = (void *)(~(uintptr_t)0 & ~(args->page_size - 1));
		lret = shim_move_pages(args->pid, 1, pages, dest_nodes, status, MPOL_MF_MOVE);
		(void)lret;

		/* Exercise invalid dest_node */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		dest_nodes[0] = ~0;
		lret = shim_move_pages(args->pid, 1, pages, dest_nodes, status, MPOL_MF_MOVE);
		(void)lret;

		/* Exercise NULL nodes */
		(void)memset(status, 0, sizeof(status));
		pages[0] = buf;
		lret = shim_move_pages(args->pid, 1, pages, NULL, status, MPOL_MF_MOVE);
		(void)lret;

		inc_counter(args);
	} while (keep_stressing(args));

	rc = EXIT_SUCCESS;
err:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	(void)munmap(buf, MMAP_SZ);
numa_free:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
	stress_numa_free_nodes(n);

	return rc;
}

stressor_info_t stress_numa_info = {
	.stressor = stress_numa,
	.class = CLASS_CPU | CLASS_MEMORY | CLASS_OS,
	.verify = VERIFY_ALWAYS,
	.help = help
};
#else
stressor_info_t stress_numa_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_CPU | CLASS_MEMORY | CLASS_OS,
	.help = help
};
#endif
