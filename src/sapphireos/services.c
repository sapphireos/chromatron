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

/*

Possible improvements?


- firstly - figure out if this module is behaving correctly,
Re: weird time desync behavior usually on power outages.


Run each service in its own thread?
Might make state management much simpler, and this
ptotocol has a ton of state.


*/

#include "system.h"
#include "wifi.h"
#include "config.h"
#include "timers.h"
#include "threading.h"
#include "random.h"
#include "list.h"
#include "hash.h"

#include "services.h"

// #define TEST_MODE

#if defined(ENABLE_NETWORK) && defined(ENABLE_SERVICES)


#ifdef TEST_MODE
static uint8_t test_service_priority;
static uint8_t test_service_mode;

KV_SECTION_META kv_meta_t services_test_kv[] = {
    { CATBUS_TYPE_UINT8,  0, 0, &test_service_priority,   0,  "test_service_priority" },
    { CATBUS_TYPE_UINT8,  0, 0, &test_service_mode,       0,  "test_service_mode" },
};


#endif

// #define NO_LOGGING
#include "logging.h"

#define STATE_LISTEN        0
#define STATE_CONNECTED     1
#define STATE_SERVER        2

typedef struct __attribute__((packed)){
    uint32_t id;
    uint64_t group;   

    uint64_t server_origin;
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

#define SERVICE_FILE "services"

typedef struct __attribute__((packed)){
    uint32_t id;
    uint64_t group;
    sock_addr_t addr;
    uint32_t hash;
} service_cache_entry_t;

static socket_t sock;

static list_t service_list;

PT_THREAD( service_sender_thread( pt_t *pt, void *state ) );
PT_THREAD( service_server_thread( pt_t *pt, void *state ) );
PT_THREAD( service_timer_thread( pt_t *pt, void *state ) );

static bool compare_self( service_state_t *service );

static void clear_tracking( service_state_t *service ){

    service->server_priority       = 0;
    service->server_uptime         = 0;
    service->server_port           = 0;
    service->server_ip             = ip_a_addr(0,0,0,0);
    service->server_origin         = 0;
    service->server_valid          = FALSE;
}

static void _reset_state( service_state_t *service ){

    service->state          = STATE_LISTEN;  
    service->timeout        = SERVICE_LISTEN_TIMEOUT;
    service->local_uptime   = 0;

    clear_tracking( service );
}

// #define reset_state(a) log_v_debug_P( PSTR("Reset to LISTEN") ); _reset_state( a )
#define reset_state(a) _reset_state( a )

static service_state_t* get_service( uint32_t id, uint64_t group ){

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

static bool search_cached_service( file_t f, uint32_t id, uint64_t group, sock_addr_t *raddr ){
    
    ASSERT( f > 0 );

    service_cache_entry_t entry;

    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.id != id ){

            continue;
        }

        if( entry.group != group ){

            continue;
        }

        if( entry.hash != hash_u32_data( (uint8_t *)&entry, sizeof(entry) - sizeof(uint32_t) ) ){

            continue;
        }

        // match!

        if( raddr != 0 ){

            *raddr = entry.addr;
        }

        return TRUE;
    }

    return FALSE;
}

static bool get_cached_service( uint32_t id, uint64_t group, sock_addr_t *raddr ){

    file_t f = fs_f_open_P( PSTR(SERVICE_FILE), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return FALSE;
    }

    bool found = search_cached_service( f, id, group, raddr );

    f = fs_f_close( f );

    return found;
}

static void cache_service( uint32_t id, uint64_t group, ip_addr4_t ip, uint16_t port ){

    file_t f = fs_f_open_P( PSTR(SERVICE_FILE), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return;
    }

    if( ip_b_is_zeroes( ip ) ){

        // use our IP address
        cfg_i8_get( CFG_PARAM_IP_ADDRESS, &ip );
    }

    // setup entry
    sock_addr_t raddr = {
        ip,
        port
    };

    service_cache_entry_t entry = {
        id,
        group,
        raddr,
    };

    entry.hash = hash_u32_data( (uint8_t *)&entry, sizeof(entry) - sizeof(uint32_t) );

    if( search_cached_service( f, id, group, 0 ) ){

        // update existing entry

        // rewind file pointer
        fs_v_seek( f, fs_i32_tell( f ) - sizeof(entry) );

        fs_i16_write( f, &entry, sizeof(entry) );
    }
    else{

        // search for empty entry
        fs_v_seek( f, 0 );

        if( search_cached_service( f, 0, 0, 0 ) ){

            // rewind and overwrite
            fs_v_seek( f, fs_i32_tell( f ) - sizeof(entry) );

            fs_i16_write( f, &entry, sizeof(entry) );
        }
        else{

            // no empty slots, write to end of file
            fs_i16_write( f, &entry, sizeof(entry) );
        }
    }

    f = fs_f_close( f );

    // log_v_debug_P( PSTR( "cached service") );
}

