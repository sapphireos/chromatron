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

#include "system.h"
#include "timers.h"
#include "sockets.h"
#include "threading.h"
#include "fs.h"
#include "keyvalue.h"
#include "catbus.h"
#include "list.h"
#include "hash.h"
#include "services.h"
#include "util.h"
#include "config.h"
#include "random.h"
#include "wifi.h"

#include "catbus_link.h"

// #define NO_LOGGING
#include "logging.h"


/*

SYNC:


Synchronize a variable among all nodes in the link group.
Source and dest key are the same.
This requires integration with the FX engine: synced variable
writes are intercepted, only the link leader is passed through.
Link followers update the variable to match the leader, and ignore
local writes from the FX engine.


Sync leverages the send and receive machinery.  It shares attributes with 
both.

A key difference is that all members of the sync group have a copy
of the link.  Thus some shared context is already available.

Sync followers send a consumer match in the discovery process.

The leader will receive the consumer matches, which will create the data
binding.  The sync leader will transmit to consumers at the configured rate.
The transmit_to_consumers function should work without modification.  This
is simliar to the leader on receive.

No aggregation is performed, however, the aggregate function may be used
since it will retrieve the local data item and format it for 
transmission.

The link module must provide an API to check if a given key is
synchronized and if it is the leader.


*/


// #define TEST_MODE


#ifdef ENABLE_CATBUS_LINK

static uint16_t link_process_tick_rate = LINK_MAX_TICK_RATE;

PT_THREAD( link_server_thread( pt_t *pt, void *state ) );
PT_THREAD( link_processor_thread( pt_t *pt, void *state ) );
PT_THREAD( link_discovery_thread( pt_t *pt, void *state ) );

static list_t link_list;
static list_t consumer_list;
static list_t producer_list;
static list_t remote_list;
static socket_t sock;


#ifdef TEST_MODE
static uint8_t test_link_mode;

#define TEST_LINK_TAG __KV__TEST_LINK

KV_SECTION_META kv_meta_t link_test_kv[] = {
    { CATBUS_TYPE_UINT8,  0, 0, &test_link_mode,       0,  "test_link_mode" },
};


#endif


// producer:
// sends data to a link leader
// producers don't necessarily have a copy of the link,
// sender links automatically become producers.
// receiver links query for producers.
typedef struct __attribute__((packed)){
    catbus_hash_t32 source_key;
    uint64_t link_hash;
    sock_addr_t leader_addr;  // supplied via services (send mode) or via message (recv mode)
    uint32_t data_hash;
    link_rate_t16 rate;
    int16_t ticks;
    int16_t retransmit_timer;
    int32_t timeout;
} producer_state_t;

// remote producer:
// producer state tracked by the leader
typedef struct __attribute__((packed)){
    uint64_t link_hash;
    sock_addr_t addr;
    uint16_t padding; // padding to ensure data is 32 bit aligned.  the ESP8266 will crash if it isn't.
    int32_t timeout;
    catbus_data_t data;
    // variable length data follows
} remote_state_t;

// consumer:
// stored on leader, tracks consumers to transmit data to
typedef struct __attribute__((packed)){
    uint64_t link_hash;
    sock_addr_t addr;
    int32_t timeout;
} consumer_state_t;


#define LINK_FILE "links"

static int32_t link_test_key;
static int32_t link_test_key2;

KV_SECTION_META kv_meta_t link_kv[] = {
    { CATBUS_TYPE_INT32,   0, 0,                   &link_test_key,        0,  "link_test_key" },
    { CATBUS_TYPE_INT32,   0, 0,                   &link_test_key2,       0,  "link_test_key2" },
};

PT_THREAD( test_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 1000 );

        link_test_key++;
    }

PT_END( pt );
}


static uint32_t link_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &link_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &link_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

static uint32_t consumer_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &consumer_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &consumer_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

static uint32_t producer_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &producer_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &producer_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

static uint32_t remote_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &remote_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &remote_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}


#ifdef TEST_MODE

PT_THREAD( link_test_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        if( test_link_mode == 0 ){

            link_v_delete_by_tag( TEST_LINK_TAG );
            link_test_key = 0;
            link_test_key2 = 0;
        }

        THREAD_WAIT_WHILE( pt, test_link_mode == 0 );

        if( test_link_mode == 1 ){

            while( test_link_mode == 1 ){

                link_test_key++;

                TMR_WAIT( pt, 100 );
            }
        }
        else if( test_link_mode == 2 ){

            catbus_query_t query;
            memset( &query, 0, sizeof(query) );
            query.tags[0] = __KV____TEST__;            

            link_l_create(
                LINK_MODE_SEND,
                __KV__link_test_key,
                __KV__link_test_key2,
                &query,
                TEST_LINK_TAG,
                100,
                LINK_AGG_ANY,
                LINK_FILTER_OFF );

            THREAD_WAIT_WHILE( pt, test_link_mode == 2 );
        }
        else if( test_link_mode == 3 ){

            THREAD_WAIT_WHILE( pt, test_link_mode == 3 );
        }
        else if( test_link_mode == 4 ){

            catbus_query_t query;
            memset( &query, 0, sizeof(query) );
            query.tags[0] = __KV____TEST__;            

            link_l_create(
                LINK_MODE_RECV,
                __KV__link_test_key,
                __KV__link_test_key2,
                &query,
                TEST_LINK_TAG,
                100,
                LINK_AGG_ANY,
                LINK_FILTER_OFF );

            THREAD_WAIT_WHILE( pt, test_link_mode == 4 );
        }
        else if( test_link_mode == 5 ){

            THREAD_WAIT_WHILE( pt, test_link_mode == 5 );
        }
        else{

            test_link_mode = 0;
        }
    }    
    
PT_END( pt );
}


#endif

