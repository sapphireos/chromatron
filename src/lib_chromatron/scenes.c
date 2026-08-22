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


#include <stdlib.h>

#include "fs.h"
#include "sapphire.h"
#include "scenes.h"



/*

Scene manager





Scenes file:

plain text, line separated



scene_name
key value
key value
key value
scene_name
key value
key value

etc


Scene KV items:

if gfx_enable is false:
	We only record gfx_enable.

else:
	Record gfx items:

	gfx_master_dimmer
	gfx_sub_dimmer
	

if gfx_enable is true and sequencer is enabled (seq_time_mode > 0):
	Record sequencer keys:
	seq_slots
	seq_time_mode
	seq_select_mode
	set_interval_time
	seq_random_time_min
	seq_random_time_max

if sequencer is not enabled and vm_run is true:
	Record vm_prog, vm_run

*/


static char current_scene[CATBUS_STRING_LEN];


static int8_t load_scene( const char *s );
static int8_t save_scene( const char *s );

int8_t _scene_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

    	if( hash == __KV__scene_save_as ){

    		memset( data, 0, len );
    	}
    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__scene_current ){

         	load_scene( current_scene );   
        }
        else if( hash == __KV__scene_save_as ){

        	save_scene( data );
        }
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

KV_SECTION_META kv_meta_t scene_info_kv[] = {
    { CATBUS_TYPE_STRING32,   0, KV_FLAGS_PERSIST,  current_scene,        _scene_kv_handler,                  "scene_current" },
    { CATBUS_TYPE_STRING32,   0, 0,  				0,        			  _scene_kv_handler,                  "scene_save_as" },
};



#define SCENE_BUF_LEN (CATBUS_STRING_LEN * 2 + 8)

static file_t open_scene_file( void ){

	file_t f = fs_f_open_P( PSTR("scenes"), FS_MODE_READ_ONLY );

	return f;
}

// search for a scene by name
static int8_t search_scene( file_t f, const char *s ){

	fs_v_seek( f, 0 );

	char buf[SCENE_BUF_LEN];
	memset( buf, 0, sizeof(buf) );

	while( fs_i16_readline( f, buf, sizeof(buf) ) > 0 ){

		// log_v_info_P( PSTR("search %s -> %s %d %d"), buf, s, strlen(buf), strlen(s) );

		if( strncmp( buf, s, sizeof(buf) ) == 0 ){

			// match!

			return 0;
		}

		memset( buf, 0, sizeof(buf) );
	}

	return -1;
}

static bool is_scene_data( const char *s ){

	for( uint8_t i = 0; i < strlen(s); i++ ){

		if( s[i] == ' ' ){

			return TRUE;
		}
	}

	return FALSE;
}

static bool is_whitespace( char c ){

	return (c == ' ') || (c == '\t');
}

