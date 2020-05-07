/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#ifndef _FFS_EEPROM_H
#define _FFS_EEPROM_H

void ffs_eeprom_v_write( uint8_t block, uint16_t addr, uint8_t *src, uint16_t len );
void ffs_eeprom_v_read( uint8_t block, uint16_t addr, uint8_t *dest, uint16_t len );
void ffs_eeprom_v_erase( uint8_t block );

#endif
