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
#include "list.h"

#include "services.h"

#if defined(ENABLE_NETWORK) && defined(ENABLE_SERVICES)

// #define NO_LOGGING
#include "logging.h"

#define STATE_LISTEN        0
#define STATE_CONNECTED     1
#define STATE_SERVER        2
#define STATE_CANDIDATE     3

typedef struct __attribute__((packed)){
    uint32_t id;
    uint32_t group;   

    uint16_t server_priority;
    uint16_t server_port;
    ip_addr4_t server_ip;

    bool is_team;
    uint8_t state;
    uint8_t timeout;
} service_state_t;

typedef struct __attribute__((packed)){
    service_state_t service;

    // additional local information
    uint16_t local_priority;
    uint16_t local_port;
    uint32_t local_uptime;

    // additional leader information (main information is in service)
    uint64_t leader_device_id;
    uint32_t leader_uptime;
} team_state_t;


static socket_t sock;

static list_t service_list;

PT_THREAD( service_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( service_server_thread( pt_t *pt, void *state ) );
PT_THREAD( service_timer_thread( pt_t *pt, void *state ) );

static service_state_t* get_service( uint32_t id, uint32_t group ){

    list_node_t ln = service_list.head;

    while( ln > 0 ){

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        if( ( service->id == id ) &&
            ( service->group == group ) ){

            return service;
        }

        ln = list_ln_next( ln );
    }

    return 0;
}

static team_state_t* get_team( uint32_t id, uint32_t group ){

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return 0;
    }

    if( !service->is_team ){

        return 0;
    }

    return (team_state_t *)service;
}


void services_v_init( void ){

    list_v_init( &service_list );

    #if !defined(ENABLE_NETWORK) || !defined(ENABLE_WIFI)
    return;
    #endif

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    thread_t_create( service_server_thread,
                     PSTR("service_server"),
                     0,
                     0 );



}

void services_v_listen( uint32_t id, uint32_t group ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // check if service already registered
    if( get_service( id, group ) != 0 ){

        return;
    }

    service_state_t service = {0};
    service.id      = id;
    service.group   = group;

    list_node_t ln = list_ln_create_node2( &service, sizeof(service), MEM_TYPE_SERVICE );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &service_list, ln );    
}

// void services_v_offer( uint32_t id, uint32_t group, uint16_t priority, uint16_t port ){
    
    // if( sys_u8_get_mode() == SYS_MODE_SAFE ){

    //     return;
    // }

// }

void services_v_join_team( uint32_t id, uint32_t group, uint16_t priority, uint16_t port ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // check if service already registered
    if( get_service( id, group ) != 0 ){

        return;
    }

    team_state_t team = {0};
    team.service.id      = id;
    team.service.group   = group;
    team.service.is_team = TRUE;

    list_node_t ln = list_ln_create_node2( &team, sizeof(team), MEM_TYPE_SERVICE );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &service_list, ln );    
}

void services_v_cancel( uint32_t id, uint32_t group ){
    
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    list_node_t ln = service_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        if( ( service->id == id ) &&
            ( service->group == group ) ){

            list_v_remove( &service_list, ln );
            list_v_release_node( ln );

            return;
        }

        ln = next_ln;
    }
}

bool services_b_is_available( uint32_t id, uint32_t group ){

    return FALSE;
}

bool services_b_is_leader( uint32_t id, uint32_t group ){

    return FALSE;
}

sock_addr_t services_a_get( uint32_t service, uint32_t group ){

    sock_addr_t addr;
    memset( &addr, 0, sizeof(addr) );


    return addr;
}

ip_addr4_t services_a_get_ip( uint32_t service, uint32_t group ){

    return ip_a_addr(0,0,0,0);
}




static void init_header( service_msg_header_t *header, uint8_t type ){

    header->magic       = SERVICES_MAGIC;
    header->version     = SERVICES_VERSION;
    header->type       = type;
    header->flags       = 0;
    header->reserved    = 0;
}

static bool should_transmit( service_state_t *service ){

    if( ( service->state == STATE_LISTEN ) ||
        ( service->state == STATE_CONNECTED ) ){

        return FALSE;
    }   
    
    return TRUE;
}

