/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */

#ifndef __HASH_H
#define __HASH_H

#include <inttypes.h>

uint32_t hash_u32_data( uint8_t *data, uint16_t len );
uint32_t hash_u32_string( char *src );

#ifdef PGM_P
uint32_t hash_u32_string_P( PGM_P src );
#endif

uint32_t hash_u32_start( void );
uint32_t hash_u32_partial( uint32_t hash, uint8_t *data, uint16_t len );
uint32_t hash_u32_single( uint32_t hash, uint8_t data );

uint64_t hash_u64_data( uint8_t *data, uint16_t len );
uint64_t hash_u64_string( char *src );

#ifdef PGM_P
uint64_t hash_u64_string_P( PGM_P src );
#endif

uint64_t hash_u64_start( void );
uint64_t hash_u64_partial( uint64_t hash, uint8_t *data, uint16_t len );
uint64_t hash_u64_single( uint64_t hash, uint8_t data );

#endif
