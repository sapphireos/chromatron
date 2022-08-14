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
 

#ifndef CRC_H
#define CRC_H

#include <inttypes.h>

#define BOOTLOADER

#define CRC_SIZE 2

void crc_v_init( void );
uint16_t crc_u16_block(uint8_t *ptr, uint16_t length);
uint16_t crc_u16_partial_block(uint16_t crc, uint8_t *ptr, uint16_t length);
uint16_t crc_u16_byte(uint16_t crc, uint8_t data);
//uint16_t crc_u16_inline(uint16_t crc, uint8_t data);

#endif

