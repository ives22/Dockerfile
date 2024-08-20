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
#include "core-put.h"

#define STRESS_RANDLIST_DEFAULT_ITEMS	(100000)
#define STRESS_RANDLIST_MAX_SIZE	(8192)
#define STRESS_RANDLIST_DEFAULT_SIZE	(64)

#define STRESS_RANDLIST_ALLOC_NONE	(0)
#define STRESS_RANDLIST_ALLOC_HEAP	(1)
#define STRESS_RANDLIST_ALLOC_MMAP	(2)

static const stress_help_t help[] = {
	{ NULL,	"randlist N",		"start N workers that exercise random ordered list" },
	{ NULL,	"randlist-ops N",	"stop after N randlist bogo no-op operations" },
	{ NULL, "randlist-compact",	"reduce mmap and malloc overheads" },
	{ NULL, "randlist-items",	"number of items in the random ordered list" },
	{ NULL, "randlist-size",	"size of data in each item in the list" },
	{ NULL,	NULL,			NULL }
};

typedef struct stress_randlist_item {
	struct stress_randlist_item *next;
	uint8_t alloc_type:2;
	uint8_t data[];
} stress_randlist_item_t;

/*
 *  stress_set_randlist_compact()
 *      set randlist compact mode setting
 */
static int stress_set_randlist_compact(const char *opt)
{
	bool randlist_compact = true;

	(void)opt;
	return stress_set_setting("randlist-compact", TYPE_ID_BOOL, &randlist_compact);
}

/*
 *  stress_set_randlist_items()
 *      set randlist number of items from given option string
 */
static int stress_set_randlist_items(const char *opt)
{
	uint32_t items;
	size_t randlist_items;

	items = stress_get_uint32(opt);
	stress_check_range("randlist-size", items, 1, 0xffffffff);

	randlist_items = (size_t)items;
	return stress_set_setting("randlist-items", TYPE_ID_SIZE_T, &randlist_items);
}

/*
 *  stress_set_randlist_size()
 *      set randlist size from given option string
 */
static int stress_set_randlist_size(const char *opt)
{
	uint64_t size;
	size_t randlist_size;

	size = stress_get_uint64_byte(opt);
	stress_check_range("randlist-size", size, 1, STRESS_RANDLIST_MAX_SIZE);

	randlist_size = (size_t)size;
	return stress_set_setting("randlist-size", TYPE_ID_SIZE_T, &randlist_size);
}

static void stress_randlist_free_item(stress_randlist_item_t **item, const size_t randlist_size)
{
	if (!*item)
		return;

	if ((*item)->alloc_type == STRESS_RANDLIST_ALLOC_HEAP)
		free(*item);
	else if ((*item)->alloc_type == STRESS_RANDLIST_ALLOC_MMAP) {
		const size_t size = sizeof(**item) + randlist_size;

		(void)munmap((void *)*item, size);
	}

	*item = NULL;
}

static void stress_randlist_free_ptrs(
	stress_randlist_item_t *compact_ptr,
	stress_randlist_item_t *ptrs[],
	const size_t n,
	const size_t randlist_size)
{
	if (compact_ptr) {
		free(compact_ptr);
	} else {
		size_t i;

		for (i = 0; i < n; i++) {
			stress_randlist_free_item(&ptrs[i], randlist_size);
		}
	}
	free(ptrs);
}

static void stress_randlist_enomem(const stress_args_t *args)
{
	pr_inf("%s: cannot allocate the list, skipping stressor\n",
		args->name);
}

/*
 *  stress_randlist()
 *	stress that does lots of not a lot
 */
