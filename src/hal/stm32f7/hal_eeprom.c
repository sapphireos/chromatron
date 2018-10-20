/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "keyvalue.h"

#include "timers.h"
#include "threading.h"

#include "hal_flash25.h"
#include "ffs_eeprom.h"
#include "hal_eeprom.h"


static uint8_t current_block;
static uint32_t total_block_writes;

static bool commit;

static uint8_t ee_data[EE_ARRAY_SIZE];

PT_THREAD( ee_manager_thread( pt_t *pt, void *state ) );

void ee_v_init( void ){

	thread_t_create( ee_manager_thread,
                     PSTR("ee_manager_thread"),
                     0,
                     0 );
}

bool ee_b_busy( void ){

    return 0;
}

// write a byte to eeprom and wait for the completion of the write
void ee_v_write_byte_blocking( uint16_t address, uint8_t data ){

	ASSERT( address < cnt_of_array(ee_data) );

	ee_data[address] = data;    

	commit = TRUE;
}

void ee_v_write_block( uint16_t address, const uint8_t *data, uint16_t len ){

	ASSERT( address < cnt_of_array(ee_data) );
    ASSERT( ( address + len ) < cnt_of_array(ee_data) );

    memcpy( &ee_data[address], data, len );

    commit = TRUE;
}

void ee_v_erase_block( uint16_t address, uint16_t len ){

	ASSERT( address < cnt_of_array(ee_data) );
	ASSERT( ( address + len ) < cnt_of_array(ee_data) );
	   
  	memset( &ee_data[address], 0xff, len );

  	commit = TRUE;
}

uint8_t ee_u8_read_byte( uint16_t address ){

	ASSERT( address < cnt_of_array(ee_data) );

    return ee_data[address];
}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t len ){

	ASSERT( address < cnt_of_array(ee_data) );
	ASSERT( ( address + len ) < cnt_of_array(ee_data) );

	memcpy( data, &ee_data[address], len );
}


static void commit_to_flash( void ){


}


void ee_v_commit( void ){

	if( !commit ){

		return;
	}

	// assume we are rebooting.  if we've asserted on an error,
	// we need to make sure the flash interface is re-initialized
	// in case we were in the middle of an operation.
	hal_flash25_v_init();

	commit_to_flash();
}

PT_THREAD( ee_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

    	THREAD_WAIT_WHILE( pt, !commit );

    	TMR_WAIT( pt, 1000 );
    	commit = FALSE;


    	commit_to_flash();
    }

PT_END( pt );
}


