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

#include "hal_nvm.h"
#include "target.h"
#include "boot_data.h"

void boot_v_init( void ){

    nvm_v_init();
}

// write a page to flash.  This will NOT pre-erase the page!
void boot_v_write_page( uint16_t pagenumber, uint8_t *data ){

	uint32_t addr = ( uint32_t )pagenumber * ( uint32_t )PAGE_SIZE;

    nvm_v_write_flash_page( addr, data );

    while( nvm_b_busy() );
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
