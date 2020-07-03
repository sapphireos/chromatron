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
PT_THREAD( election_timer_thread( pt_t *pt, void *state ) );


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
    uint16_t random;

    // current leader information
    uint32_t tracking_timestamp;
    uint64_t leader_device_id;
    uint16_t leader_priority;
    uint16_t leader_random;
    uint16_t leader_port;   
    ip_addr4_t leader_ip;
    uint8_t timeout;

    election_state_t state;
} election_t;

static list_t elections_list;


void election_v_init( void ){

    list_v_init( &elections_list );

    #if !defined(ENABLE_NETWORK) || !defined(ENABLE_WIFI)
    return;
    #endif

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( election_server_thread,
                     PSTR("election_server"),
                     0,
                     0 );

    // debug: test election
    election_v_join( 0x1234, 0, 1, 9090 );
}

static uint8_t elections_count( void ){

    return list_u8_count( &elections_list );
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

static void reset_state( election_t *election ){

    log_v_debug_P( PSTR("Reset to IDLE") );

    election->state      = STATE_IDLE;    
    election->timeout    = IDLE_TIMEOUT;

    election->tracking_timestamp    = 0;
    election->leader_device_id      = 0;
    election->leader_priority       = 0;
    election->leader_port           = 0;
    election->leader_ip             = ip_a_addr(0,0,0,0);
}

void election_v_join( uint32_t service, uint32_t group, uint16_t priority, uint16_t port ){

    // check if election already exists
    election_t *election_ptr = get_election( service );
    
    if( election_ptr != 0 ){

        // update priority
        election_ptr->priority   = priority;

        reset_state( election_ptr );
        
        return;
    }

    election_t election = {0};
    election.service    = service;
    election.group      = group;
    election.priority   = priority;
    election.port       = port;
    election.random     = rnd_u16_get_int();
    
    reset_state( &election );
    
    list_node_t ln = list_ln_create_node( &election, sizeof(election) );

    if( ln < 0 ){

        return;
    }

    log_v_debug_P( PSTR("create election: service: %lu group: %lu priority: %u random: %u"), service, group, priority, election.random );

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

// true if we are better than tracked leader
static bool compare_self( election_t *election ){

    // check if a leader is being tracked.
    // if none, we win by default.
    if( ip_b_is_zeroes( election->leader_ip ) ){

        log_v_debug_P( PSTR("no ip") );

        return TRUE;
    }
    
    // check if tracked leader's priority is better than ours
    if( election->priority < election->leader_priority ){

        return FALSE;
    } 
    // our priority is bettere
    else if( election->priority > election->leader_priority ){

        log_v_debug_P( PSTR("priority win") );

        return TRUE;
    }

    // priorities are the same
    
    // check random value
    int8_t compare = util_i8_compare_sequence_u16( election->random, election->leader_random );

    if( compare < 0 ){

        return FALSE;
    }
    else if( compare > 0 ){

        log_v_debug_P( PSTR("random: %u %u"), election->random, election->leader_random );

        return TRUE;
    }

    // uptime is the same

    // check device ID
    if( cfg_u64_get_device_id() < election->leader_device_id ){

        log_v_debug_P( PSTR("device id") );

        return TRUE;
    }

    return FALSE;
}

// true if pkt is better than current election
static bool compare_leader( election_t *election, election_header_t *header, election_pkt_t *pkt ){

    // check if a leader is being tracked.
    // if none, the packet wins by default.
    if( ip_b_is_zeroes( election->leader_ip ) ){

        return TRUE;
    }

    // if message priority is lower, we're done, don't change
    if( pkt->priority < election->leader_priority ){

        return FALSE;
    }
    // message priority is higher, select new leader
    else if( pkt->priority > election->leader_priority ){

        return TRUE;
    }

    // priorities are the same
    
    // check random value
    int8_t compare = util_i8_compare_sequence_u16( pkt->random, election->leader_random );

    if( compare < 0 ){

        return FALSE;
    }
    else if( compare > 0 ){

        log_v_debug_P( PSTR("random: %u %u"), pkt->random, election->leader_random );

        return TRUE;
    }

    // uptime is the same

    // check device ID
    if( header->device_id < election->leader_device_id ){

        return TRUE;
    }

    return FALSE;
}

static void track_leader( election_t *election, election_header_t *header, election_pkt_t *pkt, ip_addr4_t *ip ){

    if( !ip_b_addr_compare( election->leader_ip, *ip ) ){

        log_v_debug_P( PSTR("tracking %d.%d.%d.%d"), ip->ip3, ip->ip2, ip->ip1, ip->ip0 );    
    }
    
    election->leader_device_id  = header->device_id;
    election->leader_priority   = pkt->priority;
    election->leader_random     = pkt->random;
    election->leader_port       = pkt->port;
    election->leader_ip         = *ip;

    election->tracking_timestamp = tmr_u32_get_system_time_us();
}


static void process_pkt( election_header_t *header, election_pkt_t *pkt, ip_addr4_t *ip ){

    // look up matching election
    election_t *election = get_election( pkt->service );

    if( election == 0 ){

        return;
    }

    if( election->group != pkt-> group ){

        return;
    }

    log_v_debug_P( PSTR("recv pkt") );

    // PACKET STATE MACHINE

    // matching election
    if( election->state == STATE_IDLE ){

        log_v_debug_P( PSTR("state: IDLE") );

        // check if leader in packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            track_leader( election, header, pkt, ip );    
        }
    }
    else if( election->state == STATE_CANDIDATE ){

        log_v_debug_P( PSTR("state: CANDIDATE") );

        // check if leader in packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            track_leader( election, header, pkt, ip );
        }        
    }
    else if( election->state == STATE_FOLLOWER ){

        log_v_debug_P( PSTR("state: FOLLOWER") );

        // check if packet is a better leader than current tracking
        if( compare_leader( election, header, pkt ) ){

            // we reset back to idle
            reset_state( election );
        }
        // check if this packet is from our current leader
        else if( ip_b_addr_compare( *ip, election->leader_ip ) ){

            // update tracking
            track_leader( election, header, pkt, ip );

            // reset timeout
            election->timeout   = FOLLOWER_TIMEOUT;
        }
    }
    else if( election->state == STATE_LEADER ){

        log_v_debug_P( PSTR("state: LEADER") );

        // check if this packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            track_leader( election, header, pkt, ip );

            // now that we've updated tracking
            // check if the tracked leader is better than us
            if( !compare_self( election ) ){

                // tracked leader is better
                // we reset back to idle
                reset_state( election );
            }
        }
    }
    else{

        // invalid state
        ASSERT( FALSE );
    }
}