void link_v_init( void )
{
	list_v_init( &link_list );
    list_v_init( &consumer_list );
    list_v_init( &producer_list );
    list_v_init( &remote_list );

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

    // create vfiles
    fs_f_create_virtual( PSTR("link_info"), link_vfile );
    fs_f_create_virtual( PSTR("link_consumers"), consumer_vfile );
    fs_f_create_virtual( PSTR("link_producers"), producer_vfile );
    fs_f_create_virtual( PSTR("link_remotes"), remote_vfile );


    thread_t_create( link_server_thread,
                 PSTR("link_server"),
                 0,
                 0 );

    thread_t_create( link_processor_thread,
                 PSTR("link_processor"),
                 0,
                 0 );

    thread_t_create( link_discovery_thread,
                 PSTR("link_discovery"),
                 0,
                 0 );

    wifi_i8_igmp_join( ip_a_addr(LINK_MCAST_ADDR) );


    #ifdef TEST_MODE

    log_v_info_P( PSTR("LINK TEST MODE ENABLED") );

    thread_t_create( link_test_thread,
                 PSTR("link_test"),
                 0,
                 0 );

    #endif

    // uint8_t test = 0;
    // kv_i8_get( __KV__test_link, &test, sizeof(test) );

    // if( test != 0 ){

    //     catbus_query_t query;
    //     memset( &query, 0, sizeof(query) );
    //     query.tags[0] = __KV____TEST__;


    //     if( test == 1 ){

    //         log_v_debug_P( PSTR("link test mode enabled: idle") );
    //     }
    //     else if( test == 2 ){

    //         log_v_debug_P( PSTR("link test mode enabled: test thread only") );

    //         thread_t_create( test_thread,
    //                  PSTR("test_thread"),
    //                  0,
    //                  0 );
    //     }
    //     else if( test == 3 ){

    //         log_v_debug_P( PSTR("link test mode enabled: send") );
            
    //         link_l_create( 
    //             LINK_MODE_SEND, 
    //             __KV__link_test_key, // source 
    //             __KV__kv_test_key,   // dest
    //             &query,
    //             __KV__test,
    //             1000,
    //             LINK_AGG_ANY,
    //             LINK_FILTER_OFF );        

    //         thread_t_create( test_thread,
    //                  PSTR("test_thread"),
    //                  0,
    //                  0 );
    //     }
    //     else if( test == 4 ){

    //         log_v_debug_P( PSTR("link test mode enabled: recv") );
            
    //         link_l_create( 
    //             LINK_MODE_RECV, 
    //             __KV__link_test_key, // source
    //             __KV__kv_test_key,   // dest
    //             &query,
    //             __KV__test,
    //             1000,
    //             LINK_AGG_ANY,
    //             LINK_FILTER_OFF );        

    //         thread_t_create( test_thread,
    //                  PSTR("test_thread"),
    //                  0,
    //                  0 );
    //     }
    // }
}

link_state_t link_ls_assemble(
	link_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter ){

	link_state_t state = {
		.mode 				= mode,
		.source_key 		= source_key,
		.dest_key 			= dest_key,
		.query 				= *query,
		.tag 				= tag,
		.rate 			    = rate,
		.aggregation 		= aggregation,
		.filter 			= filter,
	};

	return state;
}

link_state_t* link_ls_get_data( link_handle_t link ){

    return list_vp_get_data( link );    
}

bool link_b_compare( link_state_t *link1, link_state_t *link2 ){
 
	return memcmp( link1, link2, sizeof(link_state_t) ) == 0;
}

uint64_t link_u64_hash( link_state_t *link_state ){

	return hash_u64_data( 
        (uint8_t *)link_state, 
        sizeof(link_state_t) - 
            ( sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int16_t) + sizeof(int16_t) ) 
        );	
}

link_handle_t link_l_lookup( link_state_t *link ){

	list_node_t ln = link_list.head;

	while( ln >= 0 ){

		link_state_t *state = list_vp_get_data( ln );

		if( link_b_compare( link, state ) ){

			return ln;
		}

		ln = list_ln_next( ln );
	}

	return -1;
}

link_handle_t link_l_lookup_by_hash( uint64_t hash ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link_state_t *state = list_vp_get_data( ln );

        if( state->hash == hash ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

typedef struct{
    uint32_t magic;
    uint8_t version;
    uint8_t padding[3];
} link_file_header_t;


typedef struct __attribute__((packed)){
    catbus_hash_t32 source_key;
    catbus_hash_t32 dest_key;
    catbus_query_t query;
    link_mode_t8 mode;
    link_aggregation_t8 aggregation;
    link_filter_t16 filter;
    link_rate_t16 rate;
    catbus_hash_t32 tag;
    
    uint64_t hash; // must be last!
} link_file_entry_t;


#define LINK_FILE_VERSION 1

static file_t open_link_file( void ){

    file_t f;

retry: // yup.  sometimes goto is *exactly* what you need.

    f = fs_f_open_P( PSTR(LINK_FILE), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f < 0 ){

        return f;
    }

    if( fs_i32_get_size( f ) == 0 ){

        // write header
        link_file_header_t header = {
            LINK_MAGIC,
            LINK_FILE_VERSION,
        };

        fs_i16_write( f, &header, sizeof(header) );
    }
    else{

        link_file_header_t header = { 0 };

        // verify header
        fs_i16_read( f, &header, sizeof(header) );

        if( ( header.magic != LINK_MAGIC ) ||
            ( header.version != LINK_FILE_VERSION ) ){

            log_v_warn_P( PSTR("link file corrupted") );

            // file is.... wrong.  delete it and start over.
            fs_v_delete( f );
            fs_f_close( f );

            goto retry;
        }
    }

    return f;
}

static void load_links_from_file( void ){

    file_t f = open_link_file();

    if( f < 0 ){

        return;
    }

    link_file_entry_t entry;

    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.hash == 0 ){
            // empty entry
        }
        // verify hash
        else if( hash_u64_data( (uint8_t *)&entry, sizeof(entry) - sizeof(uint64_t) ) == entry.hash ){

            link_l_create(
                entry.mode,
                entry.source_key,
                entry.dest_key,
                &entry.query,
                entry.tag,
                entry.rate,
                entry.aggregation,
                entry.filter );   
        }
        else{

            log_v_warn_P( PSTR("bad link entry") );
        }
    }

    fs_f_close( f );

    return;
}

static bool search_link_in_file( file_t f, uint64_t hash ){

    link_file_entry_t entry;

    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){
        
        if( entry.hash == hash ){

            return TRUE;
        }
    }

    return FALSE;
}

static void save_link_to_file( link_handle_t link ){

    file_t f = open_link_file();

    if( f < 0 ){

        return;
    }

    link_state_t *link_state = link_ls_get_data( link );

    if( search_link_in_file( f, link_state->hash ) ){

        fs_f_close( f );
        return;
    }

    link_file_entry_t temp_entry;

    // search for empty entry
    while( fs_i16_read( f, &temp_entry, sizeof(temp_entry) ) == sizeof(temp_entry) ){
        
        if( temp_entry.hash == 0 ){

            break;
        }
    }

    link_file_entry_t entry = {
        link_state->source_key,
        link_state->dest_key,
        link_state->query,
        link_state->mode,
        link_state->aggregation,
        link_state->filter,
        link_state->rate,
        link_state->tag,
        link_state->hash,
    };

    fs_i16_write( f, &entry, sizeof(entry) );

    fs_f_close( f );
}

link_handle_t link_l_create( 
    link_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter ){ 

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return -1;
	}

	ASSERT( tag != 0 );

	link_state_t state = link_ls_assemble(
							mode,
							source_key,
							dest_key,
							query,
							tag,
							rate,
							aggregation,
							filter );

    return link_l_create2( &state );
}


