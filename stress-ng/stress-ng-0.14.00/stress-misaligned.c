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
#include "core-arch.h"
#include "core-target-clones.h"
#include "core-nt-store.h"

#define MISALIGN_LOOPS		(32768)

/* Disable atomic ops for SH4 as this breaks gcc on Debian sid */
#if defined(STRESS_ARCH_SH4)
#undef HAVE_ATOMIC
#endif

static const stress_help_t help[] = {
	{ NULL,	"misaligned N",	   	"start N workers performing misaligned read/writes" },
	{ NULL,	"misaligned-ops N",	"stop after N misaligned bogo operations" },
	{ NULL,	"misaligned-method M",	"use misaligned memory read/write method" },
	{ NULL,	NULL,			NULL }
};

static sigjmp_buf jmp_env;
static int handled_signum = -1;

typedef void (*stress_misaligned_func)(uint8_t *buffer, const size_t page_size);

typedef struct {
	const char *name;
	const stress_misaligned_func func;
	bool disabled;
	bool exercised;
} stress_misaligned_method_info_t;

static stress_misaligned_method_info_t *current_method;

#if defined(__SSE__) &&         \
    defined(STRESS_ARCH_X86) && \
    defined(HAVE_TARGET_CLONES)
/*
 *  keep_running_no_sse()
 *	non-inlined to workaround inlining NOSSE code build
 *	issues
 */
static bool NOINLINE keep_running_no_sse(void)
{
	return keep_stressing_flag();
}
#else
#define keep_running_no_sse()	keep_stressing_flag()
#endif

static void stress_misaligned_int16rd(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1  = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2  = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3  = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4  = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5  = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6  = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7  = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8  = (uint16_t *)(buffer + 15);
	volatile uint16_t *ptr9  = (uint16_t *)(buffer + page_size - 1);
	volatile uint16_t *ptr10 = (uint16_t *)(buffer + page_size - 3);
	volatile uint16_t *ptr11 = (uint16_t *)(buffer + page_size - 5);
	volatile uint16_t *ptr12 = (uint16_t *)(buffer + page_size - 7);
	volatile uint16_t *ptr13 = (uint16_t *)(buffer + page_size - 9);
	volatile uint16_t *ptr14 = (uint16_t *)(buffer + page_size - 11);
	volatile uint16_t *ptr15 = (uint16_t *)(buffer + page_size - 13);
	volatile uint16_t *ptr16 = (uint16_t *)(buffer + page_size - 15);
	volatile uint16_t *ptr17  = (uint16_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();
		(void)*ptr3;
		shim_mb();
		(void)*ptr4;
		shim_mb();
		(void)*ptr5;
		shim_mb();
		(void)*ptr6;
		shim_mb();
		(void)*ptr7;
		shim_mb();
		(void)*ptr8;
		shim_mb();

		(void)*ptr9;
		shim_mb();
		(void)*ptr10;
		shim_mb();
		(void)*ptr11;
		shim_mb();
		(void)*ptr12;
		shim_mb();
		(void)*ptr13;
		shim_mb();
		(void)*ptr14;
		shim_mb();
		(void)*ptr15;
		shim_mb();
		(void)*ptr16;
		shim_mb();

		(void)*ptr17;
		shim_mb();
	}
}

