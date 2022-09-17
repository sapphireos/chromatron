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

#ifndef _FLASH_FS_PARTITIONS_H
#define _FLASH_FS_PARTITIONS_H

#include "system.h"

#define FLASH_FS_ERASE_BLOCK_SIZE		4096

#define FLASH_FS_VERSION_ADDR           16
#define FLASH_FS_HW_TYPE_ADDR           32

#define FLASH_FS_HW_TYPE_UNSET          0xff

// Partitions:
#define PARTITON_FIRMWARE       0
#define PARTITON_RECOVERY       1
#define PARTITON_SECONDARY      2

#define FLASH_FS_PARTITION_START            ( FLASH_FS_ERASE_BLOCK_SIZE ) // start at block 1

// Firmware 0 partition
#define FLASH_FS_FIRMWARE_0_PARTITION_START	( FLASH_FS_PARTITION_START )
#define FLASH_FS_FIRMWARE_0_PARTITION_SIZE	( (uint32_t)FLASH_FS_FIRMWARE_0_SIZE_KB * (uint32_t)1024 ) // in bytes
#define FLASH_FS_FIRMWARE_0_N_BLOCKS	    ( FLASH_FS_FIRMWARE_0_PARTITION_SIZE / FLASH_FS_ERASE_BLOCK_SIZE ) // in blocks

// Firmware 1 partition
#define FLASH_FS_FIRMWARE_1_PARTITION_START	( FLASH_FS_FIRMWARE_0_PARTITION_START + FLASH_FS_FIRMWARE_0_PARTITION_SIZE )
#define FLASH_FS_FIRMWARE_1_PARTITION_SIZE	( (uint32_t)FLASH_FS_FIRMWARE_1_SIZE_KB * (uint32_t)1024 ) // in bytes
#define FLASH_FS_FIRMWARE_1_N_BLOCKS	    ( FLASH_FS_FIRMWARE_1_PARTITION_SIZE / FLASH_FS_ERASE_BLOCK_SIZE ) // in blocks

// Firmware 2 partition
#define FLASH_FS_FIRMWARE_2_PARTITION_START ( FLASH_FS_FIRMWARE_1_PARTITION_START + FLASH_FS_FIRMWARE_1_PARTITION_SIZE )
#define FLASH_FS_FIRMWARE_2_PARTITION_SIZE  ( (uint32_t)FLASH_FS_FIRMWARE_2_SIZE_KB * (uint32_t)1024 ) // in bytes
#define FLASH_FS_FIRMWARE_2_N_BLOCKS        ( FLASH_FS_FIRMWARE_2_PARTITION_SIZE / FLASH_FS_ERASE_BLOCK_SIZE ) // in blocks

// EEPROM partition
#define FLASH_FS_EEPROM_PARTITION_START 	( FLASH_FS_FIRMWARE_2_PARTITION_START + FLASH_FS_FIRMWARE_2_PARTITION_SIZE )
#define FLASH_FS_EEPROM_PARTITION_SIZE 	 	( (uint32_t)FLASH_FS_EEPROM_SIZE_KB * (uint32_t)1024 ) // in bytes
#define FLASH_FS_EEPROM_N_BLOCKS        	( FLASH_FS_EEPROM_PARTITION_SIZE / FLASH_FS_ERASE_BLOCK_SIZE ) // in blocks

// User file system partition 4:
// file system size is the size of the file system partition
#define FLASH_FS_FILE_SYSTEM_START		( FLASH_FS_EEPROM_PARTITION_START + \
                                          FLASH_FS_EEPROM_PARTITION_SIZE )

#define FLASH_FS_FILE_SYSTEM_START_BLOCK ( FLASH_FS_FILE_SYSTEM_START / FLASH_FS_ERASE_BLOCK_SIZE )

#endif
