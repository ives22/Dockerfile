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
#include "core-arch.h"
#include "core-put.h"
#include "core-target-clones.h"
#include "core-vecmath.h"

#define VERY_WIDE	(0)

static const stress_help_t help[] = {
	{ NULL,	"vecwide N",	 "start N workers performing vector math ops" },
	{ NULL,	"vecwide-ops N", "stop after N vector math bogo operations" },
	{ NULL,	NULL,		 NULL }
};

#if defined(HAVE_VECMATH)

/*
 *  8 bit vectors, named by int8wN where N = number of bits width
 */
#if VERY_WIDE
typedef int8_t stress_vint8w8192_t	__attribute__ ((vector_size(8192 / 8)));
typedef int8_t stress_vint8w4096_t	__attribute__ ((vector_size(4096 / 8)));
#endif
typedef int8_t stress_vint8w2048_t	__attribute__ ((vector_size(2048 / 8)));
typedef int8_t stress_vint8w1024_t	__attribute__ ((vector_size(1024 / 8)));
typedef int8_t stress_vint8w512_t	__attribute__ ((vector_size(512 / 8)));
typedef int8_t stress_vint8w256_t	__attribute__ ((vector_size(256 / 8)));
typedef int8_t stress_vint8w128_t	__attribute__ ((vector_size(128 / 8)));
typedef int8_t stress_vint8w64_t	__attribute__ ((vector_size(64 / 8)));
typedef int8_t stress_vint8w32_t	__attribute__ ((vector_size(32 / 8)));

#if VERY_WIDE
#define VEC_MAX_SZ	sizeof(stress_vint8w8192_t)
#else
#define VEC_MAX_SZ	sizeof(stress_vint8w2048_t)
#endif

typedef struct {
	uint8_t	a[VEC_MAX_SZ];
	uint8_t	b[VEC_MAX_SZ];
	uint8_t	c[VEC_MAX_SZ];
	uint8_t	s[VEC_MAX_SZ];
	uint8_t v23[VEC_MAX_SZ];
	uint8_t v3[VEC_MAX_SZ];
	uint8_t	res[VEC_MAX_SZ];
} vec_args_t;

typedef void (*stress_vecwide_func_t)(vec_args_t *vec_args);

typedef struct {
	stress_vecwide_func_t	vecwide_func;
	size_t byte_size;
	double duration;
} stress_vecwide_funcs_t;

#define STRESS_VECWIDE(name, type)				\
static void TARGET_CLONES OPTIMIZE3 name (vec_args_t *vec_args) \
{								\
	type a, b, c, s, v23, v3, res;				\
	register int i;						\
								\
	(void)memcpy(&a, vec_args->a, sizeof(a));		\
	(void)memcpy(&b, vec_args->b, sizeof(b));		\
	(void)memcpy(&c, vec_args->c, sizeof(c));		\
	(void)memcpy(&s, vec_args->s, sizeof(s));		\
	(void)memcpy(&v23, vec_args->v23, sizeof(s));		\
	(void)memcpy(&v3, vec_args->v23, sizeof(s));		\
								\
	for (i = 2048; i; i--) {				\
		a += b;						\
		b -= c;						\
		c += v3;					\
		s ^= b;						\
		a += v23;					\
		b *= v3;					\
		a *= s;						\
	}							\
								\
	res = a + b + c;					\
								\
	for (i = 0; i < (int)sizeof(res); i++) {		\
		stress_uint8_put(res[i]);			\
	}							\
}

#if VERY_WIDE
STRESS_VECWIDE(stress_vecwide_8192, stress_vint8w8192_t)
STRESS_VECWIDE(stress_vecwide_4096, stress_vint8w4096_t)
#endif
STRESS_VECWIDE(stress_vecwide_2048, stress_vint8w2048_t)
STRESS_VECWIDE(stress_vecwide_1024, stress_vint8w1024_t)
STRESS_VECWIDE(stress_vecwide_512, stress_vint8w512_t)
STRESS_VECWIDE(stress_vecwide_256, stress_vint8w256_t)
STRESS_VECWIDE(stress_vecwide_128, stress_vint8w128_t)
STRESS_VECWIDE(stress_vecwide_64, stress_vint8w64_t)
STRESS_VECWIDE(stress_vecwide_32, stress_vint8w32_t)

