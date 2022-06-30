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
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <util/delay.h>

#include "target.h"
#include "boot_data.h"
#include "bootcore.h"

#include "loader.h"

void boot_v_erase_page( uint16_t pagenumber ){
	
	uint32_t addr = ( uint32_t )pagenumber * ( uint32_t )PAGE_SIZE; 
	
	// erase page:
	boot_page_erase(addr);

	boot_spm_busy_wait();
	
	boot_rww_enable();
}

// write a page to flash.  This will NOT pre-erase the page!
void boot_v_write_page( uint16_t pagenumber, uint8_t *data ){

	uint32_t addr = ( uint32_t )pagenumber * ( uint32_t )PAGE_SIZE; 

	// fill page buffer:
	
	for(uint32_t i = addr; i < addr + PAGE_SIZE; i += 2){
		
		uint8_t lsb = *data++;
		uint8_t msb = *data++;
		uint16_t data = (msb << 8) + lsb;
		
		boot_page_fill(i, data);
	}
	
	
	boot_page_write(addr);
	
	boot_spm_busy_wait();
	
	boot_rww_enable();
}

// read specified flash page into destination pointer
void boot_v_read_page( uint16_t pagenumber, uint8_t *dest ){

	uint32_t addr = ( uint32_t )pagenumber * ( uint32_t )PAGE_SIZE; 
	
	for(uint16_t i = 0; i < PAGE_SIZE; i++){
		
		*dest = pgm_read_byte_far(addr);
		
		addr++;
		dest++;
	}	
}

// read data from an offset in flash
void boot_v_read_data( uint32_t offset, uint8_t *dest, uint16_t length ){
	
	for(uint16_t i = 0; i < length; i++){
		
		*dest = pgm_read_byte_far(offset);
		
		offset++;
		dest++;
	}
}

uint16_t boot_u16_get_page_size( void ){
	
	return PAGE_SIZE;
}
