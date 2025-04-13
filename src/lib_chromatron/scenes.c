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
#include "scenes.h"



/*

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

KV_SECTION_META kv_meta_t scene_info_kv[] = {
    { CATBUS_TYPE_STRING32,   0, KV_FLAGS_PERSIST,  current_scene,        0,                  "scene_current" },
};



#define SCENE_BUF_LEN (CATBUS_STRING_LEN * 2 + 8)

static file_t open_scene_file( void ){

	file_t f = fs_f_open_P( PSTR("scenes"), FS_MODE_READ_ONLY );

	return f;
}

static file_t open_scene_file_writable( void ){

	file_t f = fs_f_open_P( PSTR("scenes"), FS_MODE_WRITE_APPEND );

	return f;
}

// search for a scene by name
static int8_t search_scene( file_t f, const char *s ){

	fs_v_seek( f, 0 );

	char buf[SCENE_BUF_LEN];
	memset( buf, 0, sizeof(buf) );

	while( fs_i16_readline( f, buf, sizeof(buf) ) > 0 ){

		log_v_info_P( PSTR("search %s -> %s %d %d"), buf, s, strlen(buf), strlen(s) );

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

static int8_t load_scene( const char *s ){

	if( s[0] == 0 ){

		return -1;
	}

	file_t f = open_scene_file();

	if( f < 0 ){

		return -2;
	}

	if( search_scene( f, s ) < 0 ){

		fs_f_close( f );

		return -3;
	}

	char buf[SCENE_BUF_LEN];
	memset( buf, 0, sizeof(buf) );

	while( fs_i16_readline( f, buf, sizeof(buf) ) > 0 ){

		if( !is_scene_data( buf ) ){

			break;
		}

		log_v_info_P( PSTR("%s"), buf );

		memset( buf, 0, sizeof(buf) );
	}

	fs_f_close( f );

	return 0;
}

void scenes_v_init( void ){

	load_scene( current_scene );
}


