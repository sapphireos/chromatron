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

#ifndef _FFS_FW_H
#define _FFS_FW_H


int8_t ffs_fw_i8_init( void );

uint16_t ffs_fw_u16_crc( void );
uint32_t ffs_fw_u32_read_internal_length( void );
// uint16_t ffs_fw_u16_get_internal_crc( void );
uint32_t ffs_fw_u32_size( uint8_t partition );
void ffs_fw_v_erase( uint8_t partition, bool immediate );
int32_t ffs_fw_i32_read( uint8_t partition, uint32_t position, void *data, uint32_t len );
int32_t ffs_fw_i32_write( uint8_t partition, uint32_t position, const void *data, uint32_t len );


#endif

