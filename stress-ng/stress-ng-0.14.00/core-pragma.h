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
#ifndef CORE_PRAGMA_H
#define CORE_PRAGMA_H

#if defined(__clang__) &&	\
    NEED_CLANG(4, 0, 0) &&	\
    defined(HAVE_PRAGMA)
#define STRESS_PRAGMA_PUSH	_Pragma("GCC diagnostic push")
#define STRESS_PRAGMA_POP	_Pragma("GCC diagnostic pop")
#define STRESS_PRAGMA_WARN_OFF	_Pragma("GCC diagnostic ignored \"-Weverything\"")
#elif defined(__GNUC__) &&	\
      defined(HAVE_PRAGMA) &&	\
      NEED_GNUC(4, 4, 0)
#define STRESS_PRAGMA_PUSH	_Pragma("GCC diagnostic push")
#define STRESS_PRAGMA_POP	_Pragma("GCC diagnostic pop")
#define STRESS_PRAGMA_WARN_OFF	_Pragma("GCC diagnostic ignored \"-Wall\"") \
				_Pragma("GCC diagnostic ignored \"-Wextra\"") \
				_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"") \
				_Pragma("GCC diagnostic ignored \"-Wcast-qual\"") \
				_Pragma("GCC diagnostic ignored \"-Wnonnull\"")
#else
#define STRESS_PRAGMA_PUSH
#define STRESS_PRAGMA_POP
#define STRESS_PRAGMA_WARN_OFF
#endif

#endif
