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

#include "system.h"

#include "hash.h"
#include "crc.h"

#include <string.h>


// int32_t hash_i32_string( char *src ){

//     uint8_t len = strnlen( src, 255 );

//     uint16_t crc0;
//     uint16_t crc1;

//     crc0 = crc_u16_block( (uint8_t *)src, len );

//     crc1 = crc_u16_start();

//     uint8_t *ptr = (uint8_t *)src + ( len - 1 );

//     while( len > 0 ){

//         crc1 = crc_u16_byte( crc1, (*ptr) ^ 0x15 );

//         ptr--;
//         len--;
//     }

//     crc1 = crc_u16_finish( crc1 );

//     return ( (uint32_t)crc0 << 16 ) | crc1;
// }

// int32_t hash_i32_string_P( PGM_P src ){

//     char buf[64];
//     strlcpy_P( buf, src, sizeof(buf) );

//     return hash_i32_string( buf );
// }


