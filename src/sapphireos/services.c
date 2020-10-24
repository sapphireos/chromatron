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

typedef struct __attribute__((packed)){
    uint32_t id;
    uint32_t group;   

    uint16_t server_priority;
    uint32_t server_uptime;
    uint16_t server_port;
    ip_addr4_t server_ip;
    bool server_valid;

    uint16_t local_priority;
    uint16_t local_port;
    uint32_t local_uptime;

    bool is_team;
    uint8_t state;
    uint8_t timeout;
} service_state_t;


static socket_t sock;

static list_t service_list;

PT_THREAD( service_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( service_server_thread( pt_t *pt, void *state ) );
PT_THREAD( service_timer_thread( pt_t *pt, void *state ) );

static void clear_tracking( service_state_t *service ){

    service->server_priority       = 0;
    service->server_uptime         = 0;
    service->server_port           = 0;
    service->server_ip             = ip_a_addr(0,0,0,0);
    service->server_valid          = FALSE;
}

static void _reset_state( service_state_t *service ){

    service->state          = STATE_LISTEN;  
    service->timeout        = SERVICE_LISTEN_TIMEOUT;
    service->local_uptime   = 0;

    clear_tracking( service );
}

// #define reset_state(a) log_v_info_P( PSTR("Reset to LISTEN") ); _reset_state( a )
#define reset_state(a) _reset_state( a )

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

static uint16_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &service_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &service_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
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

    // create vfile
    fs_f_create_virtual( PSTR("serviceinfo"), vfile );
    
    services_v_listen( 0x5678, 0 );
    services_v_join_team( 0x1234, 0, 10, 1000 );
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

    reset_state( &service );

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

    service_state_t service = {0};
    service.id                  = id;
    service.group               = group;
    service.is_team             = TRUE;
    service.local_priority      = priority;
    service.local_port          = port;

    reset_state( &service );

    list_node_t ln = list_ln_create_node2( &service, sizeof(service), MEM_TYPE_SERVICE );

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

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return FALSE;
    }

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return FALSE;
    }

    if( service->state == STATE_CONNECTED ){

        return TRUE;
    }

    return FALSE;
}

bool services_b_is_server( uint32_t id, uint32_t group ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return FALSE;
    }

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return FALSE;
    }

    if( service->state == STATE_SERVER ){

        return TRUE;
    }

    return FALSE;
}

sock_addr_t services_a_get( uint32_t id, uint32_t group ){

    sock_addr_t addr;
    memset( &addr, 0, sizeof(addr) );

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return addr;
    }

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return addr;
    }

    if( service->state == STATE_LISTEN ){

        return addr;
    }

    addr.ipaddr = service->server_ip;
    addr.port = service->server_port;

    return addr;
}

ip_addr4_t services_a_get_ip( uint32_t id, uint32_t group ){

    sock_addr_t addr = services_a_get( id, group );

    return addr.ipaddr;
}


static void init_header( service_msg_header_t *header, uint8_t type ){

    header->magic       = SERVICES_MAGIC;
    header->version     = SERVICES_VERSION;
    header->type       = type;
    header->flags       = 0;
    header->reserved    = 0;
}

static bool should_transmit( service_state_t *service ){

    if( service->state == STATE_CONNECTED ){

        return FALSE;
    }

    if( service->state == STATE_LISTEN ){

        if( service->is_team ){

            if( service->local_priority == SERVICE_PRIORITY_FOLLOWER_ONLY ){

                return FALSE;
            }
        }
        else{

            return FALSE;
        }
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

                pkt->flags |= SERVICE_OFFER_FLAGS_TEAM;

                pkt->priority   = service->local_priority;
                pkt->port       = service->local_port;
                pkt->uptime     = service->local_uptime;
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
            
            pkt->flags |= SERVICE_OFFER_FLAGS_TEAM;

            pkt->priority   = service->local_priority;
            pkt->port       = service->local_port;
            pkt->uptime     = service->local_uptime;
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

static void transmit_query( service_state_t *service ){

    uint16_t len = sizeof(service_msg_header_t) + sizeof(service_msg_query_t );

    mem_handle_t h = mem2_h_alloc( len );

    if( h < 0 ){

        return;
    }

    service_msg_header_t *header = mem2_vp_get_ptr_fast( h );
    memset( header, 0, len );

    init_header( header, SERVICE_MSG_TYPE_QUERY );

    service_msg_query_t *pkt = (service_msg_query_t *)( header + 1 );

    pkt->id      = service->id;
    pkt->group   = service->group;

    sock_addr_t raddr;

    if( ip_b_is_zeroes( service->server_ip ) ){

        // no server IP, broadcast

        raddr.ipaddr = ip_a_addr(255,255,255,255);
    }
    else{

        raddr.ipaddr = service->server_ip;
    }

    raddr.port   = SERVICES_PORT;

    log_v_debug_P( PSTR("tx query %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

    sock_i16_sendto_m( sock, h, &raddr );
}


PT_THREAD( service_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) || ( transmit_count() == 0 ) );

    TMR_WAIT( pt, rnd_u16_get_int() >> 6 ); // add 1 second of random delay

    // initial broadcast offer
    transmit_service( 0, 0 );

    TMR_WAIT( pt, 500 );
    
    // initial broadcast query

    list_node_t ln = service_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        transmit_query( service );

        ln = next_ln;
    }


    while(1){
        
        TMR_WAIT( pt, SERVICE_RATE );

        THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) || ( transmit_count() == 0 ) );

        TMR_WAIT( pt, rnd_u16_get_int() >> 6 ); // add 1 second of random delay

        // check if shutting down
        if( sys_b_shutdown() ){

            THREAD_EXIT( pt );
        }

        transmit_service( 0, 0 );
    }    
    
