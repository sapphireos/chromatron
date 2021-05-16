// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "sapphire.h"

#include "app.h"
	

#define TEST_FILE PSTR("test_file")

static file_t f;
static uint32_t count;
static uint32_t start_time;
static uint32_t elapsed;

#define TEST_SIZE 100000


PT_THREAD( byte_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

	trace_printf("START byte_append\r\n");
	
	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );
	
	count = TEST_SIZE;
	start_time = tmr_u32_get_system_time_ms();

	while( count > 0 ){

		uint8_t byte = 0x43;

		int16_t status = fs_i16_write( f, &byte, sizeof(byte) );
		if( status != sizeof(byte) ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		count--;
	}

	elapsed = tmr_u32_elapsed_time_ms( start_time );

	trace_printf("END byte_append\r\n");
	trace_printf("Elapsed: %d ms Bps: %d\r\n", elapsed, ( TEST_SIZE / elapsed ) * 10000 );


end:
	f = fs_f_close( f );
	
PT_END( pt );	
}


void app_v_init( void ){

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	fs_v_delete( f );

	f = fs_f_close( f );

    thread_t_create( byte_append,
                     PSTR("app"),
                     0,
                     0 );
}