static int8_t load_scene( const char *s ){

	if( s[0] == 0 ){

		return -1;
	}

	file_t f = open_scene_file();

	if( f < 0 ){

		log_v_info_P( PSTR("scenes file not found") );

		return -2;
	}

	if( search_scene( f, s ) < 0 ){

		fs_f_close( f );

		log_v_info_P( PSTR("Scene %s not found"), s );

		return -3;
	}

	char buf[SCENE_BUF_LEN];
	memset( buf, 0, sizeof(buf) );

	while( fs_i16_readline( f, buf, sizeof(buf) ) > 0 ){

		// we got to a scene header, done parsing this scene
		if( !is_scene_data( buf ) ){

			break;
		}

		// log_v_info_P( PSTR("%s"), buf );

		// parse key and value
		char *key = buf;

		// string whitespace from prefix of key
		while( is_whitespace( *key ) ){

			key++;
		}

		char *value = buf;

		for( uint8_t i = 0; i < strlen(buf) - 1; i++ ){

			if( buf[i] == ' ' ){

				buf[i] = 0; // replace space with null term
				// set value to next character
				value = &buf[i + 1];
			}
		}

		// log_v_info_P( PSTR("%s = %s"), key, value );

		catbus_hash_t32 key_hash = hash_u32_string( key );

		// get value type for this key
		catbus_type_t8 val_type = kv_i8_type( key_hash );

		if( val_type < 0 ){

			// key not found
			log_v_info_P( PSTR("Key %s not found"), key );
			
			goto next;
		}

		if( type_b_is_string( val_type ) ){

			// apply string value
			char catbus_str[CATBUS_STRING_LEN];
			memset( catbus_str, 0, sizeof(catbus_str) );
			strncpy( catbus_str, value, sizeof(catbus_str) );

			int8_t status = kv_i8_set( key_hash, catbus_str, sizeof(catbus_str) );

			if( status < 0 ){

				log_v_error_P( PSTR("%d"), status );
			}
		}
		else{

			// convert to integer
			int32_t val_int = atoi( value );

			// log_v_info_P( PSTR("int %d"), val_int );

			int8_t status = kv_i8_set( key_hash, &val_int, sizeof(val_int) );

			if( status < 0 ){

				log_v_error_P( PSTR("%d"), status );
			}
		}


next:
		memset( buf, 0, sizeof(buf) );
	}

	fs_f_close( f );

	return 0;
}

static void write_scene_key_str( file_t f, const char *key, const char *value ){

	char tab = '\t';
	fs_i16_write( f, &tab, sizeof(tab) );	
	fs_i16_write( f, key, strlen(key) );
	char space = ' ';
	fs_i16_write( f, &space, sizeof(space) );	
	fs_i16_write( f, value, strlen(value) );
	char newline = '\n';
	fs_i16_write( f, &newline, sizeof(newline) );
}

static void write_scene_key_int( file_t f, const char *key, int32_t value ){

	char buf[32] = {0};
	snprintf_P( buf, sizeof(buf), PSTR("%d"), value );
	
	write_scene_key_str( f, key, buf );
}