PT_THREAD( election_timer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        THREAD_WAIT_WHILE( pt, elections_count() == 0 );

        TMR_WAIT( pt, 1000 );
    
        list_node_t ln = elections_list.head;

        while( ln > 0 ){

            election_t *election = (election_t *)list_vp_get_data( ln );

            if( election->timeout == 0 ){

                goto next;
            }

            election->timeout--;

            if( election->timeout > 0 ){

                goto next;
            }

            // TIMEOUT STATE MACHINE

            if( election->state == STATE_IDLE ){

                log_v_debug_P( PSTR("IDLE timeout") );

                // check if can only be a follower
                if( election->priority == ELECTION_PRIORITY_FOLLOWER_ONLY ){

                    // do we have a leader?
                    if( !ip_b_is_zeroes( election->leader_ip ) ){

                        log_v_debug_P( PSTR("-> FOLLOWER") );
                        election->state     = STATE_FOLLOWER;   
                        election->timeout   = FOLLOWER_TIMEOUT;                     
                    }
                    else{

                        // no leader, reset
                        reset_state( election );
                    }
                }
                else{

                    // compare us to best leader we've seen
                    if( compare_self( election ) ){

                        // switch to candidate so we inform other nodes
                        log_v_debug_P( PSTR("-> CANDIDATE") );
                        election->state = STATE_CANDIDATE;
                        election->timeout = CANDIDATE_TIMEOUT;
                    }
                    else{

                        // sanity check - remove after debug
                        if( ip_b_is_zeroes( election->leader_ip ) ){

                            log_v_error_P( PSTR("STATE CHANGE FAIL") );    
                        }

                        // found a leader
                        log_v_debug_P( PSTR("-> FOLLOWER") );
                        election->state     = STATE_FOLLOWER;
                        election->timeout   = FOLLOWER_TIMEOUT;                
                    }
                }
            }
            else if( election->state == STATE_CANDIDATE ){

                log_v_debug_P( PSTR("CANDIDATE timeout") );

                // compare us to best leader we've seen
                if( compare_self( election ) ){

                    log_v_debug_P( PSTR("No leader found -> elect ourselves LEADER") );
                    log_v_debug_P( PSTR("-> LEADER") );
                    election->state = STATE_LEADER;
                }
                else{

                    // sanity check - remove after debug
                    if( ip_b_is_zeroes( election->leader_ip ) ){

                        log_v_error_P( PSTR("STATE CHANGE FAIL") );    
                    }

                    log_v_debug_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
                    election->state     = STATE_FOLLOWER;
                    election->timeout   = FOLLOWER_TIMEOUT;
                }
            }
            else if( election->state == STATE_FOLLOWER ){                    

                log_v_debug_P( PSTR("FOLLOWER timeout: lost leader") );

                reset_state( election );
            }
            
next:
            ln = list_ln_next( ln );
        }
    }    
    
