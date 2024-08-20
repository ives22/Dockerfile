/*
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

static const stress_help_t help[] = {
	{ NULL,	"pageswap N",		"start N workers that swap pages out and in" },
	{ NULL,	"pageswap-ops N",	"stop after N page swap bogo operations" },
	{ NULL,	NULL,			NULL }
};

typedef struct page_info {
	struct page_info *self;		/* address of page info for verification */
	struct page_info *next;		/* next page in list */
	size_t size;			/* size of page */
} page_info_t;

static int stress_pageswap_supported(const char *name)
{
#if defined(MADV_PAGEOUT)
	(void)name;

	return 0;
#else
	pr_inf_skip("%s: stressor will be skipped, madvise MADV_PAGEOUT is not "
		"implemented on this system\n", name);
		return -1;
#endif
}

#if defined(MADV_PAGEOUT)
static void stress_pageswap_unmap(const stress_args_t *args, page_info_t **head)
{
	page_info_t *pi = *head;
	const bool verify = !!(g_opt_flags & OPT_FLAGS_VERIFY);

	while (pi) {
		page_info_t *next = pi->next;

		(void)madvise(pi, pi->size, MADV_PAGEOUT);
		if (verify && (pi->self != pi)) {
			pr_fail("%s: page at %p does not contain expected data\n",
				args->name, pi);
		}
		(void)munmap(pi, pi->size);
		pi = next;
	}
	*head = NULL;
}

/*
 *  stress_pageswap_child()
 *	oomable process that maps pages and force pages them out with
 *	madvise. Once 65536 pages (or we run out of mappings) occurs
 *	the pages are unmapped - the walking of the list pages them back
 *	in before they are unmapped.
 */
static int stress_pageswap_child(const stress_args_t *args, void *context)
{
	const size_t page_size = STRESS_MAXIMUM(args->page_size, sizeof(page_info_t));
	size_t max = 0;

	(void)context;

	page_info_t *head = NULL;

	do {
		page_info_t *pi;

		pi = (page_info_t *)mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, -1, 0);
		if (pi == MAP_FAILED) {
			stress_pageswap_unmap(args, &head);
			max = 0;
		} else {
			page_info_t *oldhead = head;

			pi->size = page_size;
			pi->next = head;
			pi->self = pi;
			head = pi;

			(void)madvise(pi, pi->size, MADV_PAGEOUT);
			if (oldhead)
				(void)madvise(oldhead, oldhead->size, MADV_PAGEOUT);
			if (max++ > 65536) {
				stress_pageswap_unmap(args, &head);
				max = 0;
			}
			inc_counter(args);
		}
	} while (keep_stressing(args));

	stress_pageswap_unmap(args, &head);

	return EXIT_SUCCESS;
}

/*
 *  stress_pageswap()
 *	stress page swap in and swap out
 */
static int stress_pageswap(const stress_args_t *args)
{
	int rc;

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
	rc = stress_oomable_child(args, NULL, stress_pageswap_child, STRESS_OOMABLE_DROP_CAP);
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	return rc;
}

stressor_info_t stress_pageswap_info = {
	.stressor = stress_pageswap,
	.supported = stress_pageswap_supported,
	.class = CLASS_OS | CLASS_VM,
	.verify = VERIFY_OPTIONAL,
	.help = help
};

#else

stressor_info_t stress_pageswap_info = {
	.stressor = stress_not_implemented,
	.supported = stress_pageswap_supported,
	.class = CLASS_OS | CLASS_VM,
	.help = help
};

#endif
