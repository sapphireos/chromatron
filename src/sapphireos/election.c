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

#include "system.h"
#include "wifi.h"
#include "config.h"
#include "timers.h"
#include "threading.h"
#include "random.h"

#include "election.h"

#if defined(ENABLE_NETWORK) && defined(ENABLE_ELECTION)

// #define NO_LOGGING
#include "logging.h"

static socket_t sock;

PT_THREAD( election_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( election_server_thread( pt_t *pt, void *state ) );
PT_THREAD( election_timer_thread( pt_t *pt, void *state ) );


#define STATE_IDLE      0
#define STATE_CANDIDATE 1
#define STATE_FOLLOWER  2
#define STATE_LEADER    3


typedef struct  __attribute__((packed)){
    uint32_t service;
    uint32_t group;

    // our priority and port
    uint16_t priority;
    uint16_t port;
    uint32_t cycles;

    // current leader information
    uint64_t leader_device_id;
    uint16_t leader_priority;
    uint32_t leader_cycles;
    uint16_t leader_port;   
    ip_addr4_t leader_ip;

    uint8_t timeout;
    uint8_t state;
} election_t;

static list_t elections_list;


static uint16_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &elections_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &elections_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}


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

    // create vfile
    fs_f_create_virtual( PSTR("electioninfo"), vfile );

    // debug: test election
    // election_v_join( 0x1234, 0, 1, 9090 );
}


static void reset_state( election_t *election ){

    log_v_info_P( PSTR("Reset to IDLE") );

    election->state      = STATE_IDLE;  
    election->cycles     = 0;  
    election->timeout    = IDLE_TIMEOUT;

    election->leader_device_id      = 0;
    election->leader_priority       = 0;
    election->leader_cycles         = 0;
    election->leader_port           = 0;
    election->leader_ip             = ip_a_addr(0,0,0,0);
}

void election_v_handle_shutdown( ip_addr4_t ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // look for any elections with this IP as a leader, and reset them

    list_node_t ln = elections_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        election_t *election = (election_t *)list_vp_get_data( ln );

        if( ip_b_addr_compare( election->leader_ip, ip ) ){

            log_v_debug_P( PSTR("leader shutdown") );

            reset_state( election );
        }

        ln = next_ln;
    }
}

static uint8_t elections_count( void ){

    return list_u8_count( &elections_list );
}

static election_t* get_election( uint32_t service ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

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

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // check if election already exists
    election_t *election_ptr = get_election( service );
    
    if( election_ptr != 0 ){

        // check if priority changed, if so, reset election state
        if( priority != election_ptr->priority ){

            reset_state( election_ptr );    
        }

        // update priority
        election_ptr->priority = priority;
        
        return;
    }

    election_t election = {0};
    election.service    = service;
    election.group      = group;
    election.priority   = priority;
    election.port       = port;
    election.cycles     = 0;

    reset_state( &election );
    
    list_node_t ln = list_ln_create_node( &election, sizeof(election) );

    if( ln < 0 ){

        return;
    }

    log_v_info_P( PSTR("create election: service: %lu group: %lu priority: %u"), service, group, priority );

    list_v_insert_tail( &elections_list, ln );  
}

void election_v_cancel( uint32_t service ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

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

    if( election == 0 ){

        return FALSE;
    }

    return !ip_b_is_zeroes( election->leader_ip );
}

bool election_b_is_leader( uint32_t service ){

    election_t *election = get_election( service );

    if( election == 0 ){

        return FALSE;
    }

    return election->state == STATE_LEADER;
}

sock_addr_t election_a_get_leader( uint32_t service ){

    sock_addr_t addr;
    memset( &addr, 0, sizeof(addr) );

    election_t *election = get_election( service );

    if( election == 0 ){
    
        return addr;
    }

    addr.ipaddr = election->leader_ip;
    addr.port   = election->port;

    return addr;
}