static void stress_misaligned_int16wr(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1  = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2  = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3  = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4  = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5  = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6  = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7  = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8  = (uint16_t *)(buffer + 15);
	volatile uint16_t *ptr9  = (uint16_t *)(buffer + page_size - 1);
	volatile uint16_t *ptr10 = (uint16_t *)(buffer + page_size - 3);
	volatile uint16_t *ptr11 = (uint16_t *)(buffer + page_size - 5);
	volatile uint16_t *ptr12 = (uint16_t *)(buffer + page_size - 7);
	volatile uint16_t *ptr13 = (uint16_t *)(buffer + page_size - 9);
	volatile uint16_t *ptr14 = (uint16_t *)(buffer + page_size - 11);
	volatile uint16_t *ptr15 = (uint16_t *)(buffer + page_size - 13);
	volatile uint16_t *ptr16 = (uint16_t *)(buffer + page_size - 15);
	volatile uint16_t *ptr17  = (uint16_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		*ptr1  = (uint16_t)i;
		shim_mb();
		*ptr2  = (uint16_t)i;
		shim_mb();
		*ptr3  = (uint16_t)i;
		shim_mb();
		*ptr4  = (uint16_t)i;
		shim_mb();
		*ptr5  = (uint16_t)i;
		shim_mb();
		*ptr6  = (uint16_t)i;
		shim_mb();
		*ptr7  = (uint16_t)i;
		shim_mb();
		*ptr8  = (uint16_t)i;
		shim_mb();

		*ptr9  = (uint16_t)i;
		shim_mb();
		*ptr10 = (uint16_t)i;
		shim_mb();
		*ptr11 = (uint16_t)i;
		shim_mb();
		*ptr12 = (uint16_t)i;
		shim_mb();
		*ptr13 = (uint16_t)i;
		shim_mb();
		*ptr14 = (uint16_t)i;
		shim_mb();
		*ptr15 = (uint16_t)i;
		shim_mb();
		*ptr16 = (uint16_t)i;
		shim_mb();

		*ptr17 = (uint16_t)i;
		shim_mb();
	}
}

static void stress_misaligned_int16inc(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1  = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2  = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3  = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4  = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5  = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6  = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7  = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8  = (uint16_t *)(buffer + 15);
	volatile uint16_t *ptr9  = (uint16_t *)(buffer + page_size - 1);
	volatile uint16_t *ptr10 = (uint16_t *)(buffer + page_size - 3);
	volatile uint16_t *ptr11 = (uint16_t *)(buffer + page_size - 5);
	volatile uint16_t *ptr12 = (uint16_t *)(buffer + page_size - 7);
	volatile uint16_t *ptr13 = (uint16_t *)(buffer + page_size - 9);
	volatile uint16_t *ptr14 = (uint16_t *)(buffer + page_size - 11);
	volatile uint16_t *ptr15 = (uint16_t *)(buffer + page_size - 13);
	volatile uint16_t *ptr16 = (uint16_t *)(buffer + page_size - 15);
	volatile uint16_t *ptr17  = (uint16_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();
		(*ptr3)++;
		shim_mb();
		(*ptr4)++;
		shim_mb();
		(*ptr5)++;
		shim_mb();
		(*ptr6)++;
		shim_mb();
		(*ptr7)++;
		shim_mb();
		(*ptr8)++;
		shim_mb();

		(*ptr9)++;
		shim_mb();
		(*ptr10)++;
		shim_mb();
		(*ptr11)++;
		shim_mb();
		(*ptr12)++;
		shim_mb();
		(*ptr13)++;
		shim_mb();
		(*ptr14)++;
		shim_mb();
		(*ptr15)++;
		shim_mb();
		(*ptr16)++;
		shim_mb();

		(*ptr17)++;
		shim_mb();
	}
}

