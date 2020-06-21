/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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
#include "election.h"

// #define NO_LOGGING
#include "logging.h"

static socket_t sock;

PT_THREAD( election_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( election_server_thread( pt_t *pt, void *state ) );

typedef struct{
	uint32_t service;
	uint16_t priority;
	uint16_t port;
} election_t;

static list_t elections_list;


void election_v_init( void ){

	#if !defined(ENABLE_NETWORK) || !defined(ENABLE_WIFI)
	return;
	#endif

	list_v_init( &elections_list );

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // create socket
    sock = sock_s_create( SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, ELECTION_PORT );

    sock_v_set_timeout( sock, 1 );

	thread_t_create( election_sender_thread,
                     PSTR("election_sender"),
                     0,
                     0 );

    thread_t_create( election_server_thread,
                     PSTR("election_server"),
                     0,
                     0 );
}

static election_t* get_election( uint32_t service ){

	list_node_t ln = elections_list.head;

	while( ln > 0 ){

		election_t *election = (election_t *)list_vp_get_data( ln );

		if( election->service == service ){

			return election;
		}

		ln = list_ln_next( ln );
	}

	return 0;
}

void election_v_create( uint32_t service, uint16_t priority, uint16_t port ){

	// check if election already exists
	if( get_election( service ) != 0 ){

		return;
	}

	election_t election = {
		service,
		priority,
		port
	};

	list_node_t ln = list_ln_create_node( &election, sizeof(election) );

	if( ln < 0 ){

		return;
	}

	list_v_insert_tail( &elections_list, ln );	
}

void election_v_cancel( uint32_t service ){

	list_node_t ln = elections_list.head;

	while( ln > 0 ){

		list_node_t next_ln = list_ln_next( ln );

		election_t *election = (election_t *)list_vp_get_data( ln );

		if( election->service == service ){

			list_v_remove( &elections_list, ln );
            list_v_release_node( ln );

            return;
		}

		ln = next_ln;
	}
}

static void init_header( election_header_t *header ){

	header->magic 		= ELECTION_MAGIC;
	header->version		= ELECTION_VERSION;
	header->flags 		= 0;
	header->count 		= 0;
	header->padding 	= 0;
	header->device_id   = cfg_u64_get_device_id();
	header->uptime 		= tmr_u64_get_system_time_us();
}


PT_THREAD( election_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		THREAD_WAIT_WHILE( pt, list_u8_count( &elections_list ) == 0 );

		TMR_WAIT( pt, 1000 );

		uint16_t len = sizeof(election_header_t) + 
						( sizeof(election_pkt_t) * list_u8_count( &elections_list ) );

		mem_handle_t h = mem2_h_alloc( len );

		if( h < 0 ){

			continue;
		}

		election_header_t *header = mem2_vp_get_ptr_fast( h );

		init_header( header );

		election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

		list_node_t ln = elections_list.head;

		while( ln > 0 ){

			election_t *election = (election_t *)list_vp_get_data( ln );

			pkt->service 	= election->service;
			pkt->priority 	= election->priority;
			pkt->port 		= election->port;

			pkt++;
			ln = list_ln_next( ln );
		}

		sock_addr_t raddr;
		raddr.ipaddr = ip_a_addr(255,255,255,255);
		raddr.port 	 = ELECTION_PORT;

		sock_i16_sendto_m( sock, h, &raddr );

		THREAD_YIELD( pt );
	}    
    
PT_END( pt );
}

PT_THREAD( election_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

		if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        election_header_t *header = sock_vp_get_data( sock );

        if( header->magic != ELECTION_MAGIC ){

        	continue;
        }

        if( header->version != ELECTION_VERSION ){

        	continue;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

        while( header->count > 0 ){




        	pkt++;
        	header->count--;
        }



		THREAD_YIELD( pt );
	}    
    
PT_END( pt );
}



