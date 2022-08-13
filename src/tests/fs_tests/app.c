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

#include "sapphire.h"

#include "app.h"

#include "system.h"
#include "ffs_gc.h"	
#include "ffs_page.h"	

#define TEST_FILE PSTR("test_file")

static file_t f;
static uint32_t count;
static uint32_t start_time;
static uint32_t elapsed;
static uint32_t idx;

#define BYTE_TEST_SIZE 4000
#define BYTE_OVERWRITE_TEST_SIZE 2000
#define TEST_SIZE 16384
static uint8_t test_buf[TEST_SIZE];

static bool done;

static void setup( char *test_name ){

	trace_printf("START %s\r\n", test_name);

	ffs_page_v_stats_reset();

	idx = 0;
	
	for( uint32_t i = 0; i < sizeof(test_buf); i++ ){

		test_buf[i] = idx++;
	}

	// ffs_gc_v_suspend_gc( TRUE );

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );
	
	start_time = tmr_u32_get_system_time_ms();
}

static void test_finished( char *test_name, uint32_t test_len, bool read_back ){

	elapsed = tmr_u32_elapsed_time_ms( start_time );	

	float elapsed_f = (float)elapsed / 1000.0;

	if( read_back ){

		fs_v_seek( f, 0 );

		memset( test_buf, 0, test_len );
		fs_i16_read( f, test_buf, test_len );
	}

	for( uint32_t i = 0; i < test_len; i++ ){

		if( test_buf[i] != (uint8_t)i ){

			trace_printf("FAIL: read back error at index: %d -> %d != %d\r\n", i, test_buf[i], i);

			break;
		}
	}

	trace_printf("END %s\r\n", test_name);
	trace_printf("Elapsed: %d ms Bps: %8.0f\r\n", elapsed, TEST_SIZE / elapsed_f );
	trace_printf("Block copies: %d  Page allocs: %d Cache hits: %d misses: %d\r\n",
		ffs_page_u32_get_block_copies(),
		ffs_page_u32_get_page_allocs(),
		ffs_page_u32_get_cache_hits(),
		ffs_page_u32_get_cache_misses() );		
}

static void clean_up( void ){

	f = fs_f_close( f );
	done = TRUE;


	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	fs_v_delete( f );

	f = fs_f_close( f );

	// ffs_gc_v_suspend_gc( FALSE );
}