#if defined(HAVE_ATOMIC_FETCH_ADD_2) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
static void stress_misaligned_int16atomic(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint16_t *ptr1  = (uint16_t *)(buffer + 1);
	volatile uint16_t *ptr2  = (uint16_t *)(buffer + 3);
	volatile uint16_t *ptr3  = (uint16_t *)(buffer + 5);
	volatile uint16_t *ptr4  = (uint16_t *)(buffer + 7);
	volatile uint16_t *ptr5  = (uint16_t *)(buffer + 9);
	volatile uint16_t *ptr6  = (uint16_t *)(buffer + 11);
	volatile uint16_t *ptr7  = (uint16_t *)(buffer + 13);
	volatile uint16_t *ptr8  = (uint16_t *)(buffer + 15);
	volatile uint16_t *ptr9 = (uint16_t *)(buffer + page_size - 1);
	volatile uint16_t *ptr10 = (uint16_t *)(buffer + page_size - 3);
	volatile uint16_t *ptr11 = (uint16_t *)(buffer + page_size - 5);
	volatile uint16_t *ptr12 = (uint16_t *)(buffer + page_size - 7);
	volatile uint16_t *ptr13 = (uint16_t *)(buffer + page_size - 9);
	volatile uint16_t *ptr14 = (uint16_t *)(buffer + page_size - 11);
	volatile uint16_t *ptr15 = (uint16_t *)(buffer + page_size - 13);
	volatile uint16_t *ptr16 = (uint16_t *)(buffer + page_size - 15);
	volatile uint16_t *ptr17  = (uint16_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		__atomic_fetch_add_2(ptr1, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr2, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr3, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr4, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr5, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr6, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr7, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr8, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_2(ptr9, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr10, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr11, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr12, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr13, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr14, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr15, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_2(ptr16, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_2(ptr17, 1, __ATOMIC_SEQ_CST);
		shim_mb();
	}
}
#endif

static void stress_misaligned_int32rd(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);
	volatile uint32_t *ptr5 = (uint32_t *)(buffer + page_size - 1);
	volatile uint32_t *ptr6 = (uint32_t *)(buffer + page_size - 5);
	volatile uint32_t *ptr7 = (uint32_t *)(buffer + page_size - 9);
	volatile uint32_t *ptr8 = (uint32_t *)(buffer + page_size - 13);
	volatile uint32_t *ptr9 = (uint32_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();
		(void)*ptr3;
		shim_mb();
		(void)*ptr4;
		shim_mb();

		(void)*ptr5;
		shim_mb();
		(void)*ptr6;
		shim_mb();
		(void)*ptr7;
		shim_mb();
		(void)*ptr8;
		shim_mb();

		(void)*ptr9;
		shim_mb();
	}
}

static void stress_misaligned_int32wr(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);
	volatile uint32_t *ptr5 = (uint32_t *)(buffer + page_size - 1);
	volatile uint32_t *ptr6 = (uint32_t *)(buffer + page_size - 5);
	volatile uint32_t *ptr7 = (uint32_t *)(buffer + page_size - 9);
	volatile uint32_t *ptr8 = (uint32_t *)(buffer + page_size - 13);
	volatile uint32_t *ptr9 = (uint32_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		*ptr1 = (uint32_t)i;
		shim_mb();
		*ptr2 = (uint32_t)i;
		shim_mb();
		*ptr3 = (uint32_t)i;
		shim_mb();
		*ptr4 = (uint32_t)i;
		shim_mb();

		*ptr5 = (uint32_t)i;
		shim_mb();
		*ptr6 = (uint32_t)i;
		shim_mb();
		*ptr7 = (uint32_t)i;
		shim_mb();
		*ptr8 = (uint32_t)i;
		shim_mb();

		*ptr9 = (uint32_t)i;
		shim_mb();
	}
}

#if defined(HAVE_NT_STORE32)
static void stress_misaligned_int32wrnt(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	uint32_t *ptr4 = (uint32_t *)(buffer + 13);
	uint32_t *ptr5 = (uint32_t *)(buffer + page_size - 1);
	uint32_t *ptr6 = (uint32_t *)(buffer + page_size - 5);
	uint32_t *ptr7 = (uint32_t *)(buffer + page_size - 9);
	uint32_t *ptr8 = (uint32_t *)(buffer + page_size - 13);
	uint32_t *ptr9 = (uint32_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		stress_nt_store32(ptr1, (uint32_t)i);
		stress_nt_store32(ptr2, (uint32_t)i);
		stress_nt_store32(ptr3, (uint32_t)i);
		stress_nt_store32(ptr4, (uint32_t)i);
		stress_nt_store32(ptr5, (uint32_t)i);
		stress_nt_store32(ptr6, (uint32_t)i);
		stress_nt_store32(ptr7, (uint32_t)i);
		stress_nt_store32(ptr8, (uint32_t)i);
		stress_nt_store32(ptr9, (uint32_t)i);
	}
}

#endif

static void stress_misaligned_int32inc(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);
	volatile uint32_t *ptr5 = (uint32_t *)(buffer + page_size - 1);
	volatile uint32_t *ptr6 = (uint32_t *)(buffer + page_size - 5);
	volatile uint32_t *ptr7 = (uint32_t *)(buffer + page_size - 9);
	volatile uint32_t *ptr8 = (uint32_t *)(buffer + page_size - 13);
	volatile uint32_t *ptr9 = (uint32_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();
		(*ptr3)++;
		shim_mb();
		(*ptr4)++;
		shim_mb();

		(*ptr5)++;
		shim_mb();
		(*ptr6)++;
		shim_mb();
		(*ptr7)++;
		shim_mb();
		(*ptr8)++;
		shim_mb();

		(*ptr9)++;
		shim_mb();
	}
}

