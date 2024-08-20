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

#define DEBUG_TAG_INFO		(0)

static const stress_help_t help[] = {
	{ NULL,	"l1cache N",	 	"start N CPU level 1 cache thrashing workers" },
	{ NULL, "l1cache-line-size N",	"specify level 1 cache line size" },
	{ NULL, "l1cache-sets N",	"specify level 1 cache sets" },
	{ NULL, "l1cache-size N",	"specify level 1 cache size" },
	{ NULL,	"l1cache-ways N",	"only fill specified number of cache ways" },
	{ NULL,	NULL,			NULL }
};

static int stress_l1cache_set(const char *opt, const char *name, size_t max)
{
	uint32_t val32;
	uint64_t val64;

        val64 = stress_get_uint64_byte(opt);
        stress_check_range_bytes(name, val64, 1, max);
	val32 = (uint32_t)val64;
        return stress_set_setting(name, TYPE_ID_UINT32, &val32);
}

static int stress_l1cache_set_ways(const char *opt)
{
	return stress_l1cache_set(opt, "l1cache-ways", 65536);
}

static int stress_l1cache_set_size(const char *opt)
{
	return stress_l1cache_set(opt, "l1cache-size", INT_MAX);
}

static int stress_l1cache_set_line_size(const char *opt)
{
	return stress_l1cache_set(opt, "l1cache-line-size", INT_MAX);
}

static int stress_l1cache_set_sets(const char *opt)
{
	return stress_l1cache_set(opt, "l1cache-sets", 65536);
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_l1cache_ways,	 stress_l1cache_set_ways },
	{ OPT_l1cache_size,	 stress_l1cache_set_size },
	{ OPT_l1cache_line_size, stress_l1cache_set_line_size },
	{ OPT_l1cache_sets,	 stress_l1cache_set_sets },
	{ 0,			NULL }
};

#if DEBUG_TAG_INFO
/*
 *  stress_l1cache_ln2()
 *	calculate log base 2
 */
static uint32_t stress_l1cache_ln2(uint32_t val)
{
	int ln2 = 0;

	while (val >>= 1)
		++ln2;

	return ln2;
}
#endif

/*
 *  stress_l1cache_info_check()
 *	sanity check cache information
 */
