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


#include "cpu.h"

#include "system.h"
#include "target.h"

#include "hal_eeprom.h"

#ifdef __SIM__
static uint8_t array[EE_ARRAY_SIZE];
#endif

void ee_v_init( void ){

}

bool ee_b_busy( void ){

    return FALSE;
}

// write a byte to eeprom and wait for the completion of the write
void ee_v_write_byte_blocking( uint16_t address, uint8_t data ){

    array[address] = data;
}

void ee_v_write_block( uint16_t address, const uint8_t *data, uint16_t len ){

    while( len > 0 ){

        ee_v_write_byte_blocking( address, *data );

        data++;
        len--;
        address++;

        wdt_reset();
    }
}

void ee_v_erase_block( uint16_t address, uint16_t len ){

    while( len > 0 ){

        ee_v_write_byte_blocking( address, 0xff );

        len--;
        address++;

        wdt_reset();
    }
}

uint8_t ee_u8_read_byte( uint16_t address ){

    return array[address];
}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t length ){

    // busy wait
    while( ee_b_busy() );

	for( uint16_t i = 0; i < length; i++ ){

        *data = array[address];

		data++;
		address++;
	}
}
