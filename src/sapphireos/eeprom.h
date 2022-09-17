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

#include "hal_eeprom.h"

void ee_v_init( void );
bool ee_b_busy( void );
void ee_v_write_byte_blocking( uint16_t address, uint8_t data );
void ee_v_write_block( uint16_t address, const uint8_t *data, uint16_t len );
void ee_v_erase_block( uint16_t address, uint16_t len );
uint8_t ee_u8_read_byte( uint16_t address );
void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t len );
void ee_v_commit( void );

#endif
