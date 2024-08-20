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
#ifndef CORE_NT_STORE_H
#define CORE_NT_STORE_H

#if defined(HAVE_XMMINTRIN_H)
#include <xmmintrin.h>
#endif

/* Non-temporal store writes */

/*
 *  128 bit non-temporal stores
 */
#if defined(HAVE_INT128_T) &&				\
    defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_NONTEMPORAL_STORE)
/* Clang non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store128(__uint128_t *addr, register __uint128_t value)
{
	__builtin_nontemporal_store(value, addr);
}
#define HAVE_NT_STORE128
#elif defined(HAVE_XMMINTRIN_H) &&			\
    defined(HAVE_INT128_T) &&				\
    defined(HAVE_V2DI) && 				\
    (defined(__x86_64__) || defined(__x86_64)) && 	\
    defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_IA32_MOVNTDQ)
/* gcc x86 non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store128(__uint128_t *addr, register __uint128_t value)
{
	__builtin_ia32_movntdq((__v2di *)addr, (__v2di)value);
}
#define HAVE_NT_STORE128
#endif


/*
 *  64 bit non-temporal stores
 */
#if defined(HAVE_BUILTIN_SUPPORTS) &&	\
    defined(HAVE_BUILTIN_NONTEMPORAL_STORE)
/* Clang non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store64(uint64_t *addr, register uint64_t value)
{
	__builtin_nontemporal_store(value, addr);
}
#define HAVE_NT_STORE64
#elif defined(HAVE_XMMINTRIN_H) &&			\
    (defined(__x86_64__) || defined(__x86_64)) && 	\
    defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_IA32_MOVNTI64)
/* gcc x86 non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store64(uint64_t *addr, register uint64_t value)
{
	__builtin_ia32_movnti64((long long int *)addr, (long long int)value);
}
#define HAVE_NT_STORE64
#endif

/*
 *  32 bit non-temporal stores
 */
#if defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_NONTEMPORAL_STORE)
/* Clang non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store32(uint32_t *addr, register uint32_t value)
{
	__builtin_nontemporal_store(value, addr);
}
#define HAVE_NT_STORE32
#elif defined(HAVE_XMMINTRIN_H) &&			\
    (defined(__x86_64__) || defined(__x86_64)) && 	\
    defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_IA32_MOVNTI)
/* gcc x86 non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store32(uint32_t *addr, register uint32_t value)
{
	__builtin_ia32_movnti((int *)addr, value);
}
#define HAVE_NT_STORE32
#endif

/*
 *  double precision float non-temporal stores
 */
#if defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_NONTEMPORAL_STORE)
/* Clang non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store_double(double *addr, register double value)
{
	__builtin_nontemporal_store(value, addr);
}
#define HAVE_NT_STORE_DOUBLE
#elif defined(HAVE_XMMINTRIN_H) &&			\
    (defined(__x86_64__) || defined(__x86_64)) && 	\
    defined(HAVE_BUILTIN_SUPPORTS) &&			\
    defined(HAVE_BUILTIN_IA32_MOVNTI64)
/* gcc x86 non-temporal stores */
static inline void ALWAYS_INLINE OPTIMIZE3 stress_nt_store_double(double *addr, register double value)
{
	__builtin_ia32_movnti64((long long int *)addr, value);
}
#define HAVE_NT_STORE_DOUBLE
#endif

#endif