static uint8_t service_count( void ){

    return list_u8_count( &service_list );
}

// number of services to transmit
static uint8_t transmit_count( void ){

    uint8_t count = 0;

    list_node_t ln = service_list.head;

    while( ln > 0 ){

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        if( should_transmit( service ) ){

            count++;
        }

        ln = list_ln_next( ln );
    }

    return count;
}


static void transmit_service( service_state_t *service, ip_addr4_t *ip ){

    uint16_t count = 1;

    // send all
    if( service == 0 ){

        count = transmit_count();

        if( count == 0 ){

           return;
        }
    }

    uint16_t len = sizeof(service_msg_header_t) + 
                   sizeof(service_msg_offer_hdr_t) + 
                   ( sizeof(service_msg_offer_t) * count );

    mem_handle_t h = mem2_h_alloc( len );

    if( h < 0 ){

        return;
    }

    service_msg_header_t *msg_header = mem2_vp_get_ptr_fast( h );
    memset( msg_header, 0, len ); // set all bits to 0

    init_header( msg_header, SERVICE_MSG_TYPE_OFFERS );

    service_msg_offer_hdr_t *header = (service_msg_offer_hdr_t *)( msg_header + 1 );
    header->device_id = cfg_u64_get_device_id();

    service_msg_offer_t *pkt = (service_msg_offer_t *)( header + 1 );

    // send all
    if( service == 0 ){

        list_node_t ln = service_list.head;

        while( ln > 0 ){

            service = (service_state_t *)list_vp_get_data( ln );

            if( !should_transmit( service ) ){

                goto next;
            }

            pkt->id         = service->id;
            pkt->group      = service->group;

            if( service->is_team ){

                team_state_t *team = (team_state_t *)service;
            
                pkt->flags |= SERVICE_OFFER_FLAGS_TEAM;

                pkt->priority   = team->local_priority;
                pkt->port       = team->local_port;
                pkt->uptime     = team->local_uptime;
            }

            if( service->state == STATE_SERVER ){
            
                pkt->flags |= SERVICE_OFFER_FLAGS_SERVER;
            }
            
            header->count++;

            pkt++;

        next:
            ln = list_ln_next( ln );
        }
    }
    else{

        pkt->id         = service->id;
        pkt->group      = service->group;
        
        if( service->is_team ){
            
            team_state_t *team = (team_state_t *)service;
            
            pkt->flags |= SERVICE_OFFER_FLAGS_TEAM;

            pkt->priority   = team->local_priority;
            pkt->port       = team->local_port;
            pkt->uptime     = team->local_uptime;
        }

        if( service->state == STATE_SERVER ){
        
            pkt->flags |= SERVICE_OFFER_FLAGS_SERVER;
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
    
    raddr.port   = SERVICES_PORT;

    sock_i16_sendto_m( sock, h, &raddr );
}


PT_THREAD( service_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){
        
        TMR_WAIT( pt, SERVICE_RATE );

        THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) && ( transmit_count() == 0 ) );

        TMR_WAIT( pt, rnd_u16_get_int() >> 6 ); // add 1 second of random delay

        // check if shutting down
        if( sys_b_shutdown() ){

            THREAD_EXIT( pt );
        }

        transmit_service( 0, 0 );
    }    
    
PT_END( pt );
}