ip_addr4_t election_a_get_leader_ip( uint32_t service ){

    election_t *election = get_election( service );

    if( election == 0 ){
    
        return ip_a_addr(0,0,0,0);
    }

    return election->leader_ip;
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
    
    // check cycle count
    if( election->cycles < election->leader_cycles ){

        return FALSE;
    }
    else if( election->cycles > election->leader_cycles ){

        log_v_debug_P( PSTR("cycles: %lu %lu"), (uint32_t)election->cycles, (uint32_t)election->leader_cycles );

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
    
    // check cycle count
    if( pkt->cycles < election->leader_cycles ){

        return FALSE;
    }
    else if( pkt->cycles > election->leader_cycles ){

        log_v_debug_P( PSTR("cycles: %lu %lu"), (uint32_t)pkt->cycles, (uint32_t)election->leader_cycles );

        return TRUE;
    }

    // uptime is the same

    // check device ID
    if( header->device_id < election->leader_device_id ){

        return TRUE;
    }

    return FALSE;
}

static void track_node( election_t *election, election_header_t *header, election_pkt_t *pkt, ip_addr4_t *ip ){

    if( !ip_b_addr_compare( election->leader_ip, *ip ) ){

        log_v_debug_P( PSTR("tracking %d.%d.%d.%d"), ip->ip3, ip->ip2, ip->ip1, ip->ip0 );    
    }
    
    election->leader_device_id  = header->device_id;
    election->leader_priority   = pkt->priority;
    election->leader_cycles     = pkt->cycles;
    election->leader_port       = pkt->port;
    election->leader_ip         = *ip;
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

static void transmit_election( election_t *election, ip_addr4_t *ip, uint8_t flags ){

    uint16_t count = 1;

    // send all
    if( election == 0 ){

        count = transmit_count();
    }

    if( count == 0 ){

        return;
    }

    uint16_t len = sizeof(election_header_t) + 
                        ( sizeof(election_pkt_t) * transmit_count() );

    mem_handle_t h = mem2_h_alloc( len );

    if( h < 0 ){

        return;
    }

    election_header_t *header = mem2_vp_get_ptr_fast( h );
    memset( header, 0, len );

    init_header( header );

    header->flags |= flags;
    
    election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

    // send all
    if( election == 0 ){

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
            pkt->cycles     = election->cycles;

            if( election->state == STATE_LEADER ){
            
                pkt->flags |= ELECTION_PKT_FLAGS_LEADER;
            }
            
            header->count++;

            pkt++;

        next:
            ln = list_ln_next( ln );
        }
    }
    else{

        pkt->service    = election->service;
        pkt->group      = election->group;
        pkt->priority   = election->priority;
        pkt->port       = election->port;
        pkt->cycles     = election->cycles;

        if( election->state == STATE_LEADER ){
        
            pkt->flags |= ELECTION_PKT_FLAGS_LEADER;
        }
        
        header->count++;
    }

    sock_addr_t raddr;
    
    if( ip == 0 ){
    
        raddr.ipaddr = ip_a_addr(255,255,255,255);    
    }
    else{

        raddr.ipaddr = *ip;
    }
    
    raddr.port   = ELECTION_PORT;

    sock_i16_sendto_m( sock, h, &raddr );
}

static void transmit_query( election_t *election ){

    uint16_t len = sizeof(election_header_t) + sizeof(election_query_t );

    mem_handle_t h = mem2_h_alloc( len );

    if( h < 0 ){

        return;
    }

    election_header_t *header = mem2_vp_get_ptr_fast( h );
    memset( header, 0, len );

    init_header( header );
    header->count = 1;
    header->flags |= ELECTION_HDR_FLAGS_QUERY;
    
    election_query_t *pkt = (election_query_t *)( header + 1 );

    pkt->service  = election->service;
    pkt->group    = election->group;

    sock_addr_t raddr;
    raddr.ipaddr = election->leader_ip;
    raddr.port   = ELECTION_PORT;

    sock_i16_sendto_m( sock, h, &raddr );
}

static void process_election_pkt( election_header_t *header, election_pkt_t *pkt, ip_addr4_t *ip ){

    // look up matching election
    election_t *election = get_election( pkt->service );

    if( election == 0 ){

        return;
    }

    if( election->group != pkt-> group ){

        return;
    }

    // PACKET STATE MACHINE

    // matching election
    if( election->state == STATE_IDLE ){

        // check if leader in packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            log_v_debug_P( PSTR("state: IDLE") );

            track_node( election, header, pkt, ip );    
        }
    }
    else if( election->state == STATE_CANDIDATE ){

        // check if leader in packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            log_v_debug_P( PSTR("state: CANDIDATE") );

            track_node( election, header, pkt, ip );
        }        
    }
    else if( election->state == STATE_FOLLOWER ){

        // check if packet is a better leader than current tracking (and this packet is not from the current leader)
        if( !ip_b_addr_compare( *ip, election->leader_ip ) && 
            compare_leader( election, header, pkt ) ){
            
            log_v_debug_P( PSTR("state: FOLLOWER") );

            // we reset back to idle
            reset_state( election );
        }
        // check if this packet is from our current leader
        else if( ip_b_addr_compare( *ip, election->leader_ip ) ){

            // log_v_debug_P( PSTR("state: FOLLOWER") );

            // update tracking
            track_node( election, header, pkt, ip );

            // reset timeout
            election->timeout   = FOLLOWER_TIMEOUT;
        }
    }
    else if( election->state == STATE_LEADER ){

        // check if this packet is better than current tracking
        if( compare_leader( election, header, pkt ) ){

            track_node( election, header, pkt, ip );

            // now that we've updated tracking
            // check if the tracked leader is better than us
            if( !compare_self( election ) ){

                log_v_debug_P( PSTR("state: LEADER") );

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

static void process_election_query( election_header_t *header, election_query_t *pkt, ip_addr4_t *ip ){
    
    // look up matching election
    election_t *election = get_election( pkt->service );

    if( election == 0 ){

        return;
    }

    if( election->group != pkt->group ){

        return;
    }

    transmit_election( election, ip, ELECTION_HDR_FLAGS_RESPONSE );
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

            // increment cycle count
            election->cycles++;
            election->leader_cycles++;

            if( election->timeout == 0 ){

                goto next;
            }

            election->timeout--;

            // check for query cycle
            if( ( election->timeout > 0 ) && 
                ( ( election->timeout % FOLLOWER_QUERY_TIMEOUT ) == 0 ) &&
                ( election->state == STATE_FOLLOWER ) ){

                // NOTE
                // if the follower query timeout is longer than the election broadcast interval,
                // this code path won't run very often, as the timeout will be reset by the
                // broadcast.  this mechanism is a useful backup if the broadcasts aren't 
                // being received.

                log_v_info_P( PSTR("query: %d"), election->timeout );
                transmit_query( election );
            }

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

                        log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
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
                        log_v_info_P( PSTR("-> CANDIDATE") );
                        election->state = STATE_CANDIDATE;
                        election->timeout = CANDIDATE_TIMEOUT;
                    }
                    else{

                        // sanity check - remove after debug
                        if( ip_b_is_zeroes( election->leader_ip ) ){

                            log_v_error_P( PSTR("STATE CHANGE FAIL") );    
                        }

                        // found a leader
                        log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
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
                    log_v_info_P( PSTR("-> LEADER") );
                    election->state = STATE_LEADER;
                }
                else{

                    // sanity check - remove after debug
                    if( ip_b_is_zeroes( election->leader_ip ) ){

                        log_v_error_P( PSTR("STATE CHANGE FAIL") );    
                    }

                    log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
                    election->state     = STATE_FOLLOWER;
                    election->timeout   = FOLLOWER_TIMEOUT;
                }
            }
            else if( election->state == STATE_FOLLOWER ){                    

                log_v_info_P( PSTR("FOLLOWER timeout: lost leader") );

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
    sock = sock_s_create( SOS_SOCK_DGRAM );

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

            // log_v_debug_P( PSTR("bad version") ); // this message will be noisy on version changes
            continue;
        }

        if( ( header->flags & ELECTION_HDR_FLAGS_QUERY ) == 0 ){

            if( sock_i16_get_bytes_read( sock ) != 
                ( sizeof(election_header_t) + header->count * sizeof(election_pkt_t) ) ){

                log_v_debug_P( PSTR("bad size") );
                continue;
            }
        }
        else{

            if( sock_i16_get_bytes_read( sock ) != 
                ( sizeof(election_header_t) + header->count * sizeof(election_query_t) ) ){

                log_v_debug_P( PSTR("bad size") );
                continue;
            }
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( ( header->flags & ELECTION_HDR_FLAGS_QUERY ) == 0 ){

            election_pkt_t *pkt = (election_pkt_t *)( header + 1 );

            while( header->count > 0 ){

                process_election_pkt( header, pkt, &raddr.ipaddr );

                pkt++;
                header->count--;
            }
        }
        else{

            // query
            election_query_t *pkt = (election_query_t *)( header + 1 );

            while( header->count > 0 ){

                process_election_query( header, pkt, &raddr.ipaddr );

                pkt++;
                header->count--;
            }
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}

PT_THREAD( election_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        THREAD_WAIT_WHILE( pt, transmit_count() == 0 );

        TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 6 ) ); // 1 to 2 seconds

        // check if shutting down
        if( sys_b_shutdown() ){

            THREAD_EXIT( pt );
        }

        transmit_election( 0, 0, 0 );

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}

#endif