link_handle_t link_l_create2( link_state_t *state ){ 

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return -1;
    }

    ASSERT( state->tag != 0 );

    link_handle_t lh = link_l_lookup( state );

    if( lh > 0 ){

        return lh;
    }

    if( list_u8_count( &link_list ) >= LINK_MAX_LINKS ){

        log_v_error_P( PSTR("Too many links") );

        return -1;
    }

    catbus_meta_t meta;

    // check if we have the required keys
    if( state->mode == LINK_MODE_SEND ){

        if( kv_i8_get_catbus_meta( state->source_key, &meta ) < 0 ){
        
            return -1;
        }
    }
    else if( state->mode == LINK_MODE_RECV ){

        if( kv_i8_get_catbus_meta( state->dest_key, &meta ) < 0 ){
        
            return -1;
        }
    }
    else if( state->mode == LINK_MODE_SYNC ){

        if( state->source_key != state->dest_key ){

            return -1;
        }

        if( kv_i8_get_catbus_meta( state->source_key, &meta ) < 0 ){
        
            return -1;
        }

        if( kv_i8_get_catbus_meta( state->dest_key, &meta ) < 0 ){
        
            return -1;
        }

        state->aggregation = LINK_AGG_ANY;
    }
    else{

        log_v_error_P( PSTR("Invalid link mode") );

        return -1;
    }

    // check data type
    // we are only implementing numeric types at this time
    if( type_b_is_string( meta.type ) ){

        log_v_error_P( PSTR("link system does not support strings") );

        return -1;
    }

    // check if array type
    if( meta.count > 0 ){

        // we are only supporting aggregations on scalar types for now.
        // array types can only use the ANY aggregation.
        if( state->aggregation != LINK_AGG_ANY ){

            log_v_error_P( PSTR("link system only supports ANY aggregation for array types") );

            return -1;
        }
    }

    if( state->rate < LINK_RATE_MIN ){

        state->rate = LINK_RATE_MIN;
    }
    else if( state->rate > LINK_RATE_MAX ){

        state->rate = LINK_RATE_MAX;
    }

    // sort tags from highest to lowest.
    // this is because the tags are an AND logic, so the order doesn't matter.
    // however, when computing the hash, the order WILL matter, so this ensures
    // we maintain the AND logic when constructing the link and that the hashes
    // will match regardless of the order we input the tags.
    util_v_bubble_sort_reversed_u32( state->query.tags, cnt_of_array(state->query.tags) );

    state->hash = link_u64_hash( state );

    list_node_t ln = list_ln_create_node2( state, sizeof(link_state_t), MEM_TYPE_CATBUS_LINK );

    if( ln < 0 ){

        return -1;
    }

    services_v_join_team( LINK_SERVICE, state->hash, LINK_BASE_PRIORITY - link_u8_count(), LINK_PORT );

    list_v_insert_tail( &link_list, ln );    

    // if( state->mode == LINK_MODE_SEND ){
    //     trace_printf("SEND LINK\n");
    // }
    // else if( state->mode == LINK_MODE_RECV ){
    //     trace_printf("RECV LINK\n");
    // }

    return ln;
}

uint8_t link_u8_count( void ){

    return list_u8_count( &link_list );
}

uint8_t producer_count( void ){

    return list_u8_count( &producer_list );
}

uint8_t remote_count( void ){

    return list_u8_count( &remote_list );
}

uint8_t consumer_count( void ){

    return list_u8_count( &consumer_list );
}

uint8_t object_count( void ){

    return producer_count() + remote_count() + consumer_count();
}

static void delete_link( link_handle_t link ){

    link_state_t *state = list_vp_get_data( link );

    services_v_cancel( LINK_SERVICE, link_u64_hash( state ) );
    
    list_node_t ln = consumer_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        consumer_state_t *consumer = list_vp_get_data( ln );

        if( consumer->link_hash == state->hash ){

            // remove consumer
            list_v_remove( &consumer_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }    

    list_v_remove( &link_list, link );
    list_v_release_node( link );
}

void link_v_delete_by_tag( catbus_hash_t32 tag ){
	
	list_node_t ln = link_list.head;

	while( ln >= 0 ){

		link_state_t *state = list_vp_get_data( ln );
		list_node_t next_ln = list_ln_next( ln );

		if( state->tag == tag ){

            delete_link( ln );
		}

		ln = next_ln;
	}

    // delete links in file
    file_t f = open_link_file();

    if( f < 0 ){

        return;
    }

    link_file_entry_t entry;
    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.tag == tag ){

            // rewind and overwrite
            fs_v_seek( f, fs_i32_tell( f ) - sizeof(entry) );
            memset( &entry, 0, sizeof(entry) );
            fs_i16_write( f, &entry, sizeof(entry) );
        }
    }

    fs_f_close( f );
}

void link_v_delete_by_hash( uint64_t hash ){
    
    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link_state_t *state = list_vp_get_data( ln );
        list_node_t next_ln = list_ln_next( ln );

        if( link_u64_hash( state ) == hash ){

            delete_link( ln );
        }

        ln = next_ln;
    }

    // delete links in file
    file_t f = open_link_file();

    if( f < 0 ){

        return;
    }

    link_file_entry_t entry;
    while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

        if( entry.hash == hash ){

            // rewind and overwrite
            fs_v_seek( f, fs_i32_tell( f ) - sizeof(entry) );
            memset( &entry, 0, sizeof(entry) );
            fs_i16_write( f, &entry, sizeof(entry) );
        }
    }

    fs_f_close( f );
}