#if defined(HAVE_ATOMIC_FETCH_ADD_4) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
static void stress_misaligned_int32atomic(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint32_t *ptr1 = (uint32_t *)(buffer + 1);
	volatile uint32_t *ptr2 = (uint32_t *)(buffer + 5);
	volatile uint32_t *ptr3 = (uint32_t *)(buffer + 9);
	volatile uint32_t *ptr4 = (uint32_t *)(buffer + 13);
	volatile uint32_t *ptr5 = (uint32_t *)(buffer + page_size - 1);
	volatile uint32_t *ptr6 = (uint32_t *)(buffer + page_size - 5);
	volatile uint32_t *ptr7 = (uint32_t *)(buffer + page_size - 9);
	volatile uint32_t *ptr8 = (uint32_t *)(buffer + page_size - 13);
	volatile uint32_t *ptr9 = (uint32_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		__atomic_fetch_add_4(ptr1, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr2, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr3, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr4, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_4(ptr5, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr6, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr7, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_4(ptr8, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_4(ptr9, 1, __ATOMIC_SEQ_CST);
		shim_mb();
	}
}
#endif

static void stress_misaligned_int64rd(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);
	volatile uint64_t *ptr3 = (uint64_t *)(buffer + page_size - 1);
	volatile uint64_t *ptr4 = (uint64_t *)(buffer + page_size - 9);
	volatile uint64_t *ptr5 = (uint64_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(void)*ptr1;
		shim_mb();
		(void)*ptr2;
		shim_mb();

		(void)*ptr3;
		shim_mb();
		(void)*ptr4;
		shim_mb();

		(void)*ptr5;
		shim_mb();
	}
}

static void stress_misaligned_int64wr(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);
	volatile uint64_t *ptr3 = (uint64_t *)(buffer + page_size - 1);
	volatile uint64_t *ptr4 = (uint64_t *)(buffer + page_size - 9);
	volatile uint64_t *ptr5 = (uint64_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		*ptr1 = (uint64_t)i;
		shim_mb();
		*ptr2 = (uint64_t)i;
		shim_mb();

		*ptr3 = (uint64_t)i;
		shim_mb();
		*ptr4 = (uint64_t)i;
		shim_mb();

		*ptr5 = (uint64_t)i;
		shim_mb();
	}
}

#if defined(HAVE_NT_STORE64)
static void stress_misaligned_int64wrnt(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	uint64_t *ptr2 = (uint64_t *)(buffer + 9);
	uint64_t *ptr3 = (uint64_t *)(buffer + page_size - 1);
	uint64_t *ptr4 = (uint64_t *)(buffer + page_size - 9);
	uint64_t *ptr5 = (uint64_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		stress_nt_store64(ptr1, (uint64_t)i);
		stress_nt_store64(ptr2, (uint64_t)i);
		stress_nt_store64(ptr3, (uint64_t)i);
		stress_nt_store64(ptr4, (uint64_t)i);
		stress_nt_store64(ptr5, (uint64_t)i);
	}
}
#endif

static void stress_misaligned_int64inc(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);
	volatile uint64_t *ptr3 = (uint64_t *)(buffer + page_size - 1);
	volatile uint64_t *ptr4 = (uint64_t *)(buffer + page_size - 9);
	volatile uint64_t *ptr5 = (uint64_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		(*ptr1)++;
		shim_mb();
		(*ptr2)++;
		shim_mb();

		(*ptr3)++;
		shim_mb();
		(*ptr4)++;
		shim_mb();

		(*ptr5)++;
		shim_mb();
	}
}