static int stress_randlist(const stress_args_t *args)
{
	register size_t i;
	stress_randlist_item_t **ptrs;
	stress_randlist_item_t *ptr, *head, *next;
	stress_randlist_item_t *compact_ptr = NULL;
	bool do_mmap = false;
	bool randlist_compact = false;
	size_t randlist_items = STRESS_RANDLIST_DEFAULT_ITEMS;
	size_t randlist_size = STRESS_RANDLIST_DEFAULT_SIZE;
	size_t heap_allocs = 0;
	size_t mmap_allocs = 0;

	(void)stress_get_setting("randlist-compact", &randlist_compact);
	(void)stress_get_setting("randlist-items", &randlist_items);
	(void)stress_get_setting("randlist-size", &randlist_size);

	if (randlist_size >= args->page_size)
		do_mmap = true;

	ptrs = calloc(randlist_items, sizeof(stress_randlist_item_t *));
	if (!ptrs) {
		pr_inf("%s: cannot allocate %zd temporary pointers, skipping stressor\n",
			args->name, randlist_items);
		return EXIT_NO_RESOURCE;
	}

	if (randlist_compact) {
		const size_t size = sizeof(*ptr) + randlist_size;

		compact_ptr = calloc(randlist_items, size);
		if (!compact_ptr) {
			stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
			free(ptrs);
			stress_randlist_enomem(args);
			return EXIT_NO_RESOURCE;
		}

		for (ptr = compact_ptr, i = 0; i < randlist_items; i++) {
			if (!keep_stressing_flag()) {
				stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
				stress_randlist_free_ptrs(compact_ptr, ptrs, i, randlist_size);
				stress_randlist_enomem(args);
				return EXIT_SUCCESS;
			}
			ptrs[i] = ptr;
			ptr = (stress_randlist_item_t *)((char *)ptr + size);
		}
		heap_allocs++;
	} else {
		for (i = 0; i < randlist_items; i++) {
			const size_t size = sizeof(*ptr) + randlist_size;

			if (!keep_stressing_flag()) {
				stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
				stress_randlist_free_ptrs(compact_ptr, ptrs, i, randlist_size);
				return EXIT_SUCCESS;
			}
retry:
			if (do_mmap && (stress_mwc8() < 16)) {
				ptr = (stress_randlist_item_t *)mmap(NULL, size, PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_ANONYMOUS, -1, 0);
				if (ptr == MAP_FAILED) {
					do_mmap = false;
					goto retry;
				}
				ptr->alloc_type = STRESS_RANDLIST_ALLOC_MMAP;
				mmap_allocs++;
			} else {
				ptr = (stress_randlist_item_t *)calloc(1, size);
				if (!ptr) {
					stress_randlist_free_ptrs(compact_ptr, ptrs, i, randlist_size);
					stress_randlist_enomem(args);
					return EXIT_NO_RESOURCE;
				}
				ptr->alloc_type = STRESS_RANDLIST_ALLOC_HEAP;
				heap_allocs++;
			}
			ptrs[i] = ptr;
		}
	}

	/*
	 *  Shuffle into random item order
	 */
	for (i = 0; i < randlist_items; i++) {
		size_t n = random() % randlist_items;

		ptr = ptrs[i];
		ptrs[i] = ptrs[n];
		ptrs[n] = ptr;

		if (!keep_stressing_flag()) {
			stress_set_proc_state(args->name, STRESS_STATE_DEINIT);
			stress_randlist_free_ptrs(compact_ptr, ptrs, i, randlist_size);
			return EXIT_SUCCESS;
		}
	}

	/*
	 *  Link all items together based on the random ordering
	 */
	for (i = 0; i < randlist_items; i++) {
		ptr = ptrs[i];
		ptr->next = (i == randlist_items - 1) ? NULL : ptrs[i+1];
	}

	head = ptrs[0];
	free(ptrs);

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		for (ptr = head; ptr; ptr = ptr->next) {
			(void)memset(ptr->data, stress_mwc8(), randlist_size);
			if (!keep_stressing_flag())
				break;
		}

		for (ptr = head; ptr; ptr = ptr->next) {
			uint32_t sum = 0;
			for (i = 0; i < randlist_size; i++) {
				sum += ptr->data[i];
			}
			stress_uint32_put(sum);
			if (!keep_stressing_flag())
				break;
		}
		inc_counter(args);
	} while (keep_stressing(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	pr_dbg("%s: heap allocations: %zd, mmap allocations: %zd\n", args->name, heap_allocs, mmap_allocs);

	if (compact_ptr) {
		free(compact_ptr);
	} else {
		for (ptr = head; ptr; ) {
			next = ptr->next;
			stress_randlist_free_item(&ptr, randlist_size);
			ptr = next;
		}
	}

	return EXIT_SUCCESS;
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_randlist_compact,	stress_set_randlist_compact },
	{ OPT_randlist_items,	stress_set_randlist_items },
	{ OPT_randlist_size,	stress_set_randlist_size },
	{ 0,                    NULL }
};

stressor_info_t stress_randlist_info = {
	.stressor = stress_randlist,
	.class = CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