static stress_vecwide_funcs_t stress_vecwide_funcs[] = {
#if VERY_WIDE
	{ stress_vecwide_8192, sizeof(stress_vint8w8192_t), 0.0 },
	{ stress_vecwide_4096, sizeof(stress_vint8w4096_t), 0.0 },
#endif
	{ stress_vecwide_2048, sizeof(stress_vint8w2048_t), 0.0 },
	{ stress_vecwide_1024, sizeof(stress_vint8w1024_t), 0.0 },
	{ stress_vecwide_512,  sizeof(stress_vint8w512_t),  0.0 },
	{ stress_vecwide_256,  sizeof(stress_vint8w256_t),  0.0 },
	{ stress_vecwide_128,  sizeof(stress_vint8w128_t),  0.0 },
	{ stress_vecwide_64,   sizeof(stress_vint8w64_t),   0.0 },
	{ stress_vecwide_32,   sizeof(stress_vint8w32_t),   0.0 },
};

static int stress_vecwide(const stress_args_t *args)
{
	static vec_args_t *vec_args;
	size_t i;
	double total_duration = 0.0;
	size_t total_bytes = 0;
	const size_t vec_args_size = (sizeof(*vec_args) + args->page_size - 1) & ~(args->page_size - 1);

	vec_args = (vec_args_t *)mmap(NULL, vec_args_size, PROT_READ | PROT_WRITE,
					MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (vec_args == MAP_FAILED) {
		pr_inf("%s: skipping stressor, failed to allocate vectors, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}

	for (i = 0; i < SIZEOF_ARRAY(stress_vecwide_funcs); i++)
		stress_vecwide_funcs[i].duration = 0.0;

	for (i = 0; i < SIZEOF_ARRAY(vec_args->a); i++) {
		vec_args->a[i] = (int8_t)i;
		vec_args->b[i] = stress_mwc8();
		vec_args->c[i] = stress_mwc8();
		vec_args->s[i] = stress_mwc8();
	}

	(void)memset(&vec_args->v23, 23, sizeof(vec_args->v23));
	(void)memset(&vec_args->v3, 3, sizeof(vec_args->v3));

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		for (i = 0; i < SIZEOF_ARRAY(stress_vecwide_funcs); i++) {
			double t1, t2, dt;

			t1 = stress_time_now();
			stress_vecwide_funcs[i].vecwide_func(vec_args);
			t2 = stress_time_now();
			dt = (t2 - t1);

			total_duration += dt;
			stress_vecwide_funcs[i].duration += dt;

			inc_counter(args);
		}
	} while (keep_stressing(args));

	for (i = 0; i < SIZEOF_ARRAY(stress_vecwide_funcs); i++) {
		total_bytes += stress_vecwide_funcs[i].byte_size;
	}

	if (args->instance == 0) {
		bool lock = false;

		pr_lock(&lock);
		pr_dbg_lock(&lock, "Bytes %% dur  %% exp (x win) (> 1.0 is better than expected)\n");
		for (i = 0; i < SIZEOF_ARRAY(stress_vecwide_funcs); i++) {
			double dur_pc, exp_pc, win;

			dur_pc = stress_vecwide_funcs[i].duration / total_duration * 100.0;
			exp_pc = (double)stress_vecwide_funcs[i].byte_size / (double)total_bytes * 100.0;
			win    = exp_pc / dur_pc;

			pr_dbg_lock(&lock, "%5zd %5.2f%% %5.2f%% %5.2f\n",
				stress_vecwide_funcs[i].byte_size,
				dur_pc, exp_pc, win);
		}
		pr_unlock(&lock);
	}

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)vec_args, vec_args_size);

	return EXIT_SUCCESS;
}

stressor_info_t stress_vecwide_info = {
	.stressor = stress_vecwide,
	.class = CLASS_CPU | CLASS_CPU_CACHE,
	.help = help
};
#else
stressor_info_t stress_vecwide_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_CPU | CLASS_CPU_CACHE,
	.help = help
};
#endif
