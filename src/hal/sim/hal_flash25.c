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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

/*

25 Series SPI Flash Memory Driver (sim)

File-backed implementation of the 25 series flash API: the flash is
mapped at FLASH_START the way real hardware exposes its flash as
memory-mapped, so the file system works normally in the simulator.
The backing file (sim_flash.bin, in the current directory) persists
across reboots, the same way real flash does.

*/

#define SIM_FLASH_FILE "sim_flash.bin"

// firmware info embedded in the binary: the build stamps a packed
// fw_info_t into this symbol (ExeBuilder.stamp_fwinfo) with the same
// fields the firmware builds stamp into the image.  The HAL copies it
// into the simulated flash at boot, mirroring the bootloader.
const fw_info_t sim_fwinfo_storage;

extern uint16_t block0_unlock;

static uint8_t *array;

static uint32_t max_address;

// Program the firmware info into the flash, mirroring what the
// bootloader does in firmware builds: the build embeds a packed
// fw_info_t in the binary (sim_fwinfo_storage, see
// ExeBuilder.stamp_fwinfo) and it is copied into the location the OS
// reads it from.  Stamped on every boot so a rebuilt version is
// picked up without touching the persistent flash file.
static void _v_program_fwinfo( void ){

    // an unstamped binary (e.g. linked without the build's post
    // processing) has an all-zero fwid: leave the flash content as-is
    int stamped = 0;

    for( uint8_t i = 0; i < FW_ID_LENGTH; i++ ){

        if( sim_fwinfo_storage.fwid[i] != 0 ){

            stamped = 1;
            break;
        }
    }

    if( !stamped ){

        return;
    }

    // the fw info lives in block 0, which is write protected by default
    flash25_v_unlock_block0();

    flash25_v_write( FW_INFO_ADDRESS, &sim_fwinfo_storage, sizeof(fw_info_t) );
}


void hal_flash25_v_init( void ){

    // size the backing file from the capacity of the emulated chip (the
    // same value the OS derives from the device id), so the file system
    // can use the whole part
    max_address = flash25_u32_read_capacity_from_info();

    int fd = open( SIM_FLASH_FILE, O_RDWR | O_CREAT, 0644 );

    if( fd < 0 ){

        fprintf( stderr, "sim: failed to open %s\n", SIM_FLASH_FILE );
        exit( 1 );
    }

    // a brand new file is all zeros; remember so we can pre-erase it
    // like a fresh chip. an existing file keeps its contents.
    struct stat st;
    int fresh = ( fstat( fd, &st ) == 0 && st.st_size == 0 );

    if( ftruncate( fd, max_address ) < 0 ){

        fprintf( stderr, "sim: failed to size %s\n", SIM_FLASH_FILE );
        exit( 1 );
    }

    array = ( uint8_t *)mmap( ( void *)FLASH_START, max_address,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_FIXED, fd, 0 );

    close( fd );

    if( array == MAP_FAILED ){

        // the OS reads flash through raw pointers at FLASH_START, so
        // there is no fallback; fail loudly instead of diverging.
        fprintf( stderr, "sim: failed to map flash at 0x%lx\n", ( unsigned long )FLASH_START );
        exit( 1 );
    }

    if( fresh ){

        memset( array, 0xff, max_address );
    }

    // program the firmware info (block 0) before anything reads it
    _v_program_fwinfo();
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
    memset( &array[FLASH_FS_ERASE_BLOCK_SIZE], 0xff, ( max_address - FLASH_FS_ERASE_BLOCK_SIZE ) );
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
