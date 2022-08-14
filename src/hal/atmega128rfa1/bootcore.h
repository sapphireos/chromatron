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

#ifndef BOOTCORE_H
#define BOOTCORE_H

void boot_v_init( void );
void boot_v_erase_page( uint16_t pagenumber );
void boot_v_write_page( uint16_t pagenumber, uint8_t *data );
void boot_v_read_page( uint16_t pagenumber, uint8_t *dest );
void boot_v_read_data( uint32_t offset, uint8_t *dest, uint16_t length );
uint16_t boot_u16_get_page_size( void );

#endif
