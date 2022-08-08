/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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



#ifndef _RANDOM_H
#define _RANDOM_H

#include <stdint.h>

void rnd_v_init( void );
void rnd_v_seed( uint64_t seed );
uint64_t rnd_u64_get_seed( void );
uint8_t rnd_u8_get_int_with_seed( uint64_t *seed );
uint16_t rnd_u16_get_int_with_seed( uint64_t *seed );
uint32_t rnd_u32_get_int_with_seed( uint64_t *seed );
uint8_t rnd_u8_get_int( void );
uint16_t rnd_u16_get_int( void );
uint32_t rnd_u32_get_int( void );
uint16_t rnd_u16_get_int_hw( void );
void rnd_v_fill( uint8_t *data, uint16_t len );
uint32_t rnd_u32_range_with_seed( uint64_t *seed, uint32_t range );
uint32_t rnd_u32_range( uint32_t range );
uint16_t rnd_u16_range_with_seed( uint64_t *seed, uint16_t range );
uint16_t rnd_u16_range( uint16_t range );

#endif