static int stress_l1cache_info_check(
	const stress_args_t *args,
	uint32_t ways,
	uint32_t size,
	uint32_t sets,
	uint32_t line_size)
{
	uint64_t sz = size ? size :
		(uint64_t)ways * (uint64_t)sets * (uint64_t)line_size;

	if (args->instance == 0) {
		char szstr[64];

		stress_uint64_to_str(szstr, sizeof(szstr), sz);

		pr_inf("%s: l1cache: size: %s, sets: %" PRIu32
			", ways: %" PRIu32 ", line size: %" PRIu32 "\n",
			args->name, szstr, sets, ways, line_size);
#if DEBUG_TAG_INFO
		pr_inf("%s: l1cache: offset size: %" PRIu32 ", index size: %" PRIu32 "\n",
			args->name, stress_l1cache_ln2(line_size),
			stress_l1cache_ln2(sets));
#endif
	}

	if (size == 0) {
		pr_inf("%s: invalid cache size of 0\n", args->name);
		return EXIT_FAILURE;
	}
	if (sets == 0) {
		pr_inf("%s: invalid 0 number of sets\n", args->name);
		return EXIT_FAILURE;
	}
	if (ways == 0) {
		pr_inf("%s: invalid 0 number of ways\n", args->name);
		return EXIT_FAILURE;
	}
	if (line_size == 0) {
		pr_inf("%s: invalid cache line size of 0\n", args->name);
		return EXIT_FAILURE;
	}
	if (sz != size) {
		pr_inf("%s: cache size %" PRIu32 " not equal to ways %" PRIu32
			" * sets %" PRIu32 " * line size %" PRIu32 "\n",
		args->name, size, ways, sets, line_size);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int stress_l1cache_info_ok(
	const stress_args_t *args,
	uint32_t *ways,
	uint32_t *size,
	uint32_t *sets,
	uint32_t *line_size)
{
	int ret;
#if defined(__linux__)
	stress_cpus_t *cpu_caches;
	stress_cpu_cache_t *cache = NULL;
#endif

	if ((*ways > 0) && (*size > 0) && (*sets > 0) && (*line_size > 0)) {
		return stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	}
	if ((*ways > 0) && (*size > 0) && (*sets > 0) && (*line_size == 0)) {
		*line_size = *size / (*ways * *sets);
		return stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	}
	if ((*ways > 0) && (*size > 0) && (*sets == 0) && (*line_size > 0)) {
		*sets = *size / (*ways * *line_size);
		return stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	}
	if ((*ways > 0) && (*size == 0) && (*sets > 0) && (*line_size > 0)) {
		*size = *sets * *ways * *line_size;
		return stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	}
	if ((*ways == 0) && (*size > 0) && (*sets > 0) && (*line_size > 0)) {
		*ways = *size / (*sets * *line_size);
		return stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	}

#if defined(__linux__)
	/*
	 *  User didn't provide cache info, try and figure it
	 *  out
	 */
	cpu_caches = stress_get_all_cpu_cache_details();
	if (!cpu_caches)
		goto bad_cache;

	if (stress_get_max_cache_level(cpu_caches) < 1)
		goto bad_cache_free;

	cache = stress_get_cpu_cache(cpu_caches, 1);
	if (!cache) {
		goto bad_cache_free;
	}

	if (*size == 0)
		*size = (uint32_t)cache->size;
	if (*line_size == 0)
		*line_size = cache->line_size;
	if (*ways == 0)
		*ways = cache->ways;
	if ((*sets == 0) && (*ways > 0) && (*line_size > 0))
		*sets = *size / (*ways * *line_size);

	stress_free_cpu_caches(cpu_caches);
#endif
	if ((*size == 0) && (*line_size == 0) && (*ways == 0) && (*sets == 0))
		goto bad_cache;
	ret = stress_l1cache_info_check(args, *ways, *size, *sets, *line_size);
	if (ret != EXIT_SUCCESS)
		goto bad_cache;
	return ret;

#if defined(__linux__)
bad_cache_free:
	stress_free_cpu_caches(cpu_caches);
#endif
bad_cache:
	pr_inf_skip("%s: skipping stressor, cannot determine "
		"cache level 1 information from kernel\n",
		args->name);

	return EXIT_NO_RESOURCE;
}

static int stress_l1cache(const stress_args_t *args)
{
	int ret;
	uint32_t l1cache_ways = 0;
	uint32_t l1cache_size = 0;
	uint32_t l1cache_sets = 0;
	uint32_t l1cache_line_size = 0;
	uint32_t l1cache_set_size;
	uint32_t set;
	uint8_t *cache, *cache_aligned;
	uintptr_t addr;
	uint32_t padding;

	(void)stress_get_setting("l1cache-ways", &l1cache_ways);
	(void)stress_get_setting("l1cache-size", &l1cache_size);
	(void)stress_get_setting("l1cache-sets", &l1cache_sets);
	(void)stress_get_setting("l1cache-line-size", &l1cache_line_size);

	ret = stress_l1cache_info_ok(args, &l1cache_ways, &l1cache_size,
				     &l1cache_sets, &l1cache_line_size);
	if (ret != EXIT_SUCCESS)
		return ret;

	cache = (uint8_t *)mmap(NULL, l1cache_size << 2,
				PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (cache == MAP_FAILED) {
		pr_inf_skip("%s: cannot mmap cache test buffer, skipping stressor, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}
	/* Get cache aligned buffer */
	l1cache_set_size = l1cache_ways * l1cache_line_size;
	if (l1cache_set_size == 0) {
		pr_inf("%s: invalid level 1 cache set size is zero\n", args->name);
		return EXIT_NO_RESOURCE;
	}
	addr = l1cache_size + (uintptr_t)cache;
	padding = (l1cache_set_size - (addr % l1cache_set_size)) % l1cache_set_size;
	cache_aligned = (uint8_t *)(addr + padding);

	if ((cache_aligned < cache) || (cache_aligned > cache + (l1cache_size << 1))) {
		pr_inf("%s: aligned cache address is out of range\n", args->name);
		(void)munmap((void *)cache, l1cache_size << 2);
		return EXIT_NO_RESOURCE;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	set = 0;
	do {
		const uint32_t set_offset = set * l1cache_set_size;
		register uint8_t *cache_start, *cache_end;
		register int i;

		cache_start = cache_aligned + set_offset;
		cache_end = cache_start + (l1cache_size << 1);

		/*
		 * cycle around 2 x cache size to force evictions
		 * in cache set steps to hit every cache line in
		 * the set, reads then writes
		 */
		if (UNLIKELY(g_opt_flags & OPT_FLAGS_VERIFY)) {
			/* Verify slow path */
			for (i = 0; i < 1000000; i++) {
				register volatile uint8_t *ptr;

				for (ptr = cache_start; ptr < cache_end; ptr += l1cache_set_size)
					*(ptr);

				for (ptr = cache_start; ptr < cache_end; ptr += l1cache_set_size)
					*(ptr) = (uint8_t)set;

				if (UNLIKELY(g_opt_flags & OPT_FLAGS_VERIFY)) {
					for (ptr = cache_start; ptr < cache_end; ptr += l1cache_set_size)
						if (*ptr != (uint8_t)set)
							pr_fail("%s: cache value mismatch at offset %zd\n",
								args->name, (size_t)(ptr - cache_start));
				}

				set++;
				if (set >= l1cache_sets)
					set = 0;
			}
		} else {
			/* non-verify fast path */
			for (i = 0; i < 1000000; i++) {
				register volatile uint8_t *ptr;

				for (ptr = cache_start; ptr < cache_end; ptr += l1cache_set_size)
					*(ptr);

				for (ptr = cache_start; ptr < cache_end; ptr += l1cache_set_size)
					*(ptr) = (uint8_t)set;

				set++;
				if (set >= l1cache_sets)
					set = 0;
			}
		}
		add_counter(args, l1cache_sets);
	} while (keep_stressing(args));

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)cache, l1cache_size << 2);

	return EXIT_SUCCESS;
}

stressor_info_t stress_l1cache_info = {
	.stressor = stress_l1cache,
	.class = CLASS_CPU_CACHE,
	.opt_set_funcs = opt_set_funcs,
	.verify = VERIFY_OPTIONAL,
	.help = help
};
