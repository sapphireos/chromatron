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

#include "ffs_global.h"
#include "flash25.h"
#include "flash_fs_partitions.h"
#include "ffs_eeprom.h"

uint16_t eeprom_erase_unlock;

static uint32_t flash_addr( uint8_t block, uint16_t addr ){

	return FLASH_FS_EEPROM_PARTITION_START + 
    	   ( (uint32_t)block * FLASH_FS_ERASE_BLOCK_SIZE ) +
    	   addr;
}

void ffs_eeprom_v_write( uint8_t block, uint16_t addr, uint8_t *src, uint16_t len ){

	// trace_printf("EE write: %d 0x%x -> 0x%x %d 0x%x", block, addr, flash_addr( block, addr ), len, src[0]);

	flash25_v_write( flash_addr( block, addr ), src, len );
}

void ffs_eeprom_v_read( uint8_t block, uint16_t addr, uint8_t *dest, uint16_t len ){

	// flash25_v_read( flash_addr( block, addr ), dest, len );	

	trace_printf("EE read: %d 0x%x -> 0x%x %d 0x%x", block, addr, flash_addr( block, addr ), len, dest[0]);
}

void ffs_eeprom_v_erase( uint8_t block ){

    eeprom_erase_unlock = EEPROM_UNLOCK_CODE;
	flash25_v_write_enable();
	flash25_v_erase_4k( flash_addr( block, 0 ) );
    eeprom_erase_unlock = 0;
}