static link_handle_t _link_l_lookup_sync( catbus_hash_t32 key ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link_state_t *state = list_vp_get_data( ln );

        if( ( state->source_key == key ) &&
            ( state->mode == LINK_MODE_SYNC ) ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

bool link_b_is_synced( catbus_hash_t32 key ){

    link_handle_t link = _link_l_lookup_sync( key );

    if( link < 0 ){

        return FALSE;
    }

    link_state_t *link_state = list_vp_get_data( link );

    return services_b_is_available( LINK_SERVICE, link_state->hash );
}

bool link_b_is_synced_leader( catbus_hash_t32 key ){

    link_handle_t link = _link_l_lookup_sync( key );

    if( link < 0 ){

        return FALSE;
    }

    link_state_t *link_state = list_vp_get_data( link );

    return services_b_is_server( LINK_SERVICE, link_state->hash );
}

bool link_b_is_synced_follower( catbus_hash_t32 key ){

    link_handle_t link = _link_l_lookup_sync( key );

    if( link < 0 ){

        return FALSE;
    }

    link_state_t *link_state = list_vp_get_data( link );

    return services_b_is_available( LINK_SERVICE, link_state->hash ) &&
           !services_b_is_server( LINK_SERVICE, link_state->hash );
}


/***********************************************
                LINK PROCESSING
***********************************************/

static void init_header( link_msg_header_t *header, uint8_t msg_type ){

    header->magic       = LINK_MAGIC;
    header->msg_type    = msg_type;
    header->version     = LINK_VERSION;
    header->flags       = 0;
    header->reserved    = 0;
    header->origin_id   = catbus_u64_get_origin_id();
    header->universe    = 0;
}

static void transmit_consumer_query( link_state_t *link ){

    link_msg_consumer_query_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_CONSUMER_QUERY );

    sock_addr_t raddr = {
        .ipaddr = ip_a_addr(LINK_MCAST_ADDR),
        .port = LINK_PORT
    };

    msg.key     = link->dest_key;
    msg.query   = link->query;
    msg.hash    = link->hash;
    msg.mode    = link->mode;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

    // trace_printf("LINK: %s()\n", __FUNCTION__);
}

static void transmit_producer_query( link_state_t *link ){

    link_msg_producer_query_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_PRODUCER_QUERY );

    sock_addr_t raddr = {
        .ipaddr = ip_a_addr(LINK_MCAST_ADDR),
        .port = LINK_PORT
    };

    ASSERT( link->mode == LINK_MODE_RECV );

    msg.key     = link->source_key;
    msg.query   = link->query;
    msg.rate    = link->rate;
    msg.hash    = link->hash;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

    // trace_printf("LINK: %s()\n", __FUNCTION__);
}

static void transmit_consumer_match( uint64_t hash, sock_addr_t *raddr ){

    // this function assumes the destination is cached in the socket raddr

    link_msg_consumer_match_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_CONSUMER_MATCH );

    msg.hash    = hash;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );

    // trace_printf("LINK: %s()\n", __FUNCTION__);
}

static bool is_link_leader( link_handle_t link ){

    link_state_t *link_state = list_vp_get_data( link );

    return services_b_is_server( LINK_SERVICE, link_state->hash );
}

static void update_consumer( uint64_t hash, sock_addr_t *raddr ){
    
    list_node_t ln = consumer_list.head;

    while( ln >= 0 ){

        consumer_state_t *consumer = list_vp_get_data( ln );

        if( consumer->link_hash != hash ){

            goto next;
        }

        if( !sock_b_addr_compare( raddr, &consumer->addr ) ){

            goto next;
        }

        // trace_printf("LINK: refreshed consumer timeout\n");

        // update timeout and return
        consumer->timeout = LINK_CONSUMER_TIMEOUT;

        return;
        
    next:
        ln = list_ln_next( ln );
    }

    // check object count
    if( object_count() >= LINK_MAX_OBJECTS ){

        log_v_warn_P( PSTR("object limit exceeded") );

        return;
    }

    // consumer was not found, create one
    link_handle_t link = link_l_lookup_by_hash( hash );

    // make sure link was found!
    if( link < 0 ){

        trace_printf("LINK: link not found!\n");

        // TODO: is this right?
        // we are getting consumer leaks.

    }

    consumer_state_t new_consumer = {
        hash,
        *raddr,
        LINK_CONSUMER_TIMEOUT
    };

    ln = list_ln_create_node2( &new_consumer, sizeof(new_consumer), MEM_TYPE_LINK_CONSUMER );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &consumer_list, ln );

    trace_printf("LINK: added consumer\n");
}

static void process_consumer_timeouts( uint32_t elapsed_ms ){

    list_node_t ln = consumer_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        consumer_state_t *consumer = list_vp_get_data( ln );

        consumer->timeout -= elapsed_ms;

        link_handle_t link = link_l_lookup_by_hash( consumer->link_hash );

        if( link < 0 ){

            log_v_error_P( PSTR("error") );

            // TODO:
            // this causes leaks, consumers with no links will not time out.

            goto next;
        }

        // if timeout expires, or we are not link leader
        if( ( consumer->timeout < 0 ) || ( !is_link_leader( link ) ) ){

            // remove consumer
            list_v_remove( &consumer_list, ln );
            list_v_release_node( ln );

            trace_printf("LINK: pruned consumer for timeout\n");
        }

    next:
        ln = next_ln;
    }
}


static void process_producer_timeouts( uint32_t elapsed_ms ){

    list_node_t ln = producer_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        producer_state_t *producer = list_vp_get_data( ln );

        producer->timeout -= elapsed_ms;

        // if timeout expires
        if( producer->timeout < 0 ){

            // remove producer
            list_v_remove( &producer_list, ln );
            list_v_release_node( ln );

            trace_printf("LINK: pruned producer for timeout\n");
        }

        ln = next_ln;
    }
}

static void process_remote_timeouts( uint32_t elapsed_ms ){

    list_node_t ln = remote_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        remote_state_t *remote = list_vp_get_data( ln );

        remote->timeout -= elapsed_ms;

        // if timeout expires
        if( remote->timeout < 0 ){

            // remove remote
            list_v_remove( &remote_list, ln );
            list_v_release_node( ln );

            trace_printf("LINK: pruned remote for timeout\n");
        }

        ln = next_ln;
    }
}

