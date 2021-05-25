/*
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
 */


#include "sapphire.h"

#include "superconductor.h"

#ifdef ENABLE_SERVICES

static catbus_hash_t32 banks[SC_MAX_BANKS];

KV_SECTION_META kv_meta_t superconductor_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,   0, KV_FLAGS_PERSIST,  0, 				0, "sc_enable" },
    { SAPPHIRE_TYPE_UINT32, 0, 0,  				  &banks[0], 		0, "sc_bank0" },
    { SAPPHIRE_TYPE_UINT32, 0, 0,  				  &banks[1], 		0, "sc_bank1" },
    { SAPPHIRE_TYPE_UINT32, 0, 0,  				  &banks[2], 		0, "sc_bank2" },
    { SAPPHIRE_TYPE_UINT32, 0, 0,  				  &banks[3], 		0, "sc_bank3" },
};


static socket_t sock;
static uint32_t timer;


static void send_init_msg( void ){

	sc_msg_init_t msg;
	memset( &msg, 0, sizeof(msg) );

	msg.hdr.msg_type = SC_MSG_TYPE_INIT;

	for( uint8_t i = 0; i < SC_MAX_BANKS; i++ ){

		msg.banks[i] = banks[i];
	}

	sock_addr_t raddr = services_a_get( __KV__SuperConductor, 0 );

	sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );
}

PT_THREAD( superconductor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	log_v_info_P( PSTR("SuperConductor server is waiting") );
  
	services_v_listen( __KV__SuperConductor, 0 );

	THREAD_WAIT_WHILE( pt, !services_b_is_available( __KV__SuperConductor, 0 ) );

	// service is available

	// create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    if( sock < 0 ){

		log_v_critical_P( PSTR("socket fail") );

		THREAD_EXIT( pt );    	
    }

    sock_v_set_timeout( sock, 4 );

    log_v_info_P( PSTR("SuperConductor is connected") );

    send_init_msg();
    timer = tmr_u32_get_system_time_ms();

	while( TRUE ){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

		// check if service is gone
		if( !services_b_is_available( __KV__SuperConductor, 0 ) ){

			goto cleanup;
		}

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        if( tmr_u32_elapsed_time_ms( timer ) > SC_SYNC_INTERVAL ){

        	send_init_msg();
        	timer = tmr_u32_get_system_time_ms();
        }

        
	}


cleanup:
	
	sock_v_release( sock );

	sock = -1;

	THREAD_RESTART( pt );

PT_END( pt );
}


#endif

void sc_v_init( void ){

	#ifdef ENABLE_SERVICES

	bool enable = FALSE;

	kv_i8_get( __KV__sc_enable, &enable, sizeof(enable) );

	if( !enable ){

		return;
	}

	thread_t_create( superconductor_thread,
                PSTR("superconductor"),
                0,
                0 );
	#else

	#endif
}

