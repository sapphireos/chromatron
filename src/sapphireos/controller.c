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

static bool controller_enabled;
static bool is_leader;
static bool is_follower;

KV_SECTION_META kv_meta_t controller_kv[] = {
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_leader, 			0,  "controller_is_leader" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_follower, 		0,  "controller_is_follower" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST,   &controller_enabled, 0,  "controller_enable_leader" },
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
	if( !controller_enabled ){

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

static void send_msg( uint8_t msgtype, uint8_t *msg, uint8_t len, sock_addr_t *raddr ){

	controller_header_t *header = (controller_header_t *)msg;

	header->magic 		= CONTROLLER_MSG_MAGIC;
	header->version 	= CONTROLLER_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->reserved    = 0;

	sock_i16_sendto( sock, msg, len, raddr );
}

static void send_announce( void ){

	sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
    raddr.port = CONTROLLER_PORT;

	controller_msg_announce_t msg = {
		{ 0 },
		get_priority(),
		tmr_u64_get_system_time_us(),
		cfg_u64_get_device_id(),
	};

	send_msg( CONTROLLER_MSG_ANNOUNCE, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void process_announce( controller_msg_announce_t *msg ){
	
	
	
}

PT_THREAD( controller_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
   	while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        uint16_t error = CATBUS_STATUS_OK;

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        controller_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != CONTROLLER_MSG_MAGIC ){

            goto end;
        }

        if( header->version != CONTROLLER_MSG_VERSION ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->msg_type == CONTROLLER_MSG_ANNOUNCE ){

        	process_announce( (controller_msg_announce_t *)header );
        }
        else{

        	// invalid message
        	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
        }


    end:

    	THREAD_YIELD( pt );
	}
    
PT_END( pt );
}


PT_THREAD( controller_announce_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	while(1){

		// wait until controller is enabled
		THREAD_WAIT_WHILE( pt, !controller_enabled );

		// initial connection loop
		while( !is_leader && !is_follower && controller_enabled ){

			TMR_WAIT( pt, rnd_u16_get_int() >> 5 );

			send_announce();
		}

		THREAD_YIELD( pt );
	}
    
PT_END( pt );
}



#endif