static void update_remote( sock_addr_t *raddr, link_handle_t link, catbus_data_t *src_data, uint16_t data_len ){

    link_state_t *link_state = link_ls_get_data( link );

    // get dest data len
    catbus_meta_t dest_meta;
    if( kv_i8_get_catbus_meta( link_state->dest_key, &dest_meta ) != 0 ){

        log_v_error_P( PSTR("dest key not found") );

        return;
    }

    uint16_t dest_data_len = type_u16_size( dest_meta.type );
    uint16_t dest_count = dest_meta.count + 1;

    remote_state_t *remote;

    // NOTE data len is checked by server routine,
    // we can assume it is good here.

    list_node_t ln = remote_list.head;

    while( ln >= 0 ){

        remote = list_vp_get_data( ln );
        
        if( remote->link_hash != link_state->hash ){

            goto next;
        }

        if( !sock_b_addr_compare( raddr, &remote->addr ) ){

            goto next;
        }

        // update state
        remote->timeout = LINK_REMOTE_TIMEOUT;

        // trace_printf("LINK: refreshed remote: %d.%d.%d.%d\n",
        //     ip.ip3,
        //     ip.ip2,
        //     ip.ip1,
        //     ip.ip0
        // );

        goto update_data;
        
    next:
        ln = list_ln_next( ln );
    }

    // check object count
    if( object_count() >= LINK_MAX_OBJECTS ){

        log_v_warn_P( PSTR("object limit exceeded") );

        return;
    }

    // remote was not found, create one
    uint16_t remote_len = ( sizeof(remote_state_t) - sizeof(uint8_t) ) + ( dest_data_len * dest_count ); // subtract an extra byte to compensate for the catbus_data_t.data field

    ln = list_ln_create_node2( 0, remote_len, MEM_TYPE_LINK_REMOTE );

    if( ln < 0 ){

        return;
    }

    remote = list_vp_get_data( ln );
    memset( remote, 0, remote_len );
        
    remote->link_hash   = link_state->hash;
    remote->addr        = *raddr;
    remote->timeout     = LINK_REMOTE_TIMEOUT;
    
    // set which key we care about
    catbus_hash_t32 key = 0;
    if( link_state->mode == LINK_MODE_SEND ){

        key = link_state->source_key;
    }
    else if( link_state->mode == LINK_MODE_RECV ){

        key = link_state->dest_key;
    }


    ASSERT( kv_i8_get_catbus_meta( key, &remote->data.meta ) >= 0 );

    list_v_insert_tail( &remote_list, ln );

    trace_printf("LINK: add remote\n");

update_data:
    {}

    uint8_t *dest_ptr = &remote->data.data;
    uint16_t src_data_len = type_u16_size( src_data->meta.type );
    uint8_t *src_ptr = &src_data->data;

    uint16_t src_count = src_data->meta.count + 1;

    if( dest_count > src_count ){

        dest_count = src_count;
    }

    for( uint16_t i = 0; i < dest_count; i++ ){

        if( type_i8_convert( dest_meta.type, dest_ptr, src_data->meta.type, src_ptr, src_data_len ) != 0 ){
            // value changed
        }
    
        src_ptr += src_data_len;
        dest_ptr += dest_data_len;
    }
}

static void update_producer_from_query( link_msg_producer_query_t *msg, sock_addr_t *raddr ){

    list_node_t ln = producer_list.head;

    while( ln >= 0 ){

        producer_state_t *producer = list_vp_get_data( ln );
        
        if( producer->link_hash != msg->hash ){

            goto next;
        }

        // update state
        producer->leader_addr = *raddr;
        producer->timeout = LINK_PRODUCER_TIMEOUT;

        // trace_printf("LINK: refreshed RECV producer: %d.%d.%d.%d\n",
        //     producer->leader_ip.ip3,
        //     producer->leader_ip.ip2,
        //     producer->leader_ip.ip1,
        //     producer->leader_ip.ip0
        // );

        return;
        
    next:
        ln = list_ln_next( ln );
    }

    // check object count
    if( object_count() >= LINK_MAX_OBJECTS ){

        log_v_warn_P( PSTR("object limit exceeded") );

        return;
    }

    // producer was not found, create one

    producer_state_t new_producer = {
        msg->key,
        msg->hash,
        *raddr,
        0,
        msg->rate,
        msg->rate,
        LINK_RETRANSMIT_RATE,
        LINK_PRODUCER_TIMEOUT
    };


    ln = list_ln_create_node2( &new_producer, sizeof(new_producer), MEM_TYPE_LINK_PRODUCER );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &producer_list, ln );

    trace_printf("LINK: became RECV producer\n");
}