static void delete_cached_service( uint32_t id, uint64_t group ){

    file_t f = fs_f_open_P( PSTR(SERVICE_FILE), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return;
    }

    service_cache_entry_t entry;

    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.id != id ){

            continue;
        }

        if( entry.group != group ){

            continue;
        }

        // match!

        memset( &entry, 0, sizeof(entry) );

        // rewind file
        fs_v_seek( f, fs_i32_tell( f ) - sizeof(entry) );

        // overwrite with 0s
        fs_i16_write( f, &entry, sizeof(entry) );

        // log_v_debug_P( PSTR("deleting cached service") );

        break;
    }

    
    f = fs_f_close( f );

    return;
}


static void load_cached_service( service_state_t *service ){

    if( !wifi_b_connected() ){

        return;
    }

    // log_v_debug_P( PSTR("Loading cached services") );

    sock_addr_t raddr;
    if( get_cached_service( service->id, service->group, &raddr ) ){

        service->server_ip = raddr.ipaddr;
        service->server_port = raddr.port;

        ip_addr4_t ip;
        cfg_i8_get( CFG_PARAM_IP_ADDRESS, &ip );

        if( ip_b_addr_compare( ip, service->server_ip ) ){

            service->state = STATE_SERVER;

            // log_v_debug_P( PSTR("loaded cached service: SERVER") );
        }
        else{

            service->state = STATE_CONNECTED;
            service->timeout = SERVICE_CONNECTED_PING_THRESHOLD;

            // log_v_debug_P( PSTR("loaded cached service: CONNECTED") );
        }
    }
}


static uint32_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

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


#ifdef TEST_MODE

PT_THREAD( service_test_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        if( test_service_mode == 0 ){

            services_v_cancel( 1234, 5678 );
        }

        THREAD_WAIT_WHILE( pt, test_service_mode == 0 );

        if( test_service_mode == 1 ){

            services_v_offer( 1234, 5678, test_service_priority, 1000 );

            THREAD_WAIT_WHILE( pt, test_service_mode == 1 );
        }
        else if( test_service_mode == 2 ){

            services_v_listen( 1234, 5678 );

            THREAD_WAIT_WHILE( pt, test_service_mode == 2 );
        }
        else if( test_service_mode == 3 ){

            services_v_join_team( 1234, 5678, test_service_priority, 1000 );

            THREAD_WAIT_WHILE( pt, test_service_mode == 3 );
        }
        else{

            test_service_mode = 0;
        }
    }    
    
PT_END( pt );
}


#endif

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

    #ifdef TEST_MODE
    thread_t_create( service_test_thread,
                     PSTR("service_test"),
                     0,
                     0 );

    #endif

    // uint8_t test = 0;
    // kv_i8_get( __KV__test_services, &test, sizeof(test) );

    // if( test == 1 ){

    //     log_v_debug_P( PSTR("services test mode enabled: OFFER") );

    //     services_v_offer(1234, 5678, 1, 1);     
    // }
    // else if( test == 2 ){

    //     log_v_debug_P( PSTR("services test mode enabled: LISTEN") );

    //     services_v_listen(1234, 5678);     
    // }
    // else if( test == 3 ){

    //     log_v_debug_P( PSTR("services test mode enabled: JOIN") );

    //     services_v_join_team(1234, 5678, 1, 1);     
    // }

    // debug
    // if( cfg_u64_get_device_id() == 93172270997720 ){

    //     // services_v_join_team( 0x1234, 0, 11, 1000 );

    //     services_v_offer(1234, 5678, 1, 0);
    // }
    // else{

    //     // services_v_join_team( 0x1234, 0, 10, 1000 );

    //     services_v_listen(1234, 5678);
    // }
}


