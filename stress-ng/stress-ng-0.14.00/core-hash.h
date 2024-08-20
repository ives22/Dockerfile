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
#ifndef CORE_HASH_H
#define CORE_HASH_H

/*
 *  Hashing core functions
 */
extern WARN_UNUSED stress_hash_table_t *stress_hash_create(const size_t n);
extern stress_hash_t *stress_hash_add(stress_hash_table_t *hash_table,
	const char *str);
extern WARN_UNUSED stress_hash_t *stress_hash_get(
	stress_hash_table_t *hash_table, const char *str);
extern void stress_hash_delete(stress_hash_table_t *hash_table);

extern WARN_UNUSED uint32_t stress_hash_adler32(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_coffin(const char *str);
extern WARN_UNUSED uint32_t stress_hash_coffin32_be(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_coffin32_le(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_crc32c(const char *str);
extern WARN_UNUSED uint32_t stress_hash_djb2a(const char *str);
extern WARN_UNUSED uint32_t stress_hash_fnv1a(const char *str);
extern WARN_UNUSED uint32_t stress_hash_jenkin(const uint8_t *data, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_kandr(const char *str);
extern WARN_UNUSED uint32_t stress_hash_knuth(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_loselose(const char *str);
extern WARN_UNUSED uint32_t stress_hash_mid5(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_muladd32(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_muladd64(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_mulxror64(const char *str, const size_t len);
extern WARN_UNUSED uint32_t stress_hash_murmur3_32(const uint8_t *key, size_t len, uint32_t seed);
extern WARN_UNUSED uint32_t stress_hash_nhash(const char *str);
extern WARN_UNUSED uint32_t stress_hash_pjw(const char *str);
extern WARN_UNUSED uint32_t stress_hash_sdbm(const char *str);
extern WARN_UNUSED uint32_t stress_hash_x17(const char *str);

#endif