PT_THREAD( link_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK_PORT );

    // sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        link_msg_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != LINK_MAGIC ){

            goto end;
        }

        if( header->version != LINK_VERSION ){

            goto end;
        }

        // filter our own messages
        if( header->origin_id == catbus_u64_get_origin_id() ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->msg_type == LINK_MSG_TYPE_CONSUMER_QUERY ){

            // trace_printf("LINK: RX consumer query\n");

            link_msg_consumer_query_t *msg = (link_msg_consumer_query_t *)header;

            if( msg->mode == LINK_MODE_SEND ){

                // check if we have this link, if so, we are part of the send group,
                // not the consumer group, even if we would otherwise match the query.
                // this is a bit of a corner case, but it handles the scenario where
                // a send producer also matches as a consumer and is receiving data
                // it is trying to send.
                if( link_l_lookup_by_hash( msg->hash ) > 0 ){

                    goto end;
                }

                // check query
                if( !catbus_b_query_self( &msg->query ) ){

                    goto end;
                }
            }
            else if( msg->mode == LINK_MODE_RECV ){

                // consumers on a receive link should
                // have the link itself, so we should
                // not be receiving this message at all (receive leaders shouldn't be sending it).
                // the sender is probably confused.
                log_v_error_P( PSTR("receive links should not be sending consumer query") );
                
                goto end;
            }

            #ifdef TEST_MODE
            if( test_link_mode == 0 ){

                if( ( msg->key == __KV__link_test_key ) ||
                    ( msg->key == __KV__link_test_key2 ) ){

                    goto end;
                }

            }
            #endif

            // check key
            if( kv_i16_search_hash( msg->key ) < 0 ){

                goto end;
            }

            // we are a consumer for this link
            
            // transmit response
            transmit_consumer_match( msg->hash, &raddr );
        }
        else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_QUERY ){

            // trace_printf("LINK: RX producer query\n");

            link_msg_producer_query_t *msg = (link_msg_producer_query_t *)header;

            // check query
            if( !catbus_b_query_self( &msg->query ) ){

                goto end;
            }

            // check key
            if( kv_i16_search_hash( msg->key ) < 0 ){

                goto end;
            }

             #ifdef TEST_MODE
            if( test_link_mode == 0 ){

                if( ( msg->key == __KV__link_test_key ) ||
                    ( msg->key == __KV__link_test_key2 ) ){

                    goto end;
                }

            }
            #endif

            // we are a producer for this link

            update_producer_from_query( msg, &raddr );

            // log_v_debug_P("LINK: %s() producer match\n", __FUNCTION__);
            // trace_printf("LINK: %s() producer match\n", __FUNCTION__);
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_MATCH ){

            // trace_printf("LINK: RX consumer match\n");

            link_msg_consumer_match_t *msg = (link_msg_consumer_match_t *)header;

            // received a match
            update_consumer( msg->hash, &raddr );
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_DATA ){

            // trace_printf("LINK: RX consumer DATA\n");

            link_msg_data_t *msg = (link_msg_data_t *)header;

            catbus_meta_t meta;

            if( kv_i8_get_catbus_meta( msg->hash, &meta ) < 0 ){

                log_v_error_P( PSTR("rx hash 0x%08x not found!"), msg->hash );

                goto end;
            }

            // if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

            //     log_v_error_P( PSTR("rx meta does not match!") );

            //     goto end;
            // }

            // verify data lengths
            uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
            uint16_t array_len = meta.count + 1;
            // uint16_t type_len = type_u16_size( meta.type );
            // uint16_t data_len = array_len * type_len;

            // if( data_len != msg_data_len ){

            //     log_v_error_P( PSTR("rx len does not match!") );

            //     goto end;
            // }

            if( catbus_i8_array_set( msg->hash, msg->data.meta.type, 0, array_len, &msg->data.data, msg_data_len ) < 0 ){

                log_v_error_P( PSTR("data fail: 0c%08x"), msg->hash );

                goto end;
            }
        }
        else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_DATA ){

            // trace_printf("LINK: RX producer DATA\n");

            link_msg_data_t *msg = (link_msg_data_t *)header;

            // get link
            link_handle_t link = link_l_lookup_by_hash( msg->hash );

            if( link < 0 ){

                log_v_error_P( PSTR("link not found!") );

                goto end;
            }

            // are we leader?
            if( !is_link_leader( link ) ){

                log_v_error_P( PSTR("not a leader!") );

                goto end;
            }

            link_state_t *link_state = link_ls_get_data( link );

            // get meta data from database
            catbus_meta_t meta;
            if( kv_i8_get_catbus_meta( link_state->dest_key, &meta ) < 0 ){

                log_v_error_P( PSTR("dest key not found!") );

                goto end;
            }

            // check keys.  the producer should be sending us the 
            // source key (they don't know the destination key)
            if( msg->data.meta.hash != link_state->source_key ){

                log_v_error_P( PSTR("producer sent wrong source key!") );

                goto end;
            }

            // now change the key in the msg meta data to the 
            // dest key, which is what we're using from here on out
            msg->data.meta.hash = link_state->dest_key;

            // compare meta data, all producers need to match the leader
            // if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

            //     log_v_error_P( PSTR("meta data mismatch!") );

            //     goto end;
            // }

            // verify data lengths
            uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
            // uint16_t array_len = meta.count + 1;
            // uint16_t type_len = type_u16_size( meta.type );
            // uint16_t data_len = array_len * type_len;

            // if( data_len != msg_data_len ){

            //     log_v_error_P( PSTR("rx len does not match!") );

            //     goto end;
            // }

            // update remote data and timeout
            // update_remote( &raddr, link, &msg->data.data, data_len );
            update_remote( &raddr, link, &msg->data, msg_data_len );
        }
        else if( header->msg_type == LINK_MSG_TYPE_ADD ){

            link_msg_add_t *msg = (link_msg_add_t *)header;

            link_handle_t link = link_l_create(
                msg->mode,
                msg->source_key,
                msg->dest_key,
                &msg->query,
                msg->tag,
                msg->rate,
                msg->aggregation,
                msg->filter );

            link_msg_confirm_t reply;

            if( link > 0 ){

                save_link_to_file( link );
                reply.status = 0;
            }
            else{

                reply.status = -1;
            }
            
            init_header( &reply.header, LINK_MSG_TYPE_CONFIRM );

            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == LINK_MSG_TYPE_DELETE ){

            link_msg_delete_t *msg = (link_msg_delete_t *)header;

            if( msg->tag != 0 ){
                
                link_v_delete_by_tag( msg->tag );
            }
            else{

                link_v_delete_by_hash( msg->hash );
            }

            link_msg_confirm_t reply;
            init_header( &reply.header, LINK_MSG_TYPE_CONFIRM );
            reply.status = 0;

            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
        }
        else if( header->msg_type == LINK_MSG_TYPE_SHUTDOWN ){

            list_node_t ln = producer_list.head;

            while( ln >= 0 ){

                list_node_t next_ln = list_ln_next( ln );

                producer_state_t *producer = list_vp_get_data( ln );

                if( sock_b_addr_compare( &raddr, &producer->leader_addr ) ){

                    // remove producer
                    list_v_remove( &producer_list, ln );
                    list_v_release_node( ln );

                    trace_printf("LINK: producer leader shutdown\n");
                }

                ln = next_ln;
            }   


            ln = remote_list.head;

            while( ln >= 0 ){

                list_node_t next_ln = list_ln_next( ln );

                remote_state_t *remote = list_vp_get_data( ln );

                if( sock_b_addr_compare( &raddr, &remote->addr ) ){

                    // remove remote
                    list_v_remove( &remote_list, ln );
                    list_v_release_node( ln );

                    trace_printf("LINK: remote shutdown\n");
                }

                ln = next_ln;
            }


            ln = consumer_list.head;

            while( ln >= 0 ){

                list_node_t next_ln = list_ln_next( ln );

                consumer_state_t *consumer = list_vp_get_data( ln );

                // if timeout expires, or we are not link leader
                if( sock_b_addr_compare( &raddr, &consumer->addr ) ){

                    // remove consumer
                    list_v_remove( &consumer_list, ln );
                    list_v_release_node( ln );

                    trace_printf("LINK: consumer shutdown\n");
                }

                ln = next_ln;
            }

        }


end:
    
        THREAD_YIELD( pt );
    }

PT_END( pt );
}

PT_THREAD( link_discovery_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, link_u8_count() == 0 );

    while(1){

        list_node_t ln = link_list.head;

        while( ln >= 0 ){

            link_state_t *link_state = list_vp_get_data( ln );

            // check if we are link leader
            if( services_b_is_server( LINK_SERVICE, link_state->hash ) ){

                // send links use consumer query
                if( link_state->mode == LINK_MODE_SEND ){

                    transmit_consumer_query( link_state );
                }
                // receiver links used the producer query
                else if( link_state->mode == LINK_MODE_RECV ){

                    transmit_producer_query( link_state );
                }
            }
            // we are not link leader
            else if( services_b_is_available( LINK_SERVICE, link_state->hash ) ){

                // receive and sync link
                if( ( link_state->mode == LINK_MODE_RECV ) ||
                    ( link_state->mode == LINK_MODE_SYNC ) ){

                    // we need to send consumer match to the leader
                    sock_addr_t raddr = services_a_get( LINK_SERVICE, link_state->hash );
                        
                    transmit_consumer_match( link_state->hash, &raddr );
                }
            }   
            

            ln = list_ln_next( ln );
        }        

        TMR_WAIT( pt, LINK_DISCOVER_RATE + ( rnd_u16_get_int() >> 7 ) );

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }
    }

PT_END( pt );
}

typedef struct __attribute__((packed)){
    link_msg_data_t msg;
    uint8_t buf[CATBUS_MAX_DATA - 1]; // subtract 1 to account for data byte in catbus_data_t
} link_data_msg_buf_t;


