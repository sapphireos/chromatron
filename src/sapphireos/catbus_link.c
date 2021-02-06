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


// producer:
// sends data to a link leader
// producers don't necessarily have a copy of the link,
// sender links automatically become producers.
// receiver links query for producers.
typedef struct __attribute__((packed)){
    catbus_hash_t32 source_key;
    uint64_t link_hash;
    ip_addr4_t leader_ip;  // supplied via services (send mode) or via message (recv mode)
    uint32_t data_hash;
    link_rate_t16 rate;
    int16_t ticks;
    int16_t retransmit_timer;
    int32_t timeout;
} producer_state_t;

// remote producer:
// producer state tracked by the leader
typedef struct __attribute__((packed)){
    link_handle_t link;
    ip_addr4_t ip;
    int32_t timeout;
    catbus_data_t data;
    // variable length data follows
} remote_state_t;

// consumer:
// stored on leader, tracks consumers to transmit data to
typedef struct __attribute__((packed)){
    link_handle_t link;
    ip_addr4_t ip;
    int32_t timeout;
} consumer_state_t;



static int32_t link_test_key;

KV_SECTION_META kv_meta_t link_kv[] = {
    { SAPPHIRE_TYPE_INT32,   0, 0,                   &link_test_key,        0,  "link_test_key" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,    0,                     0,  "test_link" },
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



void link_v_init( void ){

	list_v_init( &link_list );
    list_v_init( &consumer_list );
    list_v_init( &producer_list );
    list_v_init( &remote_list );

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

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

    uint8_t test = 0;
    kv_i8_get( __KV__test_link, &test, sizeof(test) );

    if( test != 0 ){

        catbus_query_t query = { 0 };
        query.tags[0] = __KV____TEST__;


        if( test == 1 ){

            log_v_debug_P( PSTR("link test mode enabled: idle") );
        }
        else if( test == 2 ){

            log_v_debug_P( PSTR("link test mode enabled: test thread only") );

            thread_t_create( test_thread,
                     PSTR("test_thread"),
                     0,
                     0 );
        }
        else if( test == 3 ){

            log_v_debug_P( PSTR("link test mode enabled: send") );
            
            link_l_create( 
                LINK_MODE_SEND, 
                __KV__link_test_key, // source 
                __KV__kv_test_key,   // dest
                &query,
                __KV__test,
                1000,
                LINK_AGG_ANY,
                LINK_FILTER_OFF );        

            thread_t_create( test_thread,
                     PSTR("test_thread"),
                     0,
                     0 );
        }
        else if( test == 4 ){

            log_v_debug_P( PSTR("link test mode enabled: recv") );
            
            link_l_create( 
                LINK_MODE_RECV, 
                __KV__link_test_key, // source
                __KV__kv_test_key,   // dest
                &query,
                __KV__test,
                1000,
                LINK_AGG_ANY,
                LINK_FILTER_OFF );        

            thread_t_create( test_thread,
                     PSTR("test_thread"),
                     0,
                     0 );
        }
    }
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

uint64_t link_u64_hash( link_state_t *link ){

	return hash_u64_data( 
        (uint8_t *)link, 
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
    else{

        ASSERT( FALSE );
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

    if( state->mode == LINK_MODE_SEND ){
        trace_printf("SEND LINK\n");
    }
    else if( state->mode == LINK_MODE_RECV ){
        trace_printf("RECV LINK\n");
    }

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

        if( consumer->link == link ){

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
}

void link_v_handle_shutdown( ip_addr4_t ip ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

    // log_v_debug_P( PSTR("link shutdown %d.%d.%d.%d"), ip.ip3, ip.ip2, ip.ip1, ip.ip0 );

    list_node_t ln = producer_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        producer_state_t *producer = list_vp_get_data( ln );

        if( ip_b_addr_compare( ip, producer->leader_ip ) ){

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

        if( ip_b_addr_compare( ip, remote->ip ) ){

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
        if( ip_b_addr_compare( ip, consumer->ip ) ){

            // remove consumer
            list_v_remove( &consumer_list, ln );
            list_v_release_node( ln );

            trace_printf("LINK: consumer shutdown\n");
        }

        ln = next_ln;
    }
}

void link_v_shutdown( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
	}

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

static void transmit_consumer_match( uint64_t hash, ip_addr4_t ip ){

    // this function assumes the destination is cached in the socket raddr

    link_msg_consumer_match_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_CONSUMER_MATCH );

    msg.hash    = hash;

    sock_addr_t raddr;
    raddr.ipaddr = ip;
    raddr.port = LINK_PORT;    

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

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
        link_state_t *link_state = link_ls_get_data( consumer->link );

        if( link_state->hash != hash ){

            goto next;
        }

        if( !ip_b_addr_compare( raddr->ipaddr, consumer->ip ) ){

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
    }

    consumer_state_t new_consumer = {
        link,
        raddr->ipaddr,
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

        // if timeout expires, or we are not link leader
        if( ( consumer->timeout < 0 ) || ( !is_link_leader( consumer->link ) ) ){

            // remove consumer
            list_v_remove( &consumer_list, ln );
            list_v_release_node( ln );

            trace_printf("LINK: pruned consumer for timeout\n");
        }

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

static void update_remote( ip_addr4_t ip, link_handle_t link, void *data, uint16_t data_len ){

    // NOTE data len is checked by server routine,
    // we can assume it is good here.

    list_node_t ln = remote_list.head;

    while( ln >= 0 ){

        remote_state_t *remote = list_vp_get_data( ln );
        
        if( remote->link != link ){

            goto next;
        }

        if( !ip_b_addr_compare( ip, remote->ip ) ){

            goto next;
        }

        // update state
        remote->timeout = LINK_REMOTE_TIMEOUT;

        memcpy( &remote->data.data, data, data_len );

        // trace_printf("LINK: refreshed remote: %d.%d.%d.%d\n",
        //     ip.ip3,
        //     ip.ip2,
        //     ip.ip1,
        //     ip.ip0
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

    // remote was not found, create one
    uint16_t remote_len = ( sizeof(remote_state_t) - sizeof(uint8_t) ) + data_len; // subtract an extra byte to compensate for the catbus_data_t.data field

    ln = list_ln_create_node2( 0, remote_len, MEM_TYPE_LINK_REMOTE );

    if( ln < 0 ){

        return;
    }

    remote_state_t *new_remote = list_vp_get_data( ln );
        
    new_remote->link        = link;
    new_remote->ip          = ip;
    new_remote->timeout     = LINK_REMOTE_TIMEOUT;
    // new_remote->data.meta   = meta;
    link_state_t *link_state = link_ls_get_data( link );

    // set which key we care about
    catbus_hash_t32 key = 0;
    if( link_state->mode == LINK_MODE_SEND ){

        key = link_state->source_key;
    }
    else if( link_state->mode == LINK_MODE_RECV ){

        key = link_state->dest_key;
    }


    ASSERT( kv_i8_get_catbus_meta( key, &new_remote->data.meta ) >= 0 );

    memcpy( &new_remote->data.data, data, data_len );

    list_v_insert_tail( &remote_list, ln );

    trace_printf("LINK: add remote\n");
}

static void update_producer_from_query( link_msg_producer_query_t *msg, sock_addr_t *raddr ){

    list_node_t ln = producer_list.head;

    while( ln >= 0 ){

        producer_state_t *producer = list_vp_get_data( ln );
        
        if( producer->link_hash != msg->hash ){

            goto next;
        }

        // update state
        producer->leader_ip = raddr->ipaddr;
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
        raddr->ipaddr,
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
        if( sys_b_shutdown() ){

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

            // check key
            if( kv_i16_search_hash( msg->key ) < 0 ){

                goto end;
            }

            // we are a consumer for this link
            
            // transmit response
            transmit_consumer_match( msg->hash, raddr.ipaddr );
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

            // we are a producer for this link

            update_producer_from_query( msg, &raddr );

            
            // trace_printf("LINK: %s() producer match\n", __FUNCTION__);
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_MATCH ){

            // trace_printf("LINK: RX consumer match\n");

            link_msg_consumer_match_t *msg = (link_msg_consumer_match_t *)header;

            // received a match
            update_consumer( msg->hash, &raddr );
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_DATA ){

            trace_printf("LINK: RX consumer DATA\n");

            link_msg_data_t *msg = (link_msg_data_t *)header;

            catbus_meta_t meta;

            if( kv_i8_get_catbus_meta( msg->hash, &meta ) < 0 ){

                log_v_error_P( PSTR("rx hash not found!") );

                goto end;
            }

            if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

                log_v_error_P( PSTR("rx meta does not match!") );

                goto end;
            }

            // verify data lengths
            uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
            uint16_t array_len = meta.count + 1;
            uint16_t type_len = type_u16_size( meta.type );
            uint16_t data_len = array_len * type_len;

            if( data_len != msg_data_len ){

                log_v_error_P( PSTR("rx len does not match!") );

                goto end;
            }

            kv_i8_set( msg->hash, &msg->data.data, data_len );   
        }
        else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_DATA ){

            trace_printf("LINK: RX producer DATA\n");

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
            if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

                log_v_error_P( PSTR("meta data mismatch!") );

                goto end;
            }

            // verify data lengths
            uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
            uint16_t array_len = meta.count + 1;
            uint16_t type_len = type_u16_size( meta.type );
            uint16_t data_len = array_len * type_len;

            if( data_len != msg_data_len ){

                log_v_error_P( PSTR("rx len does not match!") );

                goto end;
            }

            // update remote data and timeout
            update_remote( raddr.ipaddr, link, &msg->data.data, data_len );
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

                if( link_state->mode == LINK_MODE_SEND ){

                    transmit_consumer_query( link_state );
                }
                else if( link_state->mode == LINK_MODE_RECV ){

                    transmit_producer_query( link_state );
                }
            }
            // we are not link leader
            else if( services_b_is_available( LINK_SERVICE, link_state->hash ) ){

                // receive link
                if( link_state->mode == LINK_MODE_RECV ){

                    // we need to send consumer match to the leader

                    transmit_consumer_match( link_state->hash, services_a_get_ip( LINK_SERVICE, link_state->hash ) );
                }
            }   
            

            ln = list_ln_next( ln );
        }        

        TMR_WAIT( pt, LINK_DISCOVER_RATE + ( rnd_u16_get_int() >> 7 ) );

        // check if shutting down
        if( sys_b_shutdown() ){

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

    uint16_t type_size = kv_u16_get_size_meta( &meta );
    uint16_t array_len = meta.array_len + 1;
    uint16_t data_len = type_size * array_len;

    if( link_state->mode == LINK_MODE_SEND ){
        
        // get data from database.
        // this will include the full array, for array types
        if( kv_i8_internal_get( &meta, hash, 0, 0, &msg_buf->msg.data.data, CATBUS_MAX_DATA ) < 0 ){

            log_v_error_P( PSTR("fatal error") );

            return 0;
        }
    }
    
    void *ptr = &msg_buf->msg.data.data;
    list_node_t ln = remote_list.head;
    remote_state_t *remote = 0;
    uint16_t count = 1;

    if( meta.type == CATBUS_TYPE_BOOL ){


    }
    else if( meta.type == CATBUS_TYPE_FLOAT ){

        
    }
    // integer types
    else{

        int64_t accumulator = 0;

        // load initial data for send link
        if( link_state->mode == LINK_MODE_SEND ){

            accumulator = specific_to_i64( meta.type, ptr );
        }
        // load initial data for recv link
        else if( link_state->mode == LINK_MODE_RECV ){

            if( ln < 0 ){ // there are no remotes, so we have no data

                return 0;
            }

            remote = list_vp_get_data( ln );
            ln = list_ln_next( ln );

            accumulator = specific_to_i64( meta.type, &remote->data.data );
        }

        // for ANY, we return the first value
        if( link_state->aggregation == LINK_AGG_ANY ){

            i64_to_specific( accumulator, meta.type, ptr );

            goto done;
        }

        while( ln > 0 ){

            remote = list_vp_get_data( ln );

            if( remote->link != link ){

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
    

done:

    if( data_len == 0 ){

        return 0;
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

        if( consumer->link == link ){

            sock_addr_t raddr = {
                .ipaddr = consumer->ip,
                .port = LINK_PORT
            };

            trace_printf("LINK: TX consumer data %d.%d.%d.%d\n",
                consumer->ip.ip3,
                consumer->ip.ip2,
                consumer->ip.ip1,
                consumer->ip.ip0
            );

            if( sock_i16_sendto( sock, (uint8_t *)&msg_buf->msg, msg_len, &raddr ) < 0 ){

                log_v_error_P( PSTR("socket send failed, possibly out of memory") );
            }
        }

        ln = next_ln;
    }
}

static void transmit_producer_data( uint64_t hash, catbus_meta_t *meta, uint8_t *data, uint16_t data_len, ip_addr4_t ip ){

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

    sock_addr_t raddr = {
        ip,
        LINK_PORT
    };

    sock_i16_sendto_m( sock, h, &raddr );

    trace_printf("LINK: transmit producer data: %d.%d.%d.%d\n", raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );   
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
            // uint16_t data_len = aggregate( link, producer->source_key, &msg_buf );
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

            transmit_producer_data( link_state->hash, &meta, msg_buf.buf, data_len, services_a_get_ip( LINK_SERVICE, link_state->hash ) );
        }
    }
    else if( link_state->mode == LINK_MODE_RECV ){
        
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
    if( ip_b_is_zeroes( producer->leader_ip ) ){

        return;
    }

    transmit_producer_data( producer->link_hash, &meta, buf, data_len, producer->leader_ip );
}


PT_THREAD( link_processor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

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
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // check if shutting down
        if( sys_b_shutdown() ){

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