void services_v_listen( uint32_t id, uint64_t group ){

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

    delete_cached_service( id, group );
}

void services_v_offer( uint32_t id, uint64_t group, uint16_t priority, uint16_t port ){

    ASSERT( priority > 0 );
    
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    services_v_cancel( id, group );

    service_state_t service = {0};
    service.id                  = id;
    service.group               = group;
    service.is_team             = FALSE;
    service.local_priority      = priority;
    service.local_port          = port;

    reset_state( &service );

    // offers become server automatically
    service.state = STATE_SERVER;

    list_node_t ln = list_ln_create_node2( &service, sizeof(service), MEM_TYPE_SERVICE );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &service_list, ln );   
     
    delete_cached_service( id, group );
}

void services_v_join_team( uint32_t id, uint64_t group, uint16_t priority, uint16_t port ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    service_state_t *svc_ptr = get_service( id, group );

    // check if service already registered
    if( svc_ptr != 0 ){

        // is port changing?
        if( port != svc_ptr->local_port ){

            // reset entire service in this case (fall through)
        }
        // is priority changing?
        else if( priority != svc_ptr->local_priority ){

            // update priority
            svc_ptr->local_priority = priority;

            // if we are connected to a server, compare and see if we make a better server
            if( svc_ptr->state == STATE_CONNECTED ){

                if( compare_self( svc_ptr ) ){

                    log_v_debug_P( PSTR("switch to server, better priority") );

                    log_v_info_P( PSTR("-> SERVER") );
                    svc_ptr->state = STATE_SERVER;
                }

                return;
            }
        }
        else{

            // neither are changing
            return;
        }

        // reset service (falling out of this if case)
        services_v_cancel( id, group );
    }
    else{

        // log_v_debug_P( PSTR("join: %lx %lx priority: %d"), id, group, priority );
    }

    service_state_t service = {0};
    service.id                  = id;
    service.group               = group;
    service.is_team             = TRUE;
    service.local_priority      = priority;
    service.local_port          = port;

    reset_state( &service );

    load_cached_service( &service );

    list_node_t ln = list_ln_create_node2( &service, sizeof(service), MEM_TYPE_SERVICE );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &service_list, ln );    
}

void services_v_cancel( uint32_t id, uint64_t group ){
    
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

            // note: don't delete the cached service here.  just because we
            // are cancelling doesn't mean we won't re-enable this service later.

            return;
        }

        ln = next_ln;
    }
}

bool services_b_is_available( uint32_t id, uint64_t group ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return FALSE;
    }

    // check that wifi is connected
    if( !wifi_b_connected() ){

        return FALSE;
    }

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return FALSE;
    }

    if( service->state != STATE_LISTEN ){

        return TRUE;
    }

    return FALSE;
}

bool services_b_is_server( uint32_t id, uint64_t group ){

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

sock_addr_t services_a_get( uint32_t id, uint64_t group ){

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

ip_addr4_t services_a_get_ip( uint32_t id, uint64_t group ){

    sock_addr_t addr = services_a_get( id, group );

    return addr.ipaddr;
}

uint16_t services_u16_get_port( uint32_t id, uint64_t group ){

    sock_addr_t addr = services_a_get( id, group );

    return addr.port;
}


uint16_t services_u16_get_leader_priority( uint32_t id, uint64_t group ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    service_state_t *service = get_service( id, group );

    if( service == 0 ){

        return 0;
    }

    if( service->state == STATE_LISTEN ){

        return 0;
    }

    return service->server_priority;
}

static void init_header( service_msg_header_t *header, uint8_t type ){

    header->magic       = SERVICES_MAGIC;
    header->version     = SERVICES_VERSION;
    header->type        = type;
    header->flags       = 0;
    header->reserved    = 0;
    header->origin      = catbus_u64_get_origin_id();
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

    if( sys_b_is_shutting_down() ){

        msg_header->flags |= SERVICE_FLAGS_SHUTDOWN;
    }

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
    
        raddr.ipaddr = ip_a_addr(SERVICES_MCAST_ADDR);    
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

        raddr.ipaddr = ip_a_addr(SERVICES_MCAST_ADDR);    
    }
    else{

        raddr.ipaddr = service->server_ip;
    }

    raddr.port   = SERVICES_PORT;

    // log_v_debug_P( PSTR("tx query %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

    sock_i16_sendto_m( sock, h, &raddr );
}


PT_THREAD( service_sender_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) || ( transmit_count() == 0 ) );

    // load cached services
    list_node_t ln = service_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        load_cached_service( service );

        ln = next_ln;
    }    

    TMR_WAIT( pt, rnd_u16_get_int() >> 6 ); // add 1 second of random delay

    // initial broadcast offer
    transmit_service( 0, 0 );

    TMR_WAIT( pt, 500 );
    
    // initial broadcast query

    ln = service_list.head;

    while( ln > 0 ){

        list_node_t next_ln = list_ln_next( ln );

        service_state_t *service = (service_state_t *)list_vp_get_data( ln );

        transmit_query( service );

        ln = next_ln;
    }

    while(1){
        
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + SERVICE_RATE );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );

        if( sys_b_is_shutting_down() ){

            goto shutdown;
        }

        THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) || ( transmit_count() == 0 ) );

        TMR_WAIT( pt, rnd_u16_get_int() >> 6 ); // add 1 second of random delay

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            goto shutdown;
        }

        transmit_service( 0, 0 );
    }    

