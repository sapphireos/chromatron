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
 

#ifndef _EEPROM_H
#define _EEPROM_H

#include "system.h"
#include "target.h"

#define EE_ARRAY_SIZE 4096 // size of eeprom storage area in bytes

#define EE_ERASE_ONLY 		0b00010000
#define EE_WRITE_ONLY 		0b00100000

void ee_v_write_byte_blocking( uint16_t address, uint8_t data );
void ee_v_write_block( uint16_t address, uint8_t *data, uint16_t len );
uint8_t ee_u8_read_byte( uint16_t address );
void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t length );

#endif