#if defined(HAVE_ATOMIC_FETCH_ADD_8) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
static void stress_misaligned_int64atomic(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile uint64_t *ptr1 = (uint64_t *)(buffer + 1);
	volatile uint64_t *ptr2 = (uint64_t *)(buffer + 9);
	volatile uint64_t *ptr3 = (uint64_t *)(buffer + page_size - 1);
	volatile uint64_t *ptr4 = (uint64_t *)(buffer + page_size - 9);
	volatile uint64_t *ptr5 = (uint64_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		__atomic_fetch_add_8(ptr1, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_8(ptr2, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_8(ptr3, 1, __ATOMIC_SEQ_CST);
		shim_mb();
		__atomic_fetch_add_8(ptr4, 1, __ATOMIC_SEQ_CST);
		shim_mb();

		__atomic_fetch_add_8(ptr5, 1, __ATOMIC_SEQ_CST);
		shim_mb();
	}
}
#endif

#if defined(HAVE_INT128_T)

/*
 *  Misaligned 128 bit fetches with SSE on x86 with some compilers
 *  with misaligned data may generate moveda rather than movdqu.
 *  For now, disable SSE optimization for x86 to workaround this
 *  even if it ends up generating two 64 bit reads.
 */
#if defined(__SSE__) && 	\
    defined(STRESS_ARCH_X86) &&	\
    defined(HAVE_TARGET_CLONES)
#define TARGET_CLONE_NO_SSE __attribute__ ((target("no-sse")))
#else
#define TARGET_CLONE_NO_SSE
#endif
static void TARGET_CLONE_NO_SSE stress_misaligned_int128rd(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);
	volatile __uint128_t *ptr2 = (__uint128_t *)(buffer + page_size - 1);
	volatile __uint128_t *ptr3 = (__uint128_t *)(buffer + 63);

	while (keep_running_no_sse() && --i) {
		(void)*ptr1;
		(void)*ptr2;
		(void)*ptr3;
	}
}

static void stress_misaligned_int128wr(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);
	volatile __uint128_t *ptr2 = (__uint128_t *)(buffer + page_size - 1);
	volatile __uint128_t *ptr3 = (__uint128_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		*ptr1 = (__uint128_t)i;
		shim_mb();

		*ptr2 = (__uint128_t)i;
		shim_mb();

		*ptr3 = (__uint128_t)i;
		shim_mb();
	}
}

#if defined(HAVE_NT_STORE128) && 0
static void stress_misaligned_int128wrnt(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	__uint128_t *ptr1 = (__uint128_t *)(buffer + 1);
	__uint128_t *ptr2 = (__uint128_t *)(buffer + page_size - 1);
	__uint128_t *ptr3 = (__uint128_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		stress_nt_store128(ptr1, (__uint128_t)i);
		stress_nt_store128(ptr2, (__uint128_t)i);
		stress_nt_store128(ptr3, (__uint128_t)i);
	}
}
#endif

static void TARGET_CLONE_NO_SSE stress_misaligned_int128inc(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);
	volatile __uint128_t *ptr2 = (__uint128_t *)(buffer + page_size - 1);
	volatile __uint128_t *ptr3 = (__uint128_t *)(buffer + 63);

	while (keep_running_no_sse() && --i) {
		(*ptr1)++;
		(*ptr2)++;
		(*ptr3)++;
	}
}

#if defined(HAVE_ATOMIC_FETCH_ADD_8) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
static void stress_misaligned_int128atomic(uint8_t *buffer, const size_t page_size)
{
	register int i = MISALIGN_LOOPS;
	volatile __uint128_t *ptr1 = (__uint128_t *)(buffer + 1);
	volatile __uint128_t *ptr2 = (__uint128_t *)(buffer + page_size - 1);
	volatile __uint128_t *ptr3 = (__uint128_t *)(buffer + 63);

	while (keep_stressing_flag() && --i) {
		/* No add 16 variant, so do 2 x 8 adds for now */
		__atomic_fetch_add_8(ptr1, 1, __ATOMIC_SEQ_CST);
		__atomic_fetch_add_8(ptr1 + 1, 1, __ATOMIC_SEQ_CST);

		__atomic_fetch_add_8(ptr2, 1, __ATOMIC_SEQ_CST);
		__atomic_fetch_add_8(ptr2 + 1, 1, __ATOMIC_SEQ_CST);

		__atomic_fetch_add_8(ptr3, 1, __ATOMIC_SEQ_CST);
		__atomic_fetch_add_8(ptr3 + 1, 1, __ATOMIC_SEQ_CST);
	}
}
#endif
#endif

