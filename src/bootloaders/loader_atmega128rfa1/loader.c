/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
 */
 

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/boot.h>
#include <avr/wdt.h>


#include "system.h"
#include "target.h"

#include "boot_data.h"
#include "bootcore.h"
#include "crc.h"
#include "eeprom.h"
#include "flash25.h"
#include "flash_fs_partitions.h"
#include "spi.h"

#include "loader.h"

#include <string.h>


void ldr_v_erase_app( void ){

	// erase app section
	for( uint16_t i = 0; i < N_APP_PAGES; i++ ){
		
		boot_v_erase_page( i );
		
		// reset watchdog timer
		wdt_reset();
	}	
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
		wdt_reset();
	}
}

void ldr_run_app( void ){

	cli();
	
    #ifndef LOADER_MODULE_TEST
    asm("jmp 0"); // jump to application section
    #else
    app_run = TRUE;
    #endif
}

void ldr_v_set_green_led( void ){
    
    LDR_LED_GREEN_PORT |= _BV(LDR_LED_GREEN_PIN);
}

void ldr_v_clear_green_led( void ){
    
    LDR_LED_GREEN_PORT &= ~_BV(LDR_LED_GREEN_PIN);
}

void ldr_v_set_yellow_led( void ){
    
    LDR_LED_YELLOW_PORT |= _BV(LDR_LED_YELLOW_PIN);
}

void ldr_v_clear_yellow_led( void ){
    
    LDR_LED_YELLOW_PORT &= ~_BV(LDR_LED_YELLOW_PIN);
}

void ldr_v_set_red_led( void ){

    LDR_LED_RED_PORT |= _BV(LDR_LED_RED_PIN);
}

void ldr_v_clear_red_led( void ){

    LDR_LED_RED_PORT &= ~_BV(LDR_LED_RED_PIN);
}

// set the system clock prescaler.
void ldr_v_set_clock_prescaler( sys_clock_t8 prescaler ){
	
    #ifndef LOADER_MODULE_TEST
	CLKPR = 0b10000000; // enable the prescaler change
	CLKPR = prescaler; // set the prescaler
    #endif
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

	static uint32_t internal_length;

	memcpy_P( &internal_length, (void *)FW_LENGTH_ADDRESS, sizeof(internal_length) );
	
	internal_length += sizeof(uint16_t);
	
	if( internal_length > ( (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE ) ){
		
		internal_length = (uint32_t)N_APP_PAGES * (uint32_t)PAGE_SIZE;
	}
	
	return internal_length;
}

uint16_t ldr_u16_get_internal_crc( void ){
	
	static uint16_t crc;
	crc = 0xffff;
	
	uint32_t length = ldr_u32_read_internal_length();
	
	for( uint32_t i = 0; i < length; i++ ){
		
		crc = crc_u16_byte( crc, pgm_read_byte_far( i ) );
		
		// reset watchdog timer
		wdt_reset();
	}
	
	crc = crc_u16_byte( crc, 0 );
	crc = crc_u16_byte( crc, 0 );
	
	return crc;		
}

// compute CRC of external partition
uint16_t ldr_u16_get_partition_crc( void ){

	uint16_t crc = 0xffff;
	
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
		wdt_reset();
	}

	crc = crc_u16_byte( crc, 0 );
	crc = crc_u16_byte( crc, 0 );
	
	return crc;
}