static int8_t save_scene( const char *s ){

	file_t current_f = open_scene_file();

	// check if scenes file does not exist
	if( current_f < 0 ){

		current_f = fs_f_open_P( PSTR("scenes"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

		if( current_f < 0 ){

			return -1;
		}
	}

	file_t new_f = fs_f_open_P( PSTR("_scenes"), FS_MODE_READ_ONLY );	

	// erase temp file
	if( new_f > 0 ){

		fs_v_delete( new_f );
		fs_f_close( new_f );
		new_f = -1;
	}

	new_f = fs_f_open_P( PSTR("_scenes"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

	if( new_f < 0 ){

		fs_f_close( current_f );

		return -2;
	}


	// copy file until scene is reached

	char nl = '\n';
	char buf[SCENE_BUF_LEN];
	memset( buf, 0, sizeof(buf) );

	while( fs_i16_readline( current_f, buf, sizeof(buf) ) > 0 ){

		if( strncmp( buf, s, sizeof(buf) ) == 0 ){

			break;
		}

		fs_i16_write( new_f, buf, strlen(buf) );
		fs_i16_write( new_f, &nl, sizeof(nl) );

		memset( buf, 0, sizeof(buf) );
	}

	// write new scene
	fs_i16_write( new_f, s, strlen(s) );	
	fs_i16_write( new_f, &nl, sizeof(nl) );


	// write scene data
	bool gfx_enable = FALSE;
	kv_i8_get( __KV__gfx_enable, &gfx_enable, sizeof(gfx_enable) );

	write_scene_key_int( new_f, PSTR("gfx_enable"), gfx_enable );

	if( !gfx_enable ){

		goto done;
	}

	bool vm_run = FALSE;
	kv_i8_get( __KV__vm_run, &vm_run, sizeof(vm_run) );

	write_scene_key_int( new_f, PSTR("vm_run"), vm_run );

	if( !vm_run ){

		goto done;
	}

	uint16_t dimmer = 0;
	kv_i8_get( __KV__gfx_master_dimmer, &dimmer, sizeof(dimmer) );
	write_scene_key_int( new_f, PSTR("gfx_master_dimmer"), dimmer );

	kv_i8_get( __KV__gfx_sub_dimmer, &dimmer, sizeof(dimmer) );
	write_scene_key_int( new_f, PSTR("gfx_sub_dimmer"), dimmer );

	
	uint8_t seq_time_mode = 0;
	kv_i8_get( __KV__seq_time_mode, &seq_time_mode, sizeof(seq_time_mode) );

	write_scene_key_int( new_f, PSTR("seq_time_mode"), seq_time_mode );

	if( seq_time_mode == 0 ){

		// write vm_prog
		char vm_prog[CATBUS_STRING_LEN] = {0};
		kv_i8_get( __KV__vm_prog, &vm_prog, sizeof(vm_prog) );

		write_scene_key_str( new_f, PSTR("vm_prog"), vm_prog );

		goto done;
	}
	else{

		// write sequencer settings

		uint8_t seq_select_mode = 0;
		kv_i8_get( __KV__seq_select_mode, &seq_select_mode, sizeof(seq_select_mode) );

		write_scene_key_int( new_f, PSTR("seq_select_mode"), seq_select_mode );


		char seq_prog[CATBUS_STRING_LEN] = {0};

		kv_i8_get( __KV__seq_slot_0, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_0"), seq_prog );	
		}
		
		kv_i8_get( __KV__seq_slot_1, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_1"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_2, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_2"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_3, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_3"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_4, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_4"), seq_prog );	
		}
		
		kv_i8_get( __KV__seq_slot_5, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_5"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_6, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_6"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_7, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_7"), seq_prog );	
		}


		kv_i8_get( __KV__seq_slot_charging, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_charging"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_startup, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_startup"), seq_prog );	
		}

		kv_i8_get( __KV__seq_slot_shutdown, &seq_prog, sizeof(seq_prog) );
		if( seq_prog[0] != 0 ){

			write_scene_key_str( new_f, PSTR("seq_slot_shutdown"), seq_prog );	
		}
	}


done:

	// warp to next scene
	while( fs_i16_readline( current_f, buf, sizeof(buf) ) > 0 ){

		if( !is_scene_data( buf ) ){

			// rewind
			fs_v_seek( current_f, fs_i32_tell( current_f ) - strlen( buf ) );
			break;
		}

		memset( buf, 0, sizeof(buf) );
	}

	// copy rest of file	
	int16_t bytes_read = fs_i16_readline( current_f, buf, sizeof(buf) );

	while( bytes_read > 0 ){

		if( strncmp( buf, s, sizeof(buf) ) == 0 ){

			break;
		}

		fs_i16_write( new_f, buf, strlen(buf) );
		fs_i16_write( new_f, &nl, sizeof(nl) );

		memset( buf, 0, sizeof(buf) );

		bytes_read = fs_i16_readline( current_f, buf, sizeof(buf) );
	}

	// delete scenes file
	fs_v_delete( current_f );
	fs_f_close( current_f );

	// recreate file
	current_f = fs_f_open_P( PSTR("scenes"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );	

	if( current_f < 0 ){

		fs_f_close( new_f );
		return -1;
	}

	// copy temp file to scenes
	fs_v_seek( new_f, 0 );

	bytes_read = fs_i16_read( new_f, buf, sizeof(buf) );

	while( bytes_read > 0 ){

		fs_i16_write( current_f, buf, bytes_read );

		bytes_read = fs_i16_read( new_f, buf, sizeof(buf) );
	}

	// delete temp file
	fs_v_delete( new_f );
	fs_f_close( new_f );
	fs_f_close( current_f );


	return 0;
}

void scenes_v_init( void ){

	load_scene( current_scene );
}