static void process_offer( service_msg_offer_hdr_t *header, service_msg_offer_t *pkt, ip_addr4_t *ip ){

    // look up matching service
    service_state_t *service = get_service( pkt->id, pkt->group );

    if( service == 0 ){

        return;
    }

    // PACKET STATE MACHINE


    if( !service->is_team ){



    }
    else{

        // if( service->state == STATE_IDLE ){

        //     // check if leader in packet is better than current tracking
        //     if( compare_leader( service, header, pkt ) ){

        //         log_v_debug_P( PSTR("state: IDLE") );

        //         track_node( service, header, pkt, ip );    
        //     }
        // }
        // else if( service->state == STATE_CANDIDATE ){

        //     // check if leader in packet is better than current tracking
        //     if( compare_leader( service, header, pkt ) ){

        //         log_v_debug_P( PSTR("state: CANDIDATE") );

        //         track_node( service, header, pkt, ip );
        //     }        
        // }
        // else if( service->state == STATE_FOLLOWER ){

        //     // check if packet is a better leader than current tracking (and this packet is not from the current leader)
        //     if( !ip_b_addr_compare( *ip, service->leader_ip ) && 
        //         compare_leader( service, header, pkt ) ){
                
        //         log_v_debug_P( PSTR("state: FOLLOWER") );

        //         track_node( service, header, pkt, ip );

        //         // if tracking is a leader
        //         if( service->is_leader ){

        //             // we reset back to idle
        //             reset_state( service );
        //         }
        //     }
        //     // check if this packet is from our current leader
        //     else if( ip_b_addr_compare( *ip, service->leader_ip ) ){

        //         // log_v_debug_P( PSTR("state: FOLLOWER") );

        //         // update tracking
        //         track_node( service, header, pkt, ip );

        //         // reset timeout
        //         service->timeout   = FOLLOWER_TIMEOUT;

        //         // check if leader is still better than we are
        //         if( compare_self( service ) ){

        //             // no, we are better

        //             // hmm, let's re-run the service
        //             reset_state( service );
        //         }
        //     }
        // }
        // else if( service->state == STATE_LEADER ){

        //     // check if this packet is better than current tracking
        //     if( compare_leader( service, header, pkt ) ){

        //         track_node( service, header, pkt, ip );
        //         service->timeout   = CANDIDATE_TIMEOUT;

        //         // now that we've updated tracking
        //         // check if the tracked leader is better than us
        //         if( !compare_self( service ) ){

        //             log_v_debug_P( PSTR("state: LEADER") );

        //             // tracked leader is better
        //             // we reset back to idle
        //             reset_state( service );
        //         }
        //     }

        //     // if we are still leader
        //     if( service->state == STATE_LEADER ){

        //         // is this packet a candidate?
        //         if( ( pkt->flags & ELECTION_PKT_FLAGS_LEADER ) == 0 ){

        //             // boost our broadcast rate
        //             rate_boost = ELECTION_RATE_BOOST;
        //         }
        //     }
        // }
        // else{

        //     // invalid state
        //     ASSERT( FALSE );
        // }
    }
}


PT_THREAD( service_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, service_count() == 0 );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, SERVICES_PORT );


    // start sender
    thread_t_create( service_sender_thread,
                     PSTR("service_sender"),
                     0,
                     0 );

    thread_t_create( service_timer_thread,
                     PSTR("service_timer"),
                     0,
                     0 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        service_msg_header_t *header = sock_vp_get_data( sock );

        if( header->magic != SERVICES_MAGIC ){

            continue;
        }

        if( header->version != SERVICES_VERSION ){

            continue;
        }

        // if( ( header->flags & ELECTION_HDR_FLAGS_QUERY ) == 0 ){

        //     if( sock_i16_get_bytes_read( sock ) != 
        //         ( sizeof(election_header_t) + header->count * sizeof(election_pkt_t) ) ){

        //         log_v_debug_P( PSTR("bad size") );
        //         continue;
        //     }
        // }
        // else{

        //     if( sock_i16_get_bytes_read( sock ) != 
        //         ( sizeof(election_header_t) + header->count * sizeof(election_query_t) ) ){

        //         log_v_debug_P( PSTR("bad size") );
        //         continue;
        //     }
        // }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->type == SERVICE_MSG_TYPE_OFFERS ){

            service_msg_offer_hdr_t *offer_hdr = (service_msg_offer_hdr_t *)( header + 1 );
            service_msg_offer_t *offer = (service_msg_offer_t *)offer_hdr;

            while( offer_hdr->count > 0 ){

                process_offer( offer_hdr, offer, &raddr.ipaddr );

                offer++;
                offer_hdr->count--;
            }
        }
        else if( header->type == SERVICE_MSG_TYPE_QUERY ){

            // log_v_debug_P( PSTR("query from %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

            // query
            // election_query_t *pkt = (election_query_t *)( header + 1 );

            // while( header->count > 0 ){

            //     process_election_query( header, pkt, &raddr.ipaddr );

            //     pkt++;
            //     header->count--;
            // }
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}