static uint16_t aggregate( link_handle_t link, catbus_hash_t32 hash, link_data_msg_buf_t *msg_buf ){

    link_state_t *link_state = link_ls_get_data( link );

    // get KV meta data
    kv_meta_t meta;

    if( kv_i8_lookup_hash( hash, &meta ) < 0 ){

        log_v_error_P( PSTR("fatal error") );

        return 0;
    }    
    // also need the catbus version of meta data
    kv_i8_get_catbus_meta( hash, &msg_buf->msg.data.meta );

    uint16_t array_len = meta.array_len + 1;
    uint16_t data_len = kv_u16_get_size_meta( &meta );

    if( data_len == 0 ){

        return 0;
    }
        
    // send and sync links, get from local database
    if( ( link_state->mode == LINK_MODE_SEND ) ||
        ( link_state->mode == LINK_MODE_SYNC ) ){
        
        // get data from database.
        // this will include the full array, for array types
        if( kv_i8_internal_get( &meta, hash, 0, 0, &msg_buf->msg.data.data, CATBUS_MAX_DATA ) < 0 ){

            log_v_error_P( PSTR("fatal error") );

            return 0;
        }
    }
    
    void *ptr = &msg_buf->msg.data.data;
    uint16_t count = 1;

    remote_state_t *remote = 0;
    list_node_t ln = remote_list.head;

    if( meta.type == CATBUS_TYPE_FLOAT ){

        log_v_error_P( PSTR("float not supported") );
    }
    else if( array_len > 1 ){

        log_v_error_P( PSTR("arrays not supported: 0x%x -> %d"), hash, array_len );
    }
    // integer types
    else{

        int64_t accumulator = 0;

        // load initial data for send and sync link
        if( ( link_state->mode == LINK_MODE_SEND ) ||
            ( link_state->mode == LINK_MODE_SYNC ) ){

            accumulator = specific_to_i64( meta.type, ptr );
        }
        // load initial data for recv link
        else if( link_state->mode == LINK_MODE_RECV ){

            // warp to first remote that matches this link
            while( ln > 0 ){

                remote = list_vp_get_data( ln );

                ln = list_ln_next( ln );    

                if( remote->link_hash == link_state->hash ){

                    accumulator = specific_to_i64( meta.type, &remote->data.data );

                    break;
                }
            }
        }

        // for ANY, we return the first value
        // sync links should always be ANY since they only hvae
        // the leader value
        if( ( link_state->aggregation == LINK_AGG_ANY ) ||
            ( link_state->mode == LINK_MODE_SYNC ) ){

            i64_to_specific( accumulator, meta.type, ptr );
        }
        else{ // run aggregation

            while( ln > 0 ){

                remote = list_vp_get_data( ln );

                if( remote->link_hash != link_state->hash ){

                    goto next;
                }

                count++;
            
                // convert to i64
                int64_t temp = specific_to_i64( meta.type, &remote->data.data );

                if( link_state->aggregation == LINK_AGG_MIN ){

                    if( temp < accumulator ){

                        accumulator = temp;
                    }
                }
                else if( link_state->aggregation == LINK_AGG_MAX ){

                    if( temp > accumulator ){

                        accumulator = temp;
                    }
                }
                else if( link_state->aggregation == LINK_AGG_SUM ){

                    accumulator += temp;
                }
                else if( link_state->aggregation == LINK_AGG_AVG ){

                    accumulator += temp;
                }

    next:
                ln = list_ln_next( ln );
            };

            if( link_state->aggregation == LINK_AGG_AVG ){

                accumulator /= count;
            }

            i64_to_specific( accumulator, meta.type, ptr );
        }
    }

    // check if data changed
    uint32_t data_hash = hash_u32_data( &msg_buf->msg.data.data, data_len );
        
    // check if data did not change.
    // if no change, we skip transmission, UNLESS 
    // the transmit timer has also expired
    if( ( data_hash == link_state->data_hash ) &&
        ( link_state->retransmit_timer > 0 ) ){

        // data did not change

        // if the interval between the last transmission and now is too short,
        // we will skip transmission of data

        return 0;   
    }

    link_state->data_hash = data_hash;

    // receiver leaders need to set their own data
    if( link_state->mode == LINK_MODE_RECV ){

        kv_i8_set( hash, &msg_buf->msg.data.data, data_len );
    }

    return data_len;
}

static void transmit_to_consumers( link_handle_t link, link_data_msg_buf_t *msg_buf, uint16_t data_len ){

    link_state_t *link_state = link_ls_get_data( link );

    link_state->retransmit_timer = LINK_RETRANSMIT_RATE;

    init_header( &msg_buf->msg.header, LINK_MSG_TYPE_CONSUMER_DATA );

    msg_buf->msg.hash = link_state->dest_key;
    msg_buf->msg.data.meta.hash = link_state->dest_key;

    uint16_t msg_len = ( sizeof(link_msg_data_t) - 1 ) + data_len;

    list_node_t ln = consumer_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        consumer_state_t *consumer = list_vp_get_data( ln );

        if( consumer->link_hash == link_state->hash ){

            sock_addr_t raddr = consumer->addr;

            // trace_printf("LINK: TX consumer data %d.%d.%d.%d\n",
            //     consumer->addr.ipaddr.ip3,
            //     consumer->addr.ipaddr.ip2,
            //     consumer->addr.ipaddr.ip1,
            //     consumer->addr.ipaddr.ip0
            // );

            if( sock_i16_sendto( sock, (uint8_t *)&msg_buf->msg, msg_len, &raddr ) < 0 ){

                log_v_error_P( PSTR("socket send failed, possibly out of memory") );
            }
        }

        ln = next_ln;
    }
}

static void transmit_producer_data( uint64_t hash, catbus_meta_t *meta, uint8_t *data, uint16_t data_len, sock_addr_t *raddr ){

    // prepare data message
    uint16_t msg_len = ( sizeof(link_msg_data_t) - sizeof(uint8_t) ) + data_len; // subtract an extra byte to compensate for the catbus_data_t.data field

    mem_handle_t h = mem2_h_alloc( msg_len );

    if( h < 0 ){

        return;
    }

    link_msg_data_t *msg = mem2_vp_get_ptr_fast( h );
    init_header( &msg->header, LINK_MSG_TYPE_PRODUCER_DATA );

    msg->hash = hash;
    msg->data.meta = *meta;

    memcpy( &msg->data.data, data, data_len );

    sock_i16_sendto_m( sock, h, raddr );

    // trace_printf("LINK: transmit producer data: %d.%d.%d.%d\n", raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );   
}

static void transmit_shutdown( void ){

    link_msg_header_t header;
    init_header( &header, LINK_MSG_TYPE_SHUTDOWN );

    sock_addr_t raddr;
    raddr.ipaddr = ip_a_addr(255,255,255,255);
    raddr.port = LINK_PORT;

    sock_i16_sendto( sock, (uint8_t *)&header, sizeof(header), &raddr );
}