static void stress_misaligned_all(uint8_t *buffer, const size_t page_size);

static stress_misaligned_method_info_t stress_misaligned_methods[] = {
	{ "all",	stress_misaligned_all,		false,	false },
	{ "int16rd",	stress_misaligned_int16rd,	false,	false },
	{ "int16wr",	stress_misaligned_int16wr,	false,	false },
	{ "int16inc",	stress_misaligned_int16inc,	false,	false },
#if defined(HAVE_ATOMIC_FETCH_ADD_2) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
	{ "int16atomic",stress_misaligned_int16atomic,	false,	false },
#endif
	{ "int32rd",	stress_misaligned_int32rd,	false,	false },
	{ "int32wr",	stress_misaligned_int32wr,	false,	false },
#if defined(HAVE_NT_STORE64)
	{ "int32wrnt",	stress_misaligned_int32wrnt,	false,	false },
#endif
	{ "int32inc",	stress_misaligned_int32inc,	false,	false },
#if defined(HAVE_ATOMIC_FETCH_ADD_4) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
	{ "int32atomic",stress_misaligned_int32atomic,	false,	false },
#endif
	{ "int64rd",	stress_misaligned_int64rd,	false,	false },
	{ "int64wr",	stress_misaligned_int64wr,	false,	false },
#if defined(HAVE_NT_STORE64)
	{ "int64wrnt",	stress_misaligned_int64wrnt,	false,	false },
#endif
	{ "int64inc",	stress_misaligned_int64inc,	false,	false },
#if defined(HAVE_ATOMIC_FETCH_ADD_8) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
	{ "int64atomic",stress_misaligned_int64atomic,	false,	false },
#endif
#if defined(HAVE_INT128_T)
	{ "int128rd",	stress_misaligned_int128rd,	false,	false },
	{ "int128wr",	stress_misaligned_int128wr,	false,	false },
#if defined(HAVE_NT_STORE128) && 0
	{ "int128wrnt",	stress_misaligned_int128wrnt,	false,	false },
#endif
	{ "int128inc",	stress_misaligned_int128inc,	false,	false },
#if defined(HAVE_ATOMIC_FETCH_ADD_8) &&	\
    defined(HAVE_ATOMIC) &&		\
    defined(__ATOMIC_SEQ_CST)
	{ "int128tomic",stress_misaligned_int128atomic,	false,	false },
#endif
#endif
	{ NULL,         NULL,				false,	false }
};

static void stress_misaligned_all(uint8_t *buffer, const size_t page_size)
{
	static bool exercised = false;
	stress_misaligned_method_info_t *info;

	for (info = &stress_misaligned_methods[1]; info->func && keep_stressing_flag(); info++) {
		if (info->disabled)
			continue;
		current_method = info;
		info->func(buffer, page_size);
		info->exercised = true;
		exercised = true;
	}

	if (!exercised)
		stress_misaligned_methods[0].disabled = true;
}

static MLOCKED_TEXT NORETURN void stress_misaligned_handler(int signum)
{
	handled_signum = signum;

	if (current_method)
		current_method->disabled = true;

	siglongjmp(jmp_env, 1);		/* Ugly, bounce back */
}

static void stress_misaligned_enable_all(void)
{
	stress_misaligned_method_info_t *info;

	for (info = stress_misaligned_methods; info->func; info++) {
		info->disabled = false;
		info->exercised = false;
	}
}

/*
 *  stress_misaligned_exercised()
 *	report the methods that were successfully exercised
 */
