/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "target.h"

#include "boot_data.h"
#include "bootcore.h"
#include "crc.h"
#include "eeprom.h"
#include "flash25.h"
#include "flash_fs_partitions.h"
#include "spi.h"
#include "watchdog.h"
#include "hal_nvm.h"

#include "loader.h"

#include <string.h>

void ldr_v_erase_app( void ){

	nvm_v_erase_app_flash();
}

void ldr_v_copy_partition_to_internal( void ){

    ldr_v_erase_app();
	// page data buffer
	uint8_t buf[PAGE_SIZE];

	// calculate number of pages
	uint16_t n_pages = ( ldr_u32_read_partition_length() / PAGE_SIZE ) + 1;

	// bounds check pages
	if( n_pages > N_APP_PAGES ){

		n_pages = N_APP_PAGES;
	}

	// loop through pages
	for( uint16_t i = 0; i < n_pages; i++ ){

		// load page data
		ldr_v_read_partition_data( (uint32_t)i * PAGE_SIZE, buf, PAGE_SIZE );

		// write page data to app page
		boot_v_write_page( i, buf );

		// reset watchdog timer
		wdg_v_reset();
	}
}

// typedef void (*pFunction)(void);
// pFunction JumpToApplication;
// uint32_t jumpAddress;


void ldr_run_app( void ){

    // scan for firmware image
    esp_image_header_t image_header;
    uint32_t addr = FLASH_START;
    SPIRead( addr, &image_header, sizeof(image_header) );

    trace_printf("image: %x %u %x %x %x",
    	image_header.magic,
    	image_header.segment_count,
    	image_header.flash_mode,
    	image_header.flash_info,
    	image_header.entry_addr);

   	addr += sizeof(image_header);

    for( uint8_t i = 0; i < image_header.segment_count; i++ ){

    	esp_section_header_t section_header;
    	SPIRead( addr, &section_header, sizeof(image_header) );

		trace_printf("section: %x %x",
			section_header.addr,
			section_header.length);

    	addr += sizeof(section_header);	
    }

	// DISABLE_INTERRUPTS;
	// wdg_v_disable();
	
	// jumpAddress = *(volatile uint32_t *)( FLASH_START );
 
	// JumpToApplication = (pFunction)jumpAddress;

	// JumpToApplication();
}


// read data from an external partition
void ldr_v_read_partition_data( uint32_t offset, uint8_t *dest, uint16_t length ){

	flash25_v_read( offset + FLASH_FS_FIRMWARE_0_PARTITION_START, dest, length );
}

// return length of an external partition (actual length of file, not partition size)
uint32_t ldr_u32_read_partition_length( void ){

	uint32_t partition_length;
	uint32_t start_address = FLASH_FS_FIRMWARE_0_PARTITION_START;

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

	memcpy_P( &internal_length, (void *)(FLASH_START + FW_LENGTH_ADDRESS), sizeof(internal_length) );

	internal_length += sizeof(uint16_t);

	if( internal_length > ( (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE ) ){

		internal_length = (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE;
	}

	return internal_length;
}

uint16_t ldr_u16_get_internal_crc( void ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_internal_length();

	uint32_t *ptr = (uint32_t *)FLASH_START;

	for( uint32_t i = 0; i < length / 4; i++ ){

		// this is wrong, but demonstrates a proper 32 bit aligned read.
		crc = crc_u16_byte( crc, *ptr & 0xff );

		ptr++;

		// reset watchdog timer
		wdg_v_reset();
	}

	return crc_u16_finish( crc );
}

// compute CRC of external partition
uint16_t ldr_u16_get_partition_crc( void ){

	uint16_t crc = crc_u16_start();

	uint32_t length = ldr_u32_read_partition_length();

    uint32_t address = FLASH_FS_FIRMWARE_0_PARTITION_START;

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