shutdown:

    transmit_service( 0, 0 );
    TMR_WAIT( pt, 100 );
    transmit_service( 0, 0 );
    TMR_WAIT( pt, 100 );
    transmit_service( 0, 0 );
    
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

            // if( service->local_uptime > 0 ){

                // log_v_debug_P( PSTR("older: %lu %lu"), (uint32_t)service->server_uptime, (uint32_t)service->local_uptime );
            // }

            // log_v_debug_P( PSTR("uptime ***") );

            return FALSE;
        }
    }

    // uptime is the same (within SERVICE_UPTIME_MIN_DIFF) or server is not valid

    // compare origins
    if( catbus_u64_get_origin_id() < service->server_origin ){

        log_v_debug_P( PSTR("origin priority") );

        return TRUE;
    }

    return FALSE;
}

// true if pkt is better than current leader
static bool compare_server( service_state_t *service, service_msg_offer_hdr_t *header, service_msg_offer_t *offer, uint64_t origin ){

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
    
    // check uptimes

    uint32_t our_uptime = service->server_uptime;

    // if we are a server, use our local uptime, not the tracked server uptime
    if( service->state == STATE_SERVER ){

        our_uptime = service->local_uptime;
    }

    // check cycle count
    int64_t diff = (int64_t)offer->uptime - (int64_t)our_uptime;

    if( diff > SERVICE_UPTIME_MIN_DIFF ){

        log_v_debug_P( PSTR("uptime newer: %lu %lu"), (uint32_t)our_uptime, (uint32_t)offer->uptime );

        return TRUE;
    }
    else if( diff < ( -1 * SERVICE_UPTIME_MIN_DIFF ) ){

        // log_v_debug_P( PSTR("older: %lu %lu"), (uint32_t)our_uptime, (uint32_t)offer->uptime );

        log_v_debug_P( PSTR("uptime ***") );

        return FALSE;
    }

    // uptime is the same

    // compare  origins
    if( origin < service->server_origin ){

        log_v_debug_P( PSTR("origin priority") );

        return TRUE;
    }
    
    return FALSE;
}

static void track_node( service_state_t *service, service_msg_offer_hdr_t *header, service_msg_offer_t *offer, ip_addr4_t *ip, uint64_t origin_id ){

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
    service->server_origin     = origin_id;
}

