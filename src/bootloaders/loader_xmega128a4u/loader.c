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


#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>


#include "system.h"
#include "target.h"

#include "boot_data.h"
#include "bootcore.h"
#include "crc.h"
#include "eeprom.h"
#include "flash25.h"
#include "flash_fs_partitions.h"
#include "spi.h"
#include "watchdog.h"

#include "loader.h"

#include <string.h>


void ldr_v_erase_app( void ){

	// erase app section
	for( uint16_t i = 0; i < N_APP_PAGES; i++ ){

		boot_v_erase_page( i );

		// reset watchdog timer
		wdg_v_reset();
	}
}

void ldr_v_copy_partition_to_internal( void ){

    ldr_v_erase_app();

	// page data buffer
	uint8_t buf[PAGE_SIZE];

	// calculate number of pages
	uint16_t n_pages = ( ldr_u32_read_partition_length( 0 ) / PAGE_SIZE ) + 1;

	// bounds check pages
	if( n_pages > N_APP_PAGES ){

		n_pages = N_APP_PAGES;
	}

	// loop through pages
	for( uint16_t i = 0; i < n_pages; i++ ){

		// load page data
		ldr_v_read_partition_data( 0, (uint32_t)i * PAGE_SIZE, buf, PAGE_SIZE );

		// write page data to app page
		boot_v_write_page( i, buf );

		// reset watchdog timer
		wdg_v_reset();
	}
}

void ldr_v_copy_recovery_to_internal( void ){

    ldr_v_erase_app();

    // page data buffer
    uint8_t buf[PAGE_SIZE];

    // calculate number of pages
    uint16_t n_pages = ( ldr_u32_read_partition_length( 1 ) / PAGE_SIZE ) + 1;

    // bounds check pages
    if( n_pages > N_APP_PAGES ){

        n_pages = N_APP_PAGES;
    }

    // loop through pages
    for( uint16_t i = 0; i < n_pages; i++ ){

        // load page data
        ldr_v_read_partition_data( 1, (uint32_t)i * PAGE_SIZE, buf, PAGE_SIZE );

        // write page data to app page
        boot_v_write_page( i, buf );

        // reset watchdog timer
        wdg_v_reset();
    }
}

void (*app_ptr)( void ) = 0x0000;

void ldr_run_app( void ){

	DISABLE_INTERRUPTS;

    #ifndef LOADER_MODULE_TEST
        #ifdef LDR_HAS_EIND
        EIND = 0x00;
        #endif
    app_ptr(); // Jump to Reset vector 0x0000 in Application Section.
    #else
    app_run = TRUE;
    #endif
}

// read data from an external partition
void ldr_v_read_partition_data( uint8_t partition, uint32_t offset, uint8_t *dest, uint16_t length ){

    uint32_t addr = offset;

    if( partition == 1 ){

        addr += FLASH_FS_FIRMWARE_1_PARTITION_START;
    }
    else{

        addr += FLASH_FS_FIRMWARE_0_PARTITION_START;   
    }

	flash25_v_read( addr, dest, length );
}

// return length of an external partition (actual length of file, not partition size)
uint32_t ldr_u32_read_partition_length( uint8_t partition ){

	uint32_t partition_length;
	uint32_t start_address = FLASH_FS_FIRMWARE_0_PARTITION_START;

    if( partition == 1 ){

        start_address = FLASH_FS_FIRMWARE_1_PARTITION_START;
    }

	flash25_v_read( FW_LENGTH_ADDRESS + start_address, &partition_length, sizeof(partition_length) );

    partition_length += sizeof(uint16_t);

	if( partition_length > ( (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE ) ){

		partition_length = (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE;
	}

	return partition_length;
}

// return the application length
uint32_t ldr_u32_read_internal_length( void ){

	uint32_t internal_length;

	memcpy_P( &internal_length, (void *)FW_LENGTH_ADDRESS, sizeof(internal_length) );

	internal_length += sizeof(uint16_t);

	if( internal_length > ( (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE ) ){

		internal_length = (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE;
	}

	return internal_length;
}

uint16_t ldr_u16_get_internal_crc( void ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_internal_length();

	for( uint32_t i = 0; i < length; i++ ){

		crc = crc_u16_byte( crc, pgm_read_byte_far( i ) );

		// reset watchdog timer
		wdg_v_reset();
	}

	return crc_u16_finish( crc );
}

// compute CRC of external partition
uint16_t ldr_u16_get_partition_crc( uint8_t partition ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_partition_length( partition );

    uint32_t address = FLASH_FS_FIRMWARE_0_PARTITION_START;

    if( partition == 1 ){

        address = FLASH_FS_FIRMWARE_1_PARTITION_START;
    }

	while( length > 0 ){

        uint8_t buf[PAGE_SIZE];
        uint16_t copy_len = PAGE_SIZE;

        if( (uint32_t)copy_len > length ){

            copy_len = length;
        }

        flash25_v_read( address, buf, copy_len );

        address += copy_len;
        length -= copy_len;

		crc = crc_u16_partial_block( crc, buf, copy_len );

		// reset watchdog timer
		wdg_v_reset();
	}

	return crc_u16_finish( crc );
}
