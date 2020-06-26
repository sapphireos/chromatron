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


typedef enum{
    STATE_IDLE          = 0,
    STATE_CANDIDATE,
    STATE_FOLLOWER,
    STATE_LEADER,
} election_state_t;


typedef struct{
    uint32_t service;
    uint32_t group;

    // our priority and port
    uint16_t priority;
    uint16_t port;

    // current leader information
    uint64_t leader_uptime;
    uint64_t leader_device_id;
    uint16_t leader_priority;
    uint16_t leader_port;   
    ip_addr4_t leader_ip;
    uint16_t leader_timeout; // if 0, leader has timed out or is not confirmed.

    election_state_t state;
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

    thread_t_create( election_server_thread,
                     PSTR("election_server"),
                     0,
                     0 );
}

static uint8_t elections_count( void ){

    return list_u8_count( &elections_list );
}

// elections for which we can become leader
static uint8_t leader_elections_count( void ){

    uint8_t count = 0;

    list_node_t ln = elections_list.head;

    while( ln > 0 ){

        election_t *election = (election_t *)list_vp_get_data( ln );

        if( election->priority != ELECTION_PRIORITY_CANDIDATE_ONLY ){

            count++;
        }

        ln = list_ln_next( ln );
    }

    return count;
}

// elections for which we have not elected a leader
static uint8_t campaigns_count( void ){

    uint8_t count = 0;

    list_node_t ln = elections_list.head;

    while( ln > 0 ){

        election_t *election = (election_t *)list_vp_get_data( ln );

        if( ( election->state == STATE_IDLE ) ||
            ( election->state == STATE_CANDIDATE ) ){

            count++;
        }

        ln = list_ln_next( ln );
    }

    return count;
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

void election_v_join( uint32_t service, uint32_t group, uint16_t priority, uint16_t port ){

    // check if election already exists
    if( get_election( service ) != 0 ){

        return;
    }

    election_t election = {0};
    election.service    = service;
    election.group      = group;
    election.priority   = priority;
    election.port       = port;
    election.state      = STATE_IDLE;

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

bool election_b_leader_found( uint32_t service ){

    election_t *election = get_election( service );

    return !ip_b_is_zeroes( election->leader_ip );
}

sock_addr_t election_a_get_leader( uint32_t service ){

    sock_addr_t addr = {0};

    election_t *election = get_election( service );

    if( election == 0 ){
    
        return addr;
    }

    addr.ipaddr = election->leader_ip;
    addr.port   = election->port;

    return addr;
}


static void init_header( election_header_t *header ){

    header->magic       = ELECTION_MAGIC;
    header->version     = ELECTION_VERSION;
    header->flags       = 0;
    header->count       = 0;
    header->padding     = 0;
    header->device_id   = cfg_u64_get_device_id();
    header->uptime      = tmr_u64_get_system_time_us();
}


PT_THREAD( election_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // startup delay
    TMR_WAIT( pt, 4000 + ( rnd_u16_get_int() >> 5 ) ); // 4 to 6 seconds
    
        
    while(1){

        THREAD_WAIT_WHILE( pt, campaigns_count() == 0 );

        TMR_WAIT( pt, 1000 );

        uint16_t len = sizeof(election_header_t) + 
                        ( sizeof(election_pkt_t) * leader_elections_count() );

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

            if( election->priority == ELECTION_PRIORITY_CANDIDATE_ONLY ){

                goto next;
            }

            pkt->service    = election->service;
            pkt->group      = election->group;
            pkt->priority   = election->priority;
            pkt->port       = election->port;
            pkt->leader_ip  = election->leader_ip;

            header->count++;

            pkt++;

        next:
            ln = list_ln_next( ln );
        }

        sock_addr_t raddr;
        raddr.ipaddr = ip_a_addr(255,255,255,255);
        raddr.port   = ELECTION_PORT;

        sock_i16_sendto_m( sock, h, &raddr );

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}


static void process_pkt( election_header_t *header, election_pkt_t *pkt ){

    // look up matching election
    election_t *election = get_election( pkt->service );

    if( election == 0 ){

        return;
    }

    if( election->group != pkt-> group ){

        return;
    }

    // matching election
    if( election->state == STATE_IDLE ){


    }
    else if( election->state == STATE_CANDIDATE ){

        
    }
    else if( election->state == STATE_FOLLOWER ){

        
    }
    else if( election->state == STATE_LEADER ){

        
    }
    else{

        // invalid state
        ASSERT( FALSE );
    }
}


PT_THREAD( election_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, elections_count() == 0 );

    // create socket
    sock = sock_s_create( SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, ELECTION_PORT );

    sock_v_set_timeout( sock, 1 );


    // start sender
    thread_t_create( election_sender_thread,
                     PSTR("election_sender"),
                     0,
                     0 );

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

            process_pkt( header, pkt );

            pkt++;
            header->count--;
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}