static void process_offer( service_msg_offer_hdr_t *header, service_msg_offer_t *pkt, ip_addr4_t *ip, uint64_t origin ){

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
            else if( compare_server( service, header, pkt, origin ) ){

                track_node( service, header, pkt, ip, origin );

                log_v_debug_P( PSTR("service switched to %d.%d.%d.%d for %x"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, service->id );
            }
        }
        // NOTE servers have no processing here - they don't track other offers.
    }
    // TEAM:
    // are we already tracking this node?
    // if so, just update the tracking info
    else if( ( service->state != STATE_SERVER ) &&
             ip_b_addr_compare( *ip, service->server_ip ) ){

        bool valid = service->server_valid;

        track_node( service, header, pkt, ip, origin );

        if( !valid && service->server_valid ){

            log_v_debug_P( PSTR("%d.%d.%d.%d is now valid for %x"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, service->id );            
        }

        // are we connected to this node?
        if( service->state == STATE_CONNECTED ){

            // reset timeout
            service->timeout   = SERVICE_CONNECTED_TIMEOUT;

            // did the server reboot and we didn't notice?
            // OR did the server change to invalid?
            if( ( ( (int64_t)service->server_uptime - (int64_t)pkt->uptime ) > SERVICE_UPTIME_MIN_DIFF ) ||
                ( ( pkt->flags & SERVICE_OFFER_FLAGS_SERVER ) == 0 ) ){

                // reset, maybe there is a better server available

                log_v_debug_P( PSTR("%d.%d.%d.%d is no longer valid for %x"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, service->id );

                reset_state( service );

                delete_cached_service( service->id, service->group );
            }
            // check if server is still better than we are
            else if( compare_self( service ) ){

                // no, we are better
                log_v_debug_P( PSTR("we are a better server") );

                log_v_info_P( PSTR("%x -> SERVER"), service->id );
                service->state = STATE_SERVER;

                cache_service( service->id, service->group, ip_a_addr(0,0,0,0), service->local_port );
            }
        }
    }
    // TEAM state machine
    else{

        // log_v_debug_P( PSTR("offer from %d.%d.%d.%d flags: 0x%02x pri: %d"), ip->ip3, ip->ip2, ip->ip1, ip->ip0, pkt->flags, pkt->priority );

        if( service->state == STATE_LISTEN ){

            // check if server in packet is better than current tracking
            if( compare_server( service, header, pkt, origin ) ){

                log_v_debug_P( PSTR("%x state: LISTEN"), service->id );

                track_node( service, header, pkt, ip, origin );    
            }
        }
        else if( service->state == STATE_CONNECTED ){

            // check if packet is a better server than current tracking (and this packet is not from the current server)
            if( ( service->server_origin != origin ) && 
                compare_server( service, header, pkt, origin ) ){
                
                log_v_debug_P( PSTR("%x state: CONNECTED"), service->id );

                track_node( service, header, pkt, ip, origin );

                // if tracking is a server
                if( service->server_valid ){

                    log_v_debug_P( PSTR("%x hop to better server: %d.%d.%d.%d uptime local: %lu remote: %lu"), 
                        service->id, service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0,
                        service->local_uptime, service->server_uptime );

                    // reset timeout
                    service->timeout   = SERVICE_CONNECTED_TIMEOUT;

                    cache_service( service->id, service->group, service->server_ip, service->server_port );
                }
            }
        }
        else if( service->state == STATE_SERVER ){

            // check if this packet is better than current tracking
            if( compare_server( service, header, pkt, origin ) ){

                // check if server is valid - we will only consider
                // other valid servers, not candidates
                if( ( pkt->flags & SERVICE_OFFER_FLAGS_SERVER ) != 0 ){

                    track_node( service, header, pkt, ip, origin );

                    // now that we've updated tracking
                    // check if the tracked server is better than us
                    if( !compare_self( service ) ){

                        // tracked server is better
                        log_v_debug_P( PSTR("%x found a better server: %d.%d.%d.%d uptime local: %lu remote: %lu"), 
                            service->id, service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0,
                            service->local_uptime, service->server_uptime );

                        log_v_info_P( PSTR("-> CONNECTED") );

                        // reset timeout
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;
                        service->state     = STATE_CONNECTED;

                        cache_service( service->id, service->group, service->server_ip, service->server_port );
                    }
                    else{

                        // we are the better server.
                        // possibly the other server is missing our broadcasts (this can happen)

                        // unicast our offer directly to it
                        transmit_service( service, ip );
                    }
                }
            }
            else{

                // received server is not as good as we are.
                // possibly it is missing our broadcasts (this can happen)

                // unicast our offer directly to it
                transmit_service( service, ip );
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

        // log_v_error_P( PSTR("service not found") );

        return;
    }

    // check state
    if( service->state != STATE_SERVER ){

        // log_v_error_P( PSTR("not server") );

        return;
    }

    transmit_service( service, ip );
}


PT_THREAD( service_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // check service file size
    file_t f = fs_f_open_P( PSTR(SERVICE_FILE), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f > 0 ){

        if( fs_i32_get_size( f ) > SERVICE_MAX_FILE_SIZE ){

            fs_v_delete( f );

            log_v_info_P( PSTR("service cache size exceeded") );
        }

        f = fs_f_close( f );
    }


    THREAD_WAIT_WHILE( pt, service_count() == 0 );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, SERVICES_PORT );

    wifi_i8_igmp_join( ip_a_addr(SERVICES_MCAST_ADDR) );

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

        if( header->origin == 0 ){

            log_v_error_P( PSTR("origin should not be 0: %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

            continue;
        }

        if( header->type == SERVICE_MSG_TYPE_OFFERS ){

            service_msg_offer_hdr_t *offer_hdr = (service_msg_offer_hdr_t *)( header + 1 );
            service_msg_offer_t *offer = (service_msg_offer_t *)( offer_hdr + 1 );

            // check packet length
            uint16_t len = sizeof(service_msg_header_t) + 
                   sizeof(service_msg_offer_hdr_t) + 
                   ( sizeof(service_msg_offer_t) * offer_hdr->count );

            if( sock_i16_get_bytes_read( sock ) != len ){

                log_v_error_P( PSTR("bad size") );
                continue;
            }

            while( offer_hdr->count > 0 ){

                if( header->flags & SERVICE_FLAGS_SHUTDOWN ){

                    service_state_t *service = get_service( offer->id, offer->group );

                    if( service != 0 ){

                        if( service->state != STATE_LISTEN ){

                            log_v_debug_P( PSTR("server shutdown %d.%d.%d.%d:%u service: %0x"), 
                                raddr.ipaddr.ip3, 
                                raddr.ipaddr.ip2, 
                                raddr.ipaddr.ip1, 
                                raddr.ipaddr.ip0,
                                raddr.port,
                                service->id );        
                        }
                        
                        reset_state( service );
                    }
                }
                else{

                    process_offer( offer_hdr, offer, &raddr.ipaddr, header->origin );    
                }

                offer++;
                offer_hdr->count--;
            }
        }
        else if( header->type == SERVICE_MSG_TYPE_QUERY ){

            // log_v_debug_P( PSTR("query from %d.%d.%d.%d"), raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

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
                    if( service->timeout <= SERVICE_CONNECTED_PING_THRESHOLD ){

                        transmit_query( service );
                    }

                    if( service->timeout == SERVICE_CONNECTED_WARN_THRESHOLD ){

                        log_v_warn_P( PSTR("server unresponsive: %x"), service->id );
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

                        log_v_info_P( PSTR("%x -> CONNECTED to: %d.%d.%d.%d"), service->id, service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );
                        service->state     = STATE_CONNECTED;   
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;

                        cache_service( service->id, service->group, service->server_ip, service->server_port );                    
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

                        log_v_info_P( PSTR("%x -> SERVER"), service->id );
                        service->state = STATE_SERVER;

                        cache_service( service->id, service->group, ip_a_addr(0,0,0,0), service->local_port );
                    }
                    // if tracking a leader
                    else if( service->server_valid ){

                        // found a leader
                        log_v_info_P( PSTR("%x -> CONNECTED to: %d.%d.%d.%d"), service->id, service->server_ip.ip3, service->server_ip.ip2, service->server_ip.ip1, service->server_ip.ip0 );
                        service->state     = STATE_CONNECTED;
                        service->timeout   = SERVICE_CONNECTED_TIMEOUT;

                        cache_service( service->id, service->group, service->server_ip, service->server_port );                
                    }
                    else{

                        // no leader, reset state
                        reset_state( service );

                        // we completely reset the state including tracking.
                        // this is in case the tracked node disappears.

                        delete_cached_service( service->id, service->group );
                    }
                }
            }
            else if( service->state == STATE_CONNECTED ){                    

                log_v_info_P( PSTR("%x CONNECTED timeout: lost server"), service->id );

                reset_state( service );

                delete_cached_service( service->id, service->group );
            }
            
            ln = list_ln_next( ln );
        }
    }    
    
PT_END( pt );
}

#endif
