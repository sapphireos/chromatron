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

#include "keyvalue.h"

#include "timers.h"
#include "threading.h"

#include "ffs_global.h"
#include "flash25.h"
#include "hal_flash25.h"
#include "ffs_eeprom.h"
#include "hal_eeprom.h"


#include "logging.h"


#define N_BLOCKS FLASH_FS_EEPROM_N_BLOCKS

static uint8_t current_block;
static uint32_t total_block_writes;

KV_SECTION_META kv_meta_t hal_eeprom_kv[] = {
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY, &total_block_writes, 0,  "eeprom_block_writes" },
};


static bool commit;

static uint8_t ee_data[EE_ARRAY_SIZE] __attribute__((aligned(4)));


#define STATUS_ERASED 			0xffffffff
#define STATUS_PARTIAL_WRITE 	0xef
#define STATUS_VALID 			0x0f
#define STATUS_DIRTY 			0x00


typedef struct __attribute__((packed, aligned(4))){
    uint32_t status;
    uint32_t total_writes;
    uint32_t reserved[2];
} block_header_t;


PT_THREAD( ee_manager_thread( pt_t *pt, void *state ) );


static void set_block_status( uint8_t block, uint32_t status ){

	ffs_eeprom_v_write( block, 0, (uint8_t *)&status, sizeof(status) );
}


void ee_v_init( void ){

	trace_printf("EE init...\r\n");

	memset( ee_data, 0xff, sizeof(ee_data) );

	if( flash25_u32_capacity() == 0 ){

		trace_printf("Flash EE fail\r\n");

		return;
	}

	block_header_t header;

	// scan for first valid block
	for( uint8_t i = 0; i < N_BLOCKS; i++ ){

		// read block header
		ffs_eeprom_v_read( i, 0, (uint8_t *)&header, sizeof(header) );

		// check status
		if( header.status == STATUS_VALID ){

			current_block = i;
			total_block_writes = header.total_writes;
		}
	}

	// clean up any other blocks
	for( uint8_t i = 0; i < N_BLOCKS; i++ ){

		// read block header
		ffs_eeprom_v_read( i, 0, (uint8_t *)&header, sizeof(header) );

		// check status
		if( ( header.status != STATUS_ERASED ) && ( i != current_block ) ){

			trace_printf("Cleaning block: %d\r\n", i);

			// erase block
			ffs_eeprom_v_erase( i );
		}
	}

	ASSERT( current_block < N_BLOCKS );

	// check current block for any errors (or if it needs to be initialized)
	ffs_eeprom_v_read( current_block, 0, (uint8_t *)&header, sizeof(header) );

	if( header.status == STATUS_VALID ){

		trace_printf("Loading EEPROM data. Block: %d\r\n", current_block);
			
		// load eeprom contents
		ffs_eeprom_v_read( current_block, sizeof(header), ee_data, sizeof(ee_data) );
	}
	else{

		trace_printf("Initializing EEPROM data. Block: %d\r\n", current_block);

		// erase and initialize
		ffs_eeprom_v_erase( current_block );

		memset( &header, 0xff, sizeof(header) );
		header.total_writes = 0;
		total_block_writes = 0;

		ffs_eeprom_v_write( current_block, 0, (uint8_t *)&header, sizeof(header) );
		set_block_status( current_block, STATUS_VALID );
	}


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
    ASSERT( ( address + len ) <= cnt_of_array(ee_data) );

    memcpy( &ee_data[address], data, len );

    commit = TRUE;
}

void ee_v_erase_block( uint16_t address, uint16_t len ){

	ASSERT( address < cnt_of_array(ee_data) );
	ASSERT( ( address + len ) <= cnt_of_array(ee_data) );
	   
  	memset( &ee_data[address], 0xff, len );

  	commit = TRUE;
}

uint8_t ee_u8_read_byte( uint16_t address ){

	ASSERT( address < cnt_of_array(ee_data) );

    return ee_data[address];
}

void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t len ){

	ASSERT( address < cnt_of_array(ee_data) );
	ASSERT( ( address + len ) <= cnt_of_array(ee_data) );

	memcpy( data, &ee_data[address], len );
}


static void commit_to_flash( void ){

	// get next block
	uint8_t next_block = current_block + 1;

	if( next_block >= N_BLOCKS ){

		next_block = 0;
	}

	trace_printf("EE commit: %d\r\n", next_block);
	
	// erase next block
	ffs_eeprom_v_erase( next_block );

	total_block_writes++;

	block_header_t header;

	// read header
	ffs_eeprom_v_read( current_block, 0, (uint8_t *)&header, sizeof(header) );
	header.total_writes = total_block_writes;
	header.status = STATUS_PARTIAL_WRITE;
	
	// write header and data to new block
	ffs_eeprom_v_write( next_block, 0, (uint8_t *)&header, sizeof(header) );
	ffs_eeprom_v_write( next_block, sizeof(header), ee_data, sizeof(ee_data) );	

	// set status on new block to valid
	set_block_status( next_block, STATUS_VALID );

	// set old block to dirty
	set_block_status( current_block, STATUS_DIRTY );

	// switch current block
	current_block = next_block;
}


void ee_v_commit( void ){

	if( !commit ){

		return;
	}

	#ifndef ENABLE_COPROCESSOR // this will break with the coprocessor

	// assume we are rebooting.  if we've asserted on an error,
	// we need to make sure the flash interface is re-initialized
	// in case we were in the middle of an operation.
	hal_flash25_v_init();
	#endif

	commit_to_flash();
}

PT_THREAD( ee_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    log_v_debug_P( PSTR("EE block: %u total_writes: %lu"), current_block, total_block_writes );

    while(1){

    	THREAD_WAIT_WHILE( pt, !commit );

    	TMR_WAIT( pt, 1000 );
    	commit = FALSE;

    	commit_to_flash();

    	// log_v_debug_P( PSTR("EE commit block %u total_writes: %lu"), current_block, total_block_writes );
    }

PT_END( pt );
}