PT_THREAD( byte_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	setup("byte_append");

	idx = 0;	
	count = BYTE_TEST_SIZE;

	while( count > 0 ){

		uint8_t byte = idx++;

		int16_t status = fs_i16_write( f, &byte, sizeof(byte) );
		if( status != sizeof(byte) ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		count--;

		if( ( count % 512 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("byte_append", BYTE_TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( page_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	setup("page_append");
	
	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){
		
		int16_t status = fs_i16_write( f, &test_buf[idx], FFS_PAGE_DATA_SIZE );
		if( status != FFS_PAGE_DATA_SIZE ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += FFS_PAGE_DATA_SIZE;
		count -= FFS_PAGE_DATA_SIZE;;

		if( ( count % 512 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("page_append", TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}

PT_THREAD( page_plus_one_byte_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );
	
	#define DATA_LEN ( FFS_PAGE_DATA_SIZE + 1 )

	setup("page_plus_one_byte_append");
	
	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = DATA_LEN;
		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_write( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		THREAD_YIELD( pt );
	}

	test_finished("page_plus_one_byte_append", TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( large_write( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );
	
	setup("large_write");
	
	count = TEST_SIZE;
	uint32_t len = count;
	
	int16_t status = fs_i16_write( f, test_buf, len );
	if( status != len ){

		trace_printf("FAIL: %d\r\n", status);

		goto end;
	}

	sys_v_wdt_reset();

	test_finished("large_write", TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( random_len_append( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );
	
	#define DATA_LEN ( FFS_PAGE_DATA_SIZE + 1 )

	setup("random_len_append");
	
	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = rnd_u8_get_int();

		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_write( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		THREAD_YIELD( pt );
	}

	test_finished("random_len_append", TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}



PT_THREAD( random_len_overwrite( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	memset( test_buf, 0, sizeof(test_buf) );


	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	int16_t status = fs_i16_write( f, test_buf, sizeof(test_buf) );
	if( status != sizeof(test_buf) ){

		trace_printf("FAIL: %d\r\n", status);

		goto end;
	}

	f = fs_f_close( f );

	TMR_WAIT( pt, 2000 );
	
	#define DATA_LEN ( FFS_PAGE_DATA_SIZE + 1 )

	setup("random_len_overwrite");
	
	fs_v_seek( f, 0 );

	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = rnd_u8_get_int();

		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_write( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		THREAD_YIELD( pt );
	}

	test_finished("random_len_overwrite", TEST_SIZE, TRUE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( single_byte_overwrite( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	memset( test_buf, 0, sizeof(test_buf) );


	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	int16_t status = fs_i16_write( f, test_buf, sizeof(test_buf) );
	if( status != sizeof(test_buf) ){

		trace_printf("FAIL: %d\r\n", status);

		goto end;
	}

	f = fs_f_close( f );

	TMR_WAIT( pt, 2000 );
	
	setup("single_byte_overwrite");
	
	fs_v_seek( f, 0 );

	count = BYTE_OVERWRITE_TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = 1;

		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_write( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d len: %d\r\n", status, len);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		if( ( count % 512 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("single_byte_overwrite", BYTE_OVERWRITE_TEST_SIZE, FALSE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( contiguous_read_single_byte( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	for( uint32_t i = 0; i < sizeof(test_buf); i++ ){

		test_buf[i] = (uint8_t)i;
	}

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	int16_t status = fs_i16_write( f, test_buf, sizeof(test_buf) );
	if( status != sizeof(test_buf) ){

		trace_printf("FAIL: %d\r\n", status);

		goto end;
	}

	f = fs_f_close( f );

	TMR_WAIT( pt, 2000 );
	
	setup("contiguous_read_single_byte");
	
	fs_v_seek( f, 0 );

	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = 1;

		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_read( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		if( ( count % 512 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("contiguous_read_single_byte", TEST_SIZE, FALSE);

end:
	clean_up();
	
PT_END( pt );	
}


PT_THREAD( contiguous_read_single_page( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	for( uint32_t i = 0; i < sizeof(test_buf); i++ ){

		test_buf[i] = (uint8_t)i;
	}

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	int16_t status = fs_i16_write( f, test_buf, sizeof(test_buf) );
	if( status != sizeof(test_buf) ){

		trace_printf("FAIL: %d\r\n", status);

		goto end;
	}

	f = fs_f_close( f );

	TMR_WAIT( pt, 2000 );
	
	setup("contiguous_read_single_page");
	
	fs_v_seek( f, 0 );

	count = TEST_SIZE;
	idx = 0;

	while( count > 0 ){

		uint16_t len = FFS_PAGE_DATA_SIZE;

		if( len > count ){
			len = count;
		}
		
		int16_t status = fs_i16_read( f, &test_buf[idx], len );
		if( status != len ){

			trace_printf("FAIL: %d\r\n", status);

			goto end;
		}

		sys_v_wdt_reset();

		idx += len;
		count -= len;

		if( ( count % 512 ) == 0 ){

			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
			THREAD_YIELD( pt );
		}
	}

	test_finished("contiguous_read_single_page", TEST_SIZE, FALSE);

end:
	clean_up();
	
PT_END( pt );	
}



PT_THREAD( test_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	f = fs_f_open_P( TEST_FILE, 
			  		 FS_MODE_CREATE_IF_NOT_FOUND |
                      FS_MODE_WRITE_APPEND );

	fs_v_delete( f );

	f = fs_f_close( f );


	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

	trace_printf("FS tests START\r\n");



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



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( page_plus_one_byte_append,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );




	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( large_write,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( random_len_append,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( random_len_overwrite,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( single_byte_overwrite,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( contiguous_read_single_byte,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );



	TMR_WAIT( pt, 2000 );
	done = FALSE;
	thread_t_create( contiguous_read_single_page,
                     PSTR("test"),
                     0,
                     0 );
	
	THREAD_WAIT_WHILE( pt, !done );





	trace_printf("FS tests END\r\n");

PT_END( pt );	
}


void app_v_init( void ){


    thread_t_create( test_thread,
                     PSTR("test"),
                     0,
                     0 );
}