PT_END( pt );
}

// true if we are better than tracked leader
static bool compare_self( service_state_t *service ){

    // check if a server is being tracked.
    // if none, we win by default.
    if( ip_b_is_zeroes( service->server_ip ) ){

        log_v_debug_P( PSTR("no ip") );

        return TRUE;
    }
    
    // check if tracked leader's priority is better than ours
    if( service->local_priority < service->server_priority ){

        return FALSE;
    } 
    // our priority is better
    else if( service->local_priority > service->server_priority ){

        log_v_debug_P( PSTR("priority win") );

        return TRUE;
    }

    // priorities are the same

    // only check uptime if the server is valid
    if( service->server_valid ){
        
        // check uptime
        int64_t diff = (int64_t)service->local_uptime - (int64_t)service->server_uptime;

        if( diff > SERVICE_UPTIME_MIN_DIFF ){

            log_v_debug_P( PSTR("uptime newer: %lu %lu"), (uint32_t)service->server_uptime, (uint32_t)service->local_uptime );

            return TRUE;
        }
        else if( diff < ( -1 * SERVICE_UPTIME_MIN_DIFF ) ){

            if( service->local_uptime > 0 ){

                log_v_debug_P( PSTR("older: %lu %lu"), (uint32_t)service->server_uptime, (uint32_t)service->local_uptime );
            }

            return FALSE;
        }
    }

    // uptime is the same (within SERVICE_UPTIME_MIN_DIFF) or server is not valid

    // compare IP addresses
    ip_addr4_t our_addr;
    cfg_i8_get( CFG_PARAM_IP_ADDRESS, &our_addr );

    if( ip_u32_to_int( our_addr ) < ip_u32_to_int( service->server_ip ) ){

        log_v_debug_P( PSTR("ip priority") );

        return TRUE;
    }

    return FALSE;
}

// true if pkt is better than current leader
static bool compare_server( service_state_t *service, service_msg_offer_hdr_t *header, service_msg_offer_t *offer, ip_addr4_t *ip ){

    // check if a leader is being tracked.
    // if none, the packet wins by default.
    if( ip_b_is_zeroes( service->server_ip ) ){

        return TRUE;
    }

    // if message priority is lower, we're done, don't change
    if( offer->priority < service->server_priority ){

        return FALSE;
    }
    // message priority is higher, select new leader
    else if( offer->priority > service->server_priority ){

        return TRUE;
    }

    // priorities are the same
    
    // check cycle count
    int64_t diff = (int64_t)offer->uptime - (int64_t)service->server_uptime;

    if( diff > SERVICE_UPTIME_MIN_DIFF ){

        log_v_debug_P( PSTR("uptime newer: %lu %lu"), (uint32_t)service->server_uptime, (uint32_t)offer->uptime );

        return TRUE;
    }
    else if( diff < ( -1 * SERVICE_UPTIME_MIN_DIFF ) ){

        log_v_debug_P( PSTR("older: %lu %lu"), (uint32_t)service->server_uptime, (uint32_t)offer->uptime );

        return FALSE;
    }

    // uptime is the same

    // compare IP addresses
    if( ip_u32_to_int( *ip ) < ip_u32_to_int( service->server_ip ) ){

        log_v_debug_P( PSTR("ip priority") );

        return TRUE;
    }
    
    return FALSE;
}

