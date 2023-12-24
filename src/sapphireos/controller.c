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

#include "sapphire.h"

#include "controller.h"

#ifdef ENABLE_CONTROLLER

static socket_t sock;

static bool is_leader;
static bool is_follower;

KV_SECTION_META kv_meta_t controller_kv[] = {
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_leader, 		0,  "controller_is_leader" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_follower, 	0,  "controller_is_follower" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST,   0, 			    0,  "controller_enable_leader" },
};

PT_THREAD( controller_announce_thread( pt_t *pt, void *state ) );
PT_THREAD( controller_server_thread( pt_t *pt, void *state ) );

void controller_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
    }	

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, CONTROLLER_PORT );

    thread_t_create( controller_server_thread,
                     PSTR("controller_server"),
                     0,
                     0 );


    thread_t_create( controller_announce_thread,
                     PSTR("controller_announce"),
                     0,
                     0 );


}

static uint16_t get_priority( void ){

	// check if leader is enabled, if not, return 0 and we are follower only
	if( !kv_b_get_boolean( __KV__controller_enable_leader ) ){

		return 0;
	}

	uint16_t priority = 0;

	// set base priorities

	#ifdef ESP32
	priority = 256;
	#else
	priority = 128;
	#endif

	return priority;
}

static void send_msg( uint8_t msgtype, uint8_t *msg, uint8_t len, sock_addr_t raddr ){

	controller_header_t *header = (controller_header_t *)msg;

	header->magic 		= CONTROLLER_MSG_MAGIC;
	header->version 	= CONTROLLER_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->reserved    = 0;

	sock_i16_sendto( sock, msg, len, &raddr );
}

PT_THREAD( controller_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
    
PT_END( pt );
}


PT_THREAD( controller_announce_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	
    	
PT_END( pt );
}



#endif
