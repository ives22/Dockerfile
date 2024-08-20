/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 * Copyright (C) 2021 Colin Ian King
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

static const stress_help_t help[] = {
	{ NULL,	"lockbus N",	 "start N workers locking a memory increment" },
	{ NULL,	"lockbus-ops N", "stop after N lockbus bogo operations" },
	{ NULL, NULL,		 NULL }
};

#if (((defined(__GNUC__) || defined(__clang__)) && 	\
       defined(STRESS_ARCH_X86)) ||			\
     (defined(__GNUC__) && 				\
      defined(HAVE_ATOMIC_ADD_FETCH) &&			\
      defined(__ATOMIC_SEQ_CST) &&			\
      NEED_GNUC(4,7,0) && 				\
      defined(STRESS_ARCH_ARM)))

#if defined(HAVE_ATOMIC_ADD_FETCH)
#define MEM_LOCK(ptr, inc)				\
do {							\
	 __atomic_add_fetch(ptr, inc, __ATOMIC_SEQ_CST);\
} while (0)
#else
#define MEM_LOCK(ptr, inc)				\
do {							\
	asm volatile("lock addl %1,%0" :		\
		     "+m" (*ptr) :			\
		     "ir" (inc));			\
} while (0)
#endif

#define BUFFER_SIZE	(1024 * 1024 * 16)
#define CHUNK_SIZE	(64 * 4)

#if defined(HAVE_SYNC_BOOL_COMPARE_AND_SWAP)
/* basically locked cmpxchg */
#define SYNC_BOOL_COMPARE_AND_SWAP(ptr, old_val, new_val) 	\
do {								\
	__sync_bool_compare_and_swap(ptr, old_val, new_val);	\
} while (0)
#else
/* no-op */
#define SYNC_BOOL_COMPARE_AND_SWAP(ptr, old_val, new_val)	\
do {								\
	(void)ptr;						\
	(void)old_val;						\
	(void)new_val;						\
}
#endif

#define MEM_LOCK_AND_INC(ptr, inc)		\
do {						\
	MEM_LOCK(ptr, inc);			\
	ptr++;					\
} while (0)

#define MEM_LOCK_AND_INCx8(ptr, inc)		\
do {						\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
	MEM_LOCK_AND_INC(ptr, inc);		\
} while (0)

#define MEM_LOCKx8(ptr)				\
do {						\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
	MEM_LOCK(ptr, 0);			\
} while (0)

#if defined(STRESS_ARCH_X86)
static sigjmp_buf jmp_env;
static bool do_splitlock;

static void NORETURN MLOCKED_TEXT stress_sigbus_handler(int signum)
{
	(void)signum;

	do_splitlock = false;

	siglongjmp(jmp_env, 1);
}
#endif

/*
 *  stress_lockbus()
 *      stress memory with lock and increment
 */
static int stress_lockbus(const stress_args_t *args)
{
	uint32_t *buffer;
	int flags = MAP_ANONYMOUS | MAP_SHARED;
#if defined(STRESS_ARCH_X86)
	uint32_t *splitlock_ptr1, *splitlock_ptr2;

	if (stress_sighandler(args->name, SIGBUS, stress_sigbus_handler, NULL) < 0)
		return EXIT_FAILURE;
#endif

#if defined(MAP_POPULATE)
	flags |= MAP_POPULATE;
#endif
	buffer = (uint32_t*)mmap(NULL, BUFFER_SIZE, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (buffer == MAP_FAILED) {
		int rc = exit_status(errno);

		pr_err("%s: mmap failed\n", args->name);
		return rc;
	}

#if defined(STRESS_ARCH_X86)
	/* Split lock on a page boundary */
	splitlock_ptr1 = (uint32_t *)(((uint8_t *)buffer) + args->page_size - (sizeof(*splitlock_ptr1) >> 1));
	/* Split lock on a cache boundary */
	splitlock_ptr2 = (uint32_t *)(((uint8_t *)buffer) + 64 - (sizeof(*splitlock_ptr2) >> 1));
	do_splitlock = true;
	if (sigsetjmp(jmp_env, 1) && !keep_stressing(args))
		goto done;
#endif
	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	do {
		uint32_t *ptr0 = buffer + ((stress_mwc32() % (BUFFER_SIZE - CHUNK_SIZE)) >> 2);
#if defined(STRESS_ARCH_X86)
		uint32_t *ptr1 = do_splitlock ? splitlock_ptr1 : ptr0;
		uint32_t *ptr2 = do_splitlock ? splitlock_ptr2 : ptr0;
#else
		uint32_t *ptr1 = ptr0;
		uint32_t *ptr2 = ptr0;
#endif
		const uint32_t inc = 1;

		MEM_LOCK_AND_INCx8(ptr0, inc);
		MEM_LOCKx8(ptr1);
		MEM_LOCKx8(ptr2);
		MEM_LOCK_AND_INCx8(ptr0, inc);
		MEM_LOCKx8(ptr1);
		MEM_LOCKx8(ptr2);
		MEM_LOCK_AND_INCx8(ptr0, inc);
		MEM_LOCKx8(ptr1);
		MEM_LOCKx8(ptr2);
		MEM_LOCK_AND_INCx8(ptr0, inc);
		MEM_LOCKx8(ptr1);
		MEM_LOCKx8(ptr2);

#if defined(HAVE_SYNC_BOOL_COMPARE_AND_SWAP)
		{
			register uint32_t val;
			register const uint32_t zero = 0;

			val = *ptr0;
			SYNC_BOOL_COMPARE_AND_SWAP(ptr0, val, zero);
			SYNC_BOOL_COMPARE_AND_SWAP(ptr0, zero, val);

			val = *ptr1;
			SYNC_BOOL_COMPARE_AND_SWAP(ptr1, val, zero);
			SYNC_BOOL_COMPARE_AND_SWAP(ptr1, zero, val);

			val = *ptr2;
			SYNC_BOOL_COMPARE_AND_SWAP(ptr2, val, zero);
			SYNC_BOOL_COMPARE_AND_SWAP(ptr2, zero, val);
		}
#endif

		inc_counter(args);
	} while (keep_stressing(args));

#if defined(STRESS_ARCH_X86)
done:
#endif
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)buffer, BUFFER_SIZE);

	return EXIT_SUCCESS;
}

stressor_info_t stress_lockbus_info = {
	.stressor = stress_lockbus,
	.class = CLASS_CPU_CACHE | CLASS_MEMORY,
	.help = help
};
#else
stressor_info_t stress_lockbus_info = {
	.stressor = stress_not_implemented,
	.class = CLASS_CPU_CACHE | CLASS_MEMORY,
	.help = help
};
#endif
