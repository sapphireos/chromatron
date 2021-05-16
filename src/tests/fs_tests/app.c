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

#include "ffs_gc.h"	

#define TEST_FILE PSTR("test_file")

static file_t f;
static uint32_t count;
static uint32_t start_time;
static uint32_t elapsed;
static uint32_t idx;

#define TEST_SIZE 16384
static uint8_t test_buf[TEST_SIZE];

static bool done;

static void setup( char *test_name ){

	trace_printf("START %s\r\n", test_name);


	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	fs_v_delete( f );

	f = fs_f_close( f );


	ffs_gc_v_suspend_gc( TRUE );

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );
	
	start_time = tmr_u32_get_system_time_ms();
}

static void test_finished( char *test_name ){

	elapsed = tmr_u32_elapsed_time_ms( start_time );	

	float elapsed_f = (float)elapsed / 1000.0;

	fs_v_seek( f, 0 );

	memset( test_buf, 0, sizeof(test_buf) );
	fs_i16_read( f, test_buf, sizeof(test_buf) );

	for( uint32_t i = 0; i < sizeof(test_buf); i++ ){

		if( test_buf[i] != (uint8_t)i ){

			trace_printf("FAIL: read back error at index: %d -> %d != %d\r\n", i, test_buf[i], i);

			break;
		}
	}

	trace_printf("END %s\r\n", test_name);
	trace_printf("Elapsed: %d ms Bps: %8.0f\r\n", elapsed, TEST_SIZE / elapsed_f );
}

static void clean_up( void ){

	f = fs_f_close( f );
	done = TRUE;

	ffs_gc_v_suspend_gc( FALSE );
}

PT_THREAD( byte_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	setup("byte_append");

	idx = 0;	
	count = TEST_SIZE;

	while( count > 0 ){

		uint8_t byte = idx++;

		int16_t status = fs_i16_write( f, &byte, sizeof(byte) );
		if( status != sizeof(byte) ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		count--;

		if( ( count % 512 ) == 0 ){

			trace_printf("%d\r\n", count);

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("byte_append");

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( page_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	idx = 0;
	
	for( uint32_t i = 0; i < sizeof(test_buf); i++ ){

		test_buf[i] = idx++;
	}

	setup("page_append");
	
	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){
		
		int16_t status = fs_i16_write( f, &test_buf[idx], FFS_PAGE_DATA_SIZE );
		if( status != FFS_PAGE_DATA_SIZE ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		idx += FFS_PAGE_DATA_SIZE;
		count -= FFS_PAGE_DATA_SIZE;;

		if( ( count % 2048 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("page_append");

end:
	clean_up();
	
PT_END( pt );	
}




PT_THREAD( test_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

	TMR_WAIT( pt, 2000 );

	done = FALSE;
	thread_t_create( byte_append,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );

	TMR_WAIT( pt, 2000 );

	done = FALSE;
	thread_t_create( page_append,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );

PT_END( pt );	
}


void app_v_init( void ){


    thread_t_create( test_thread,
                     PSTR("test"),
                     0,
                     0 );
}

