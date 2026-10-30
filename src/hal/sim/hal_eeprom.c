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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*

EEPROM (sim)

File-backed implementation: the eeprom is mapped from sim_eeprom.bin
(in the current directory) and persists across reboots, the same way
real eeprom does. A brand new file is pre-erased to 0xFF like a fresh
chip; an existing file keeps its contents.

*/

#define SIM_EEPROM_FILE "sim_eeprom.bin"

static uint8_t *array;

void ee_v_init( void ){

    int fd = open( SIM_EEPROM_FILE, O_RDWR | O_CREAT, 0644 );

    if( fd < 0 ){

        fprintf( stderr, "sim: failed to open %s\n", SIM_EEPROM_FILE );
        exit( 1 );
    }

    // a brand new file is all zeros; remember so we can pre-erase it
    // like a fresh chip. an existing file keeps its contents.
    struct stat st;
    int fresh = ( fstat( fd, &st ) == 0 && st.st_size == 0 );

    if( ftruncate( fd, EE_ARRAY_SIZE ) < 0 ){

        fprintf( stderr, "sim: failed to size %s\n", SIM_EEPROM_FILE );
        exit( 1 );
    }

    array = ( uint8_t *)mmap( 0, EE_ARRAY_SIZE,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0 );

    close( fd );

    if( array == MAP_FAILED ){

        fprintf( stderr, "sim: failed to map %s\n", SIM_EEPROM_FILE );
        exit( 1 );
    }

    // fresh eeprom reads as erased (0xFF) like real hardware
    if( fresh ){

        memset( array, 0xff, EE_ARRAY_SIZE );
    }
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

void ee_v_commit( void ){

}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t length ){

    // busy wait
    while( ee_b_busy() )
    {
    }

	for( uint16_t i = 0; i < length; i++ ){

        *data = array[address];

		data++;
		address++;
	}
}
