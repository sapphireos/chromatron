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

#include "system.h"
#include "flash_fs_partitions.h"
#include "flash25.h"
#include "hal_flash25.h"

#include <string.h>

/*

25 Series SPI Flash Memory Driver (sim)

RAM-backed implementation of the 25 series flash API: the flash is a
plain memory array, so the file system works normally in the simulator.

*/

extern uint16_t block0_unlock;

static uint8_t array[FLASH_LENGTH];

static uint32_t max_address;

void hal_flash25_v_init( void ){

    memset( array, 0xff, sizeof(array) );

    max_address = sizeof(array);
}

uint8_t flash25_u8_read_status( void ){

    return 0;
}

void flash25_v_write_status( uint8_t status ){

}

// read len bytes into ptr
void flash25_v_read( uint32_t address, void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    if( len == 0 ){

        return;
    }

    memcpy( ptr, &array[address], len );
}

// read a single byte
uint8_t flash25_u8_read_byte( uint32_t address ){

    ASSERT( address < max_address );

    uint8_t byte;

    flash25_v_read( address, &byte, sizeof(byte) );

    return byte;
}

void flash25_v_write_enable( void ){

}

void flash25_v_write_disable( void ){

}

static void _write_byte( uint32_t address, uint8_t byte ){

    array[address] = byte;
}

// write a single byte to the device
void flash25_v_write_byte( uint32_t address, uint8_t byte ){

    ASSERT( address < max_address );

    // don't write to block 0
    if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
    }

    _write_byte( address, byte );
}

// write an array of data
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len ){

    ASSERT( address < max_address );

    // don't write to block 0
    if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
    }

    if( len == 0 ){

        return;
    }

    memcpy( &array[address], ptr, len );
}

// erase a 4 KB block
void flash25_v_erase_4k( uint32_t address ){

    ASSERT( address < max_address );

    // don't erase block 0
    if( address < FLASH_FS_ERASE_BLOCK_SIZE ){

        // unless unlocked
        if( block0_unlock != BLOCK0_UNLOCK_CODE ){

            return;
        }

        block0_unlock = 0;
    }

    memset( &array[address], 0xff, FLASH_FS_ERASE_BLOCK_SIZE );
}

// erase the entire chip
void flash25_v_erase_chip( void ){

    // block 0 is skipped, as with the hardware chip erase path
    memset( &array[FLASH_FS_ERASE_BLOCK_SIZE], 0xff, ( sizeof(array) - FLASH_FS_ERASE_BLOCK_SIZE ) );
}

void flash25_v_read_device_info( flash25_device_info_t *info ){

    // report as a 512 KB part so the capacity lookup works
    info->mfg_id = FLASH_MFG_SST;
    info->dev_id_1 = FLASH_DEV_ID1_SST25;
    info->dev_id_2 = FLASH_DEV_ID2_SST25_4MBIT;
}

uint32_t flash25_u32_capacity( void ){

    return max_address;
}