static void stress_misaligned_exercised(const stress_args_t *args)
{
	stress_misaligned_method_info_t *info;
	char *str = NULL;
	ssize_t str_len = 0;

	if (args->instance != 0)
		return;

	for (info = &stress_misaligned_methods[1]; info->func; info++) {
		if (info->exercised) {
			char *tmp;
			const size_t name_len = strlen(info->name);

			tmp = realloc(str, (size_t)str_len + name_len + 2);
			if (!tmp) {
				free(str);
				return;
			}
			str = tmp;
			if (str_len) {
				(void)shim_strlcpy(str + str_len, " ", 2);
				str_len++;
			}
			(void)shim_strlcpy(str + str_len, info->name, name_len + 1);
			str_len += name_len;
		}
	}

	if (str)
		pr_inf("%s: exercised %s\n", args->name, str);
	else
		pr_inf("%s: nothing exercised due to misalignment faults\n", args->name);

	free(str);
}

/*
 *  stress_set_misaligned_method()
 *      set default misaligned stress method
 */
static int stress_set_misaligned_method(const char *name)
{
	stress_misaligned_method_info_t const *info;

	for (info = stress_misaligned_methods; info->func; info++) {
		if (!strcmp(info->name, name)) {
			stress_set_setting("misaligned-method", TYPE_ID_UINTPTR_T, &info);
			return 0;
		}
	}

	(void)fprintf(stderr, "misaligned-method must be one of:");
	for (info = stress_misaligned_methods; info->func; info++) {
		(void)fprintf(stderr, " %s", info->name);
	}
	(void)fprintf(stderr, "\n");

	return -1;
}

static void stress_misaligned_set_default(void)
{
	stress_set_misaligned_method("all");
}

/*
 *  stress_misaligned()
 *	stress memory copies
 */
static int stress_misaligned(const stress_args_t *args)
{
	uint8_t *buffer;
	stress_misaligned_method_info_t *misaligned_method = &stress_misaligned_methods[0];
	int ret, rc;
	const size_t page_size = args->page_size;
	const size_t buffer_size = page_size << 1;

	(void)stress_get_setting("misaligned-method", &misaligned_method);

	if (stress_sighandler(args->name, SIGBUS, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;
	if (stress_sighandler(args->name, SIGILL, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;
	if (stress_sighandler(args->name, SIGSEGV, stress_misaligned_handler, NULL) < 0)
		return EXIT_NO_RESOURCE;

	stress_misaligned_enable_all();

	buffer = (uint8_t *)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (buffer == MAP_FAILED) {
		pr_inf("%s: cannot allocate 1 page buffer, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		return EXIT_NO_RESOURCE;
	}

	stress_set_proc_state(args->name, STRESS_STATE_RUN);

	current_method = misaligned_method;
	ret = sigsetjmp(jmp_env, 1);
	if ((ret == 1) && (args->instance == 0)) {
		pr_inf_skip("%s: skipping method %s, misaligned operations tripped %s\n",
			args->name, current_method->name,
			handled_signum == -1 ? "an error" :
			stress_strsignal(handled_signum));
	}

	rc = EXIT_SUCCESS;
	do {
		if (misaligned_method->disabled) {
			rc = EXIT_NO_RESOURCE;
			break;
		}
		misaligned_method->func(buffer, page_size);
		misaligned_method->exercised = true;
		inc_counter(args);
	} while (keep_stressing(args));

	(void)stress_sighandler_default(SIGBUS);
	(void)stress_sighandler_default(SIGILL);
	(void)stress_sighandler_default(SIGSEGV);

	stress_misaligned_exercised(args);

	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	(void)munmap((void *)buffer, buffer_size);

	return rc;
}

static const stress_opt_set_func_t opt_set_funcs[] = {
	{ OPT_misaligned_method,	stress_set_misaligned_method },
	{ 0,			NULL }
};

stressor_info_t stress_misaligned_info = {
	.stressor = stress_misaligned,
	.set_default = stress_misaligned_set_default,
	.class = CLASS_CPU_CACHE | CLASS_MEMORY,
	.opt_set_funcs = opt_set_funcs,
	.help = help
};