PT_END( pt );
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

    thread_t_create( election_timer_thread,
                     PSTR("election_timer"),
                     0,
                     0 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        election_header_t *header = sock_vp_get_data( sock );

        if( header->magic != ELECTION_MAGIC ){

            log_v_debug_P( PSTR("bad magic") );
            continue;
        }

        if( header->version != ELECTION_VERSION ){

            log_v_debug_P( PSTR("bad version") );
            continue;
        }

        if( sock_i16_get_bytes_read( sock ) != 
            ( sizeof(election_header_t) + header->count * sizeof(election_pkt_t) ) ){

            log_v_debug_P( PSTR("bad size") );
            continue;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

        while( header->count > 0 ){

            process_pkt( header, pkt, &raddr.ipaddr );

            pkt++;
            header->count--;
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}



static void init_header( election_header_t *header ){

    header->magic       = ELECTION_MAGIC;
    header->version     = ELECTION_VERSION;
    header->flags       = 0;
    header->count       = 0;
    header->padding     = 0;
    header->device_id   = cfg_u64_get_device_id();
}


static bool should_transmit( election_t *election ){

    if( election->priority == ELECTION_PRIORITY_FOLLOWER_ONLY ){

        return FALSE;
    }
    else if( ( election->state == STATE_IDLE ) ||
             ( election->state == STATE_FOLLOWER ) ){

        return FALSE;
    }    

    return TRUE;
}

// elections for which we can become leader
static uint8_t transmit_count( void ){

    uint8_t count = 0;

    list_node_t ln = elections_list.head;

    while( ln > 0 ){

        election_t *election = (election_t *)list_vp_get_data( ln );

        if( should_transmit( election ) ){

            count++;
        }

        ln = list_ln_next( ln );
    }

    return count;
}


PT_THREAD( election_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        THREAD_WAIT_WHILE( pt, transmit_count() == 0 );

        TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 6 ) ); // 1 to 2 seconds
    
        uint16_t len = sizeof(election_header_t) + 
                        ( sizeof(election_pkt_t) * transmit_count() );

        mem_handle_t h = mem2_h_alloc( len );

        if( h < 0 ){

            continue;
        }

        election_header_t *header = mem2_vp_get_ptr_fast( h );
        memset( header, 0, len );

        init_header( header );

        election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

        list_node_t ln = elections_list.head;

        while( ln > 0 ){

            election_t *election = (election_t *)list_vp_get_data( ln );

            if( !should_transmit( election ) ){

                goto next;
            }

            pkt->service    = election->service;
            pkt->group      = election->group;
            pkt->priority   = election->priority;
            pkt->port       = election->port;
            pkt->random     = election->random;
            
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
