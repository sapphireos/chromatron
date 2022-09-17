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

static uint32_t packets_sent;
static uint32_t packets_received;
static uint32_t max_loss_streak;

KV_SECTION_META kv_meta_t app_kv[] = {
    { CATBUS_TYPE_UINT32,     0, 0, &packets_sent,  		0, "packets_sent" },
    { CATBUS_TYPE_UINT32,     0, 0, &packets_received,  	0, "packets_received" },
    { CATBUS_TYPE_UINT32,     0, 0, &max_loss_streak,  	0, "packets_lost_streak" },
};

PT_THREAD( client_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
	
	static socket_t sock;
	sock = sock_s_create( SOS_SOCK_DGRAM );

	TMR_WAIT( pt, 10000 );
		
	while(1){

		TMR_WAIT( pt, 200 );

		uint8_t buf[UDP_MAX_LEN];
		memset( buf, 0, sizeof(buf) );

		uint32_t *ptr = (uint32_t *)buf;
		*ptr = packets_sent + 1;

		sock_addr_t raddr;
		raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
		raddr.port = TEST_PORT;

		if( sock_i16_sendto( sock, buf, sizeof(buf), &raddr ) >= 0 ){

			packets_sent++;	
		}
	}

PT_END( pt );	
}


PT_THREAD( server_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

	static socket_t sock;
	sock = sock_s_create( SOS_SOCK_DGRAM );
	sock_v_bind( sock, TEST_PORT );

	static uint32_t last_rx;
	last_rx = 1;
	
	while(1){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

		packets_received++;

		uint32_t *ptr = sock_vp_get_data( sock );

		if( *ptr > last_rx ){

			uint32_t delta = *ptr - last_rx;

			if( delta > max_loss_streak ){

				max_loss_streak = delta;				
			}
		}

		last_rx = *ptr;
		
		THREAD_YIELD( pt );
	}

PT_END( pt );	
}


void app_v_init( void ){

	thread_t_create( server_thread,
                 PSTR("udp_server"),
                 0,
                 0 );	

	thread_t_create( client_thread,
                 PSTR("udp_client"),
                 0,
                 0 );	
}