static void track_node( service_state_t *service, service_msg_offer_hdr_t *header, service_msg_offer_t *offer, ip_addr4_t *ip ){

    if( ( offer->flags & SERVICE_OFFER_FLAGS_SERVER ) != 0 ){

        service->server_valid = TRUE;
    }
    else{

        service->server_valid = FALSE;
    }

    if( !ip_b_addr_compare( service->server_ip, *ip ) ){

        log_v_debug_P( PSTR("tracking %d.%d.%d.%d valid: %d"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, service->server_valid );    
    }
    
    service->server_priority   = offer->priority;
    service->server_uptime     = offer->uptime;
    service->server_port       = offer->port;
    service->server_ip         = *ip;
}

static void process_offer( service_msg_offer_hdr_t *header, service_msg_offer_t *pkt, ip_addr4_t *ip ){

    // look up matching service
    service_state_t *service = get_service( pkt->id, pkt->group );

    if( service == 0 ){

        return;
    }

    // PACKET STATE MACHINE

    // SERVICE
    if( !service->is_team ){

        if( ( service->state == STATE_LISTEN ) ||
            ( service->state == STATE_CONNECTED ) ){

            // ensure packet indicates server, and also is not a team
            if( pkt->flags != SERVICE_OFFER_FLAGS_SERVER ){

                return;
            }

            // check if connected to this server
            if( ( service->state == STATE_CONNECTED ) &&
                ( ip_b_addr_compare( *ip, service->server_ip ) ) ){

                service->timeout   = SERVICE_CONNECTED_TIMEOUT;
            }
            // check priorities
            else if( compare_server( service, header, pkt, ip ) ){

                track_node( service, header, pkt, ip );
            }
        }
    }
    // TEAM:
    // are we already tracking this node?
    // if so, just update the tracking info
    else if( ip_b_addr_compare( *ip, service->server_ip ) ){

        bool valid = service->server_valid;

        track_node( service, header, pkt, ip );

        if( !valid && service->server_valid ){

            log_v_debug_P( PSTR("%d.%d.%d.%d is now valid"), ip->ip3, ip->ip2, ip->ip1, ip->ip0 );            
        }

        // are we connected to this node?
        if( service->state == STATE_CONNECTED ){

            // reset timeout
            service->timeout   = SERVICE_CONNECTED_TIMEOUT;

            // did the server reboot and we didn't notice?
            // OR did the server change to invalid?
            if( ( service->server_uptime > pkt->uptime ) ||
                ( ( pkt->flags & SERVICE_OFFER_FLAGS_SERVER ) == 0 ) ){

                // reset, maybe there is a better server available

                log_v_debug_P( PSTR("%d.%d.%d.%d is no longer valid"), ip->ip3, ip->ip2, ip->ip1, ip->ip0 );

                reset_state( service );
            }
            // check if server is still better than we are
            else if( compare_self( service ) ){

                // no, we are better
                log_v_debug_P( PSTR("we are a better server") );

                log_v_info_P( PSTR("-> SERVER") );
                service->state = STATE_SERVER;
            }
        }
    }
    // TEAM state machine
    else{

        log_v_debug_P( PSTR("offer from %d.%d.%d.%d flags: 0x%02x"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, pkt->flags );

        if( service->state == STATE_LISTEN ){

            // check if server in packet is better than current tracking
            if( compare_server( service, header, pkt, ip ) ){

                log_v_debug_P( PSTR("state: LISTEN") );

                track_node( service, header, pkt, ip );    
            }
        }
        else if( service->state == STATE_CONNECTED ){

            // check if packet is a better server than current tracking (and this packet is not from the current server)
            if( !ip_b_addr_compare( *ip, service->server_ip ) && 
                compare_server( service, header, pkt, ip ) ){
                
                log_v_debug_P( PSTR("state: CONNECTED") );

                track_node( service, header, pkt, ip );

                // if tracking is a server
                if( service->server_valid ){

                    log_v_debug_P( PSTR("hop to better server: %d.%d.%d.%d"), service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );

                    // reset timeout
                    service->timeout   = SERVICE_CONNECTED_TIMEOUT;
                }
            }
        }
        else if( service->state == STATE_SERVER ){

            // check if this packet is better than current tracking
            if( compare_server( service, header, pkt, ip ) ){

                // check if server is valid - we will only consider
                // other valid servers, not candidates
                if( service->server_valid ){

                    track_node( service, header, pkt, ip );

                    // now that we've updated tracking
                    // check if the tracked server is better than us
                    if( !compare_self( service ) ){

                        // tracked server is better
                        log_v_debug_P( PSTR("found a better server: %d.%d.%d.%d"), service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );

                        log_v_info_P( PSTR("-> CONNECTED") );

                        // reset timeout
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;
                        service->state     = STATE_CONNECTED;
                    }
                }
            }
        }
        else{

            // invalid state
            ASSERT( FALSE );
        }
    }
}

static void process_query( service_msg_query_t *query, ip_addr4_t *ip ){
    
    // look up matching election
    service_state_t *service = get_service( query->id, query->group );

    if( service == 0 ){

        return;
    }

    // check state
    if( service->state != STATE_SERVER ){

        return;
    }

    transmit_service( service, ip );
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

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->type == SERVICE_MSG_TYPE_OFFERS ){

            service_msg_offer_hdr_t *offer_hdr = (service_msg_offer_hdr_t *)( header + 1 );
            service_msg_offer_t *offer = (service_msg_offer_t *)( offer_hdr + 1 );

            // check packet length
            uint16_t len = sizeof(service_msg_header_t) + 
                   sizeof(service_msg_offer_hdr_t) + 
                   ( sizeof(service_msg_offer_t) * offer_hdr->count );

            if( sock_i16_get_bytes_read( sock ) != len ){

                log_v_debug_P( PSTR("bad size") );
                continue;
            }

            while( offer_hdr->count > 0 ){

                process_offer( offer_hdr, offer, &raddr.ipaddr );

                offer++;
                offer_hdr->count--;
            }
        }
        else if( header->type == SERVICE_MSG_TYPE_QUERY ){

            log_v_debug_P( PSTR("query from %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

            service_msg_query_t *query = (service_msg_query_t *)( header + 1 );

            process_query( query,&raddr.ipaddr );
        }

        THREAD_YIELD( pt );
    }    
    
PT_END( pt );
}

PT_THREAD( service_timer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) || ( service_count() == 0 ) );

        TMR_WAIT( pt, 1000 );
    
        list_node_t ln = service_list.head;

        while( ln > 0 ){

            service_state_t *service = (service_state_t *)list_vp_get_data( ln );

            // increment uptimes
            if( service->state == STATE_SERVER ){
                    
                service->timeout = 0;
                service->local_uptime++;

                clear_tracking( service );
            }
            else if( !ip_b_is_zeroes( service->server_ip ) ){

                service->server_uptime++;    
            }
            
            if( service->state != STATE_SERVER ){

                service->timeout--;
            }

            // PRE-TIMEOUT LOGIC
            if( service->timeout > 0 ){

                // check if we are connected
                if( service->state == STATE_CONNECTED ){

                    // check timeout and see if we need to ping
                    if( service->timeout < SERVICE_CONNECTED_PING_THRESHOLD ){

                        transmit_query( service );
                    }
                }
            }
            // TIMEOUT STATE MACHINE
            else if( service->state == STATE_LISTEN ){

                // log_v_debug_P( PSTR("LISTEN timeout") );

                // check if we can only be a follower
                if( service->local_priority == SERVICE_PRIORITY_FOLLOWER_ONLY ){

                    // do we have a leader?
                    if( service->server_valid && !ip_b_is_zeroes( service->server_ip ) ){

                        log_v_info_P( PSTR("-> CONNECTED to: %d.%d.%d.%d"), service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );
                        service->state     = STATE_CONNECTED;   
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;                     
                    }
                    else{

                        // no server, reset
                        reset_state( service );
                    }
                }
                // elible to become server
                else{

                    // compare us to best server we've seen
                    if( compare_self( service ) ){

                        log_v_info_P( PSTR("-> SERVER") );
                        service->state = STATE_SERVER;
                    }
                    // if tracking a leader
                    else if( service->server_valid ){

                        // found a leader
                        log_v_info_P( PSTR("-> CONNECTED to: %d.%d.%d.%d"), service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );
                        service->state     = STATE_CONNECTED;
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;                
                    }
                    else{

                        // no leader, reset state
                        reset_state( service );

                        // we completely reset the state including tracking.
                        // this is in case the tracked node disappears.
                    }
                }
            }
            else if( service->state == STATE_CONNECTED ){                    

                log_v_info_P( PSTR("CONNECTED timeout: lost server") );

                reset_state( service );
            }
            
            ln = list_ln_next( ln );
        }
    }    
    
PT_END( pt );
}


void service_v_handle_shutdown( ip_addr4_t ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // look for any elections with this IP as a leader, and reset them

    list_node_t ln = service_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        // if we are server, then obviously we are not shutting down
        // this routine is for other nodes sending their shutdown message
        if( service->state == STATE_SERVER ){

            goto next;
        }

        if( ip_b_addr_compare( service->server_ip, ip ) ){

            log_v_debug_P( PSTR("server shutdown %d.%d.%d.%d"), ip.ip3, ip.ip2, ip.ip1, ip.ip0 );

            reset_state( service );
        }

next:
        ln = next_ln;
    }
}

#endif