static void process_link( link_handle_t link, uint32_t elapsed_ms ){

    link_state_t *link_state = link_ls_get_data( link );

    if( link_state->retransmit_timer >= 0 ){

        link_state->retransmit_timer -= elapsed_ms;
    }

    link_state->ticks -= elapsed_ms;

    if( link_state->ticks > 0 ){

        return;
    }        

    // update ticks for next iteration
    link_state->ticks += link_state->rate;

    // update tick rate
    if( link_state->rate < link_process_tick_rate ){

        link_process_tick_rate = link_state->rate;
    }

    link_data_msg_buf_t msg_buf;

    // SEND link:
    // we are a producer
    if( link_state->mode == LINK_MODE_SEND ){

        // link leader
        if( services_b_is_server( LINK_SERVICE, link_state->hash ) ){
            
            // run aggregation
            uint16_t data_len = aggregate( link, link_state->source_key, &msg_buf );

            // transmit!
            if( data_len > 0 ){

                transmit_to_consumers( link, &msg_buf, data_len );    
            }        
        }
        // link follower
        else if( services_b_is_available( LINK_SERVICE, link_state->hash ) ){

            // get data
            if( kv_i8_array_get( link_state->source_key, 0, 0, msg_buf.buf, sizeof(msg_buf.buf) ) < 0 ){

                return;
            }

            catbus_meta_t meta;
            if( kv_i8_get_catbus_meta( link_state->source_key, &meta ) < 0 ){

                return;
            }

            uint16_t data_len = type_u16_size_meta( &meta );
            uint32_t data_hash = hash_u32_data( msg_buf.buf, data_len );

            // check if data did not change.
            // if no change, we skip transmission, UNLESS 
            // the transmit timer has also expired
            if( ( data_hash == link_state->data_hash ) &&
                ( link_state->retransmit_timer > 0 ) ){

                // data did not change

                // if the interval between the last transmission and now is too short,
                // we will skip transmission of data

                return;   
            }

            link_state->data_hash = data_hash;
            link_state->retransmit_timer = LINK_RETRANSMIT_RATE;

            sock_addr_t raddr = services_a_get( LINK_SERVICE, link_state->hash );
            transmit_producer_data( link_state->hash, &meta, msg_buf.buf, data_len, &raddr );
        }
    }
    else if( ( link_state->mode == LINK_MODE_RECV ) ||
             ( link_state->mode == LINK_MODE_SYNC ) ){
        
        // link leader
        if( services_b_is_server( LINK_SERVICE, link_state->hash ) ){

            // run aggregation
            uint16_t data_len = aggregate( link, link_state->dest_key, &msg_buf );

            // transmit!
            if( data_len > 0 ){

                transmit_to_consumers( link, &msg_buf, data_len );    
            }   
        }
    }
}

static void process_producer( producer_state_t *producer, uint32_t elapsed_ms ){

    if( producer->retransmit_timer >= 0 ){

        producer->retransmit_timer -= elapsed_ms;
    }

    producer->ticks -= elapsed_ms;

    if( producer->ticks > 0 ){

        return;
    }

    // update ticks for next iteration
    producer->ticks += producer->rate;

    // update tick rate
    if( producer->rate < link_process_tick_rate ){

        link_process_tick_rate = producer->rate;
    }

    // get meta data from database
    catbus_meta_t meta;
    if( kv_i8_get_catbus_meta( producer->source_key, &meta ) < 0 ){

        log_v_error_P( PSTR("source key not found!") );

        return;
    }

    uint16_t data_len = type_u16_size_meta( &meta );    
    if( data_len > CATBUS_MAX_DATA ){

        log_v_error_P( PSTR("data len too long!") );

        return;
    }

    // get data
    uint8_t buf[CATBUS_MAX_DATA];
    kv_i8_array_get( producer->source_key, 0, 0, buf, sizeof(buf) );

    uint32_t data_hash = hash_u32_data( buf, data_len );

    // trace_printf("LINK: produce DATA, hash: %lx\n", data_hash);

    // check if we're the link leader.
    // if so, we don't transmit a producer message (since they are coming to us).
    if( services_b_is_server( LINK_SERVICE, producer->link_hash ) ){

        producer->data_hash = data_hash;

        return;
    }


    // check if data did not change.
    // if no change, we skip transmission, UNLESS 
    // the transmit timer has also expired
    if( ( data_hash == producer->data_hash ) &&
        ( producer->retransmit_timer > 0 ) ){

        // data did not change

        // if the interval between the last transmission and now is too short,
        // we will skip transmission of data

        // trace_printf("no change\n");

        return;   
    }

    // data has either changed, or it has not but we are going to transmit anyway

    // trace_printf("LINK: data changed!\n");

    producer->data_hash = data_hash;
    producer->retransmit_timer = LINK_RETRANSMIT_RATE;

    // check if leader IP is not available
    if( ip_b_is_zeroes( producer->leader_addr.ipaddr ) ){

        return;
    }

    transmit_producer_data( producer->link_hash, &meta, buf, data_len, &producer->leader_addr );
}


PT_THREAD( link_processor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    load_links_from_file();

    THREAD_WAIT_WHILE( pt, ( link_u8_count() == 0 ) &&
                           ( producer_count() == 0 ) );

    // init alarm
    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while(1){

        if( link_process_tick_rate < LINK_MIN_TICK_RATE ){

            link_process_tick_rate = LINK_MIN_TICK_RATE;
        }
        else if( link_process_tick_rate > LINK_MAX_TICK_RATE ){

            link_process_tick_rate = LINK_MAX_TICK_RATE;
        }

        uint32_t prev_alarm = thread_u32_get_alarm();

        thread_v_set_alarm( prev_alarm + link_process_tick_rate );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            transmit_shutdown();
            TMR_WAIT( pt, 100 );
            transmit_shutdown();
            TMR_WAIT( pt, 100 );
            transmit_shutdown();

            THREAD_EXIT( pt );
        }

        uint32_t elapsed_time = link_process_tick_rate;

        // reset process tick rate.
        // existing links and producers will update to the max rate
        // needed.  this is here to reduce the rate if a link or producer
        // is removed.
        link_process_tick_rate = LINK_MIN_TICK_RATE;

        // update timeouts
        process_consumer_timeouts( elapsed_time );
        process_producer_timeouts( elapsed_time );
        process_remote_timeouts( elapsed_time );
        
        list_node_t ln;

        // process producers
        ln = producer_list.head;

        while( ln >= 0 ){

            producer_state_t *producer = list_vp_get_data( ln );
            
            process_producer( producer, elapsed_time );
            
            ln = list_ln_next( ln );
        }

        // process links
        ln = link_list.head;

        while( ln >= 0 ){

            process_link( ln, elapsed_time );

            ln = list_ln_next( ln );
        }

    }

PT_END( pt );
}




#endif
