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

#include <avr/eeprom.h>

void ee_v_init( void ){

}

bool ee_b_busy( void ){

    return !eeprom_is_ready();
}

// write a byte to eeprom and wait for the completion of the write
void ee_v_write_byte_blocking( uint16_t address, uint8_t data ){

    eeprom_write_byte( (uint8_t *)address, data );
}

void ee_v_write_block( uint16_t address, const uint8_t *data, uint16_t len ){

    eeprom_write_block( (const void *)data, (void *)address, len );
}

void ee_v_erase_block( uint16_t address, uint16_t len ){

    while( len > 0 ){

        ee_v_write_byte_blocking( address, 0xff );

        len--;
        address++;

        sys_v_wdt_reset();
    }
}

uint8_t ee_u8_read_byte( uint16_t address ){

    return eeprom_read_byte( (uint8_t *)address );
}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t len ){

    eeprom_read_block( (void *)data, (const void *)address, len );
}


void ee_v_commit( void ){

}
