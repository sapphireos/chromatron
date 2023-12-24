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
#include "config.h"

#include "controller.h"

#ifdef ENABLE_CONTROLLER

static socket_t sock;

static bool controller_enabled;
static bool is_leader;
static bool is_follower;
static ip_addr4_t leader_ip;

KV_SECTION_META kv_meta_t controller_kv[] = {
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_leader, 			0,  "controller_is_leader" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_READ_ONLY, &is_follower, 		0,  "controller_is_follower" },
    { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST,   &controller_enabled, 0,  "controller_enable_leader" },
    { CATBUS_TYPE_IPv4, 	0, KV_FLAGS_READ_ONLY, &leader_ip, 			0,  "controller_leader_ip" },
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

static void send_announce( bool drop ){

    uint16_t flags = 0;

    if( drop ){

    	flags |= CONTROLLER_FLAGS_DROP_LEADER;
    }

	controller_msg_announce_t msg = {
		{ 0 },
		flags,
		get_priority(),
		tmr_u64_get_system_time_us(),
		cfg_u64_get_device_id(),
	};

	sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
    raddr.port = CONTROLLER_PORT;

	send_msg( CONTROLLER_MSG_ANNOUNCE, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void process_announce( controller_msg_announce_t *msg ){

	if( msg->flags & CONTROLLER_FLAGS_DROP_LEADER ){

		if( is_follower ){

			is_follower = FALSE;

			log_v_debug_P( PSTR("dropped leader: %d.%d.%d.%d"),
				leader_ip.ip3, 
				leader_ip.ip2, 
				leader_ip.ip1, 
				leader_ip.ip0
			);

			leader_ip = ip_a_addr( 0, 0, 0, 0 );
		}
	}

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

	static uint8_t counter;
	counter = 0;

	// wait until controller is enabled
	THREAD_WAIT_WHILE( pt, !controller_enabled );
	
	send_announce( TRUE ); // drop leader
	TMR_WAIT( pt, rnd_u16_get_int() >> 9 );
	send_announce( TRUE ); // drop leader
	TMR_WAIT( pt, rnd_u16_get_int() >> 9 );
	send_announce( TRUE ); // drop leader

	while( controller_enabled ){

		counter = 0;

		// initial connection loop
		while( !is_leader && !is_follower && controller_enabled && counter < CONTROLLER_LEADER_CYCLES ){

			TMR_WAIT( pt, rnd_u16_get_int() >> 5 );

			send_announce( FALSE );

			counter++;
		}

		THREAD_YIELD( pt );
	}

	// we get here if controller mode is disabled while running
	THREAD_RESTART( pt );
    
PT_END( pt );
}



#endif
