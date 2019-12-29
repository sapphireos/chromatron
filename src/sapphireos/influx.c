// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "config.h"
#include "influx.h"

static socket_t sock;

void influx_v_init( void ){

	sock = sock_s_create( SOCK_DGRAM );
}

// myMeasurement,tag1=value1,tag2=value2 fieldKey="fieldValue" 1241543ms


void influx_v_transmit( catbus_hash_t32 hash ){

	// check if hash should be transmitted


	#define BUF_SIZE 256

	kv_meta_t meta;

	// get meta of measurement
	if( kv_i8_lookup_hash_with_name( hash, &meta ) < 0 ){

		return;
	}

	// allocate a data buffer
	mem_handle_t h = mem2_h_alloc( BUF_SIZE );

	if( h < 0 ){

		return;
	}

	char *ptr = mem2_vp_get_ptr_fast( h );
	uint16_t len = 0;
	char *str = meta.name;


	len += strlcpy( &ptr[len], str, BUF_SIZE - len );
	len += strlcpy_P( &ptr[len], PSTR(",name="), BUF_SIZE - len );

	cfg_i8_get( __KV__meta_tag_name, str );	
	len += strlcpy( &ptr[len], str, BUF_SIZE - len );
	len += strlcpy_P( &ptr[len], PSTR(",location="), BUF_SIZE - len );

	cfg_i8_get( __KV__meta_tag_location, str );
	len += strlcpy( &ptr[len], str, BUF_SIZE - len );

	len += strlcpy_P( &ptr[len], PSTR(" value="), BUF_SIZE - len );

	uint8_t value[CATBUS_STRING_LEN];
	kv_i8_get( hash, value, sizeof(value) );

	// convert type

	// NOTE! string to numeric conversion is not yet implemented!

	type_i8_convert( CATBUS_TYPE_STRING32, str, meta.type, value, sizeof(value) );

	// append convered value
	len += strlcpy( &ptr[len], str, BUF_SIZE - len );
	// end with newline and null terminate
	ptr[len] = '\n';
	len++;
	ptr[len] = 0;
	len++;

	ASSERT( len <= BUF_SIZE );

	log_v_debug_P( PSTR("%s"), ptr );

	if( mem2_i8_realloc( h, len ) < 0 ){

		mem2_v_free( h );
		return;
	}

	sock_addr_t raddr;
	raddr.ipaddr = ip_a_addr(10,0,0,100);
	raddr.port = INFLUX_DEFAULT_UDP_PORT;

	sock_i16_sendto_m( sock, h, &raddr );
}