PT_THREAD( service_timer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) && ( service_count() == 0 ) );

        TMR_WAIT( pt, 1000 );
    
        list_node_t ln = service_list.head;

        while( ln > 0 ){

            service_state_t *service = (service_state_t *)list_vp_get_data( ln );







            // // increment cycle count
            // election->cycles++;
            // election->leader_cycles++;

            // if( election->timeout == 0 ){

            //     if( election->state == STATE_LEADER ){                    

            //         // clear tracking info
            //         clear_tracking( election );           
            //     }
            //     else if( election->state != STATE_IDLE ){

            //         // the only state that can have a time out of 0 is LEADER and IDLE.
            //         reset_state( election );
            //     }

            //     goto next;
            // }

            // election->timeout--;

            // // timeout not expired
            // if( election->timeout > 0 ){

            //     // PRE-TIMEOUT LOGIC

            //     // check if we need to query our leader
            //     if( ( election->is_leader ) &&
            //         ( election->state == STATE_FOLLOWER ) && 
            //         ( election->timeout < ( FOLLOWER_TIMEOUT - FOLLOWER_QUERY_TIMEOUT ) ) ){

            //         // transmit query to leader.

            //         // this will repeat on the timer interval until we get a response
            //         // or time out.
            //         transmit_query( election );
            //     }

            //     // DONE with further processing for now.

            //     goto next;
            // }

            // // TIMEOUT STATE MACHINE

            // if( election->state == STATE_IDLE ){

            //     log_v_debug_P( PSTR("IDLE timeout") );

            //     // check if can only be a follower
            //     if( election->priority == ELECTION_PRIORITY_FOLLOWER_ONLY ){

            //         // do we have a leader?
            //         if( election->is_leader && !ip_b_is_zeroes( election->leader_ip ) ){

            //             log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
            //             election->state     = STATE_FOLLOWER;   
            //             election->timeout   = FOLLOWER_TIMEOUT;                     
            //         }
            //         else{

            //             // no leader, reset
            //             reset_state( election );
            //         }
            //     }
            //     else{

            //         // compare us to best leader we've seen
            //         if( compare_self( election ) ){

            //             // switch to candidate so we inform other nodes
            //             log_v_info_P( PSTR("-> CANDIDATE") );
            //             election->state = STATE_CANDIDATE;
            //             election->timeout = CANDIDATE_TIMEOUT;
            //         }
            //         // if tracking a leader
            //         else if( election->is_leader ){

            //             // sanity check - remove after debug
            //             if( ip_b_is_zeroes( election->leader_ip ) ){

            //                 log_v_error_P( PSTR("STATE CHANGE FAIL") );    
            //             }

            //             // found a leader
            //             log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
            //             election->state     = STATE_FOLLOWER;
            //             election->timeout   = FOLLOWER_TIMEOUT;                
            //         }
            //         else{

            //             // no leader, reset
            //             reset_state( election );
            //         }
            //     }
            // }
            // else if( election->state == STATE_CANDIDATE ){

            //     log_v_debug_P( PSTR("CANDIDATE timeout") );

            //     // compare us to best leader we've seen
            //     if( compare_self( election ) ){

            //         log_v_debug_P( PSTR("No leader found -> elect ourselves LEADER") );
            //         log_v_info_P( PSTR("-> LEADER") );
            //         election->state = STATE_LEADER;
            //     }
            //     else{

            //         // sanity check
            //         if( ip_b_is_zeroes( election->leader_ip ) ){

            //             reset_state( election );

            //             goto next;
            //         }

            //         // check if leader
            //         if( election->is_leader ){

            //             log_v_info_P( PSTR("-> FOLLOWER of: %d.%d.%d.%d"), election->leader_ip.ip3, election->leader_ip.ip2, election->leader_ip.ip1, election->leader_ip.ip0 );
            //             election->state     = STATE_FOLLOWER;
            //             election->timeout   = FOLLOWER_TIMEOUT;
            //         }
            //         else{

            //             reset_state( election );
            //         }
            //     }
            // }
            // else if( election->state == STATE_FOLLOWER ){                    

            //     log_v_info_P( PSTR("FOLLOWER timeout: lost leader") );

            //     reset_state( election );
            // }
            
next:
            ln = list_ln_next( ln );
        }
    }    
    
PT_END( pt );
}



































#endif
