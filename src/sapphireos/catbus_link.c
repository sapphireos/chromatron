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

#include "catbus_link.h"

// #define NO_LOGGING
#include "logging.h"

#ifdef ENABLE_CATBUS_LINK

static uint16_t tick_rate = LINK_MIN_TICK_RATE;

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
    int32_t ticks;
    int32_t timeout;
} producer_state_t;

// remote producer:
// producer state tracked by the leader
typedef struct __attribute__((packed)){
    link_handle_t link;
    ip_addr4_t ip;
    int32_t timeout;
} remote_producer_state_t;

// consumer:
// stored on leader, tracks consumers to transmit data to
typedef struct __attribute__((packed)){
    link_handle_t link;
    ip_addr4_t ip;
    int32_t timeout;
} consumer_state_t;

// // leader:
// // state stored on link leader for transmission to consumers
// // a leader always has a copy of the link
// typedef struct __attribute__((packed)){
//     link_state_t *link;
//     uint8_t consumer_count;
//     ip_addr4_t consumer_ips; // first entry, additional will follow up to consumer_count
// } leader_state_t;


static int32_t link_test_key;

KV_SECTION_META kv_meta_t link_kv[] = {
    { SAPPHIRE_TYPE_INT32,   0, 0,                   &link_test_key,        0,           "link_test_key" },
};

PT_THREAD( test_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 2000 );

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


    catbus_query_t query = { 0 };
    query.tags[0] = __KV__link_test2;

    // if( cfg_u64_get_device_id() == 93172270997720 ){

        link_l_create( 
            LINK_MODE_SEND, 
            __KV__link_test_key, 
            __KV__kv_test_key,
            &query,
            __KV__my_tag,
            LINK_RATE_1000ms,
            LINK_AGG_ANY,
            LINK_FILTER_OFF );
if( cfg_u64_get_device_id() == 93172270997720 ){
        thread_t_create( test_thread,
                 PSTR("test_thread"),
                 0,
                 0 );

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

	return hash_u64_data( (uint8_t *)link, sizeof(link_state_t) - sizeof(uint64_t) );	
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

    // check if we have the required keys
    if( state->mode == LINK_MODE_SEND ){
        
        if( kv_i16_search_hash( state->source_key ) < 0 ){

            return -1;
        }
    }
    else if( state->mode == LINK_MODE_RECV ){
        
        if( kv_i16_search_hash( state->dest_key ) < 0 ){

            return -1;
        }
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

    // update tick rate
    if( state->rate < tick_rate ){

        tick_rate = state->rate;
    }

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
        .ipaddr = ip_a_addr(255,255,255,255),
        .port = LINK_PORT
    };

    ASSERT( link->mode == LINK_MODE_SEND );

    msg.key     = link->dest_key;
    msg.query   = link->query;
    msg.hash    = link->hash;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

    trace_printf("LINK: %s()\n", __FUNCTION__);
}

static void transmit_producer_query( link_state_t *link ){

    link_msg_producer_query_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_PRODUCER_QUERY );

    sock_addr_t raddr = {
        .ipaddr = ip_a_addr(255,255,255,255),
        .port = LINK_PORT
    };

    ASSERT( link->mode == LINK_MODE_RECV );

    msg.key     = link->source_key;
    msg.query   = link->query;
    msg.hash    = link->hash;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

    trace_printf("LINK: %s()\n", __FUNCTION__);
}

static void transmit_consumer_match( uint64_t hash ){

    // this function assumes the destination is cached in the socket raddr

    link_msg_consumer_match_t msg;
    init_header( &msg.header, LINK_MSG_TYPE_CONSUMER_MATCH );

    msg.hash    = hash;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );

    trace_printf("LINK: %s()\n", __FUNCTION__);
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

        trace_printf("LINK: refreshed consumer timeout\n");

        // update timeout and return
        consumer->timeout = LINK_CONSUMER_TIMEOUT;

        return;
        
    next:
        ln = list_ln_next( ln );
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

static void update_producer_from_link( link_state_t *link_state ){

    list_node_t ln = producer_list.head;

    while( ln >= 0 ){

        producer_state_t *producer = list_vp_get_data( ln );
        
        if( link_state->hash != producer->link_hash ){

            goto next;
        }

        // update state
        producer->leader_ip = services_a_get_ip( LINK_SERVICE, link_state->hash );
        producer->timeout = LINK_PRODUCER_TIMEOUT;

        // trace_printf("LINK: refreshed SEND producer: %d.%d.%d.%d\n",
        //     producer->leader_ip.ip3,
        //     producer->leader_ip.ip2,
        //     producer->leader_ip.ip1,
        //     producer->leader_ip.ip0
        // );

        return;
        
    next:
        ln = list_ln_next( ln );
    }

    // producer was not found, create one

    producer_state_t new_producer = {
        link_state->source_key,
        link_state->hash,
        services_a_get_ip( LINK_SERVICE, link_state->hash ),
        0,
        link_state->rate,
        link_state->rate,
        LINK_PRODUCER_TIMEOUT
    };


    ln = list_ln_create_node2( &new_producer, sizeof(new_producer), MEM_TYPE_LINK_PRODUCER );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &producer_list, ln );

    trace_printf("LINK: became SEND producer\n");
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

            trace_printf("LINK: RX consumer query\n");

            link_msg_consumer_query_t *msg = (link_msg_consumer_query_t *)header;

            // check query
            if( !catbus_b_query_self( &msg->query ) ){

                goto end;
            }

            // check key
            if( kv_i16_search_hash( msg->key ) < 0 ){

                goto end;
            }

            // we are a consumer for this link
            
            // transmit response
            transmit_consumer_match( msg->hash );
        }
        else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_QUERY ){

            trace_printf("LINK: RX producer query\n");

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
            
            trace_printf("LINK: %s() producer match\n", __FUNCTION__);
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_MATCH ){

            trace_printf("LINK: RX consumer match\n");

            link_msg_consumer_match_t *msg = (link_msg_consumer_match_t *)header;

            // received a match
            update_consumer( msg->hash, &raddr );
        }
        else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_DATA ){

            trace_printf("LINK: RX consumer DATA\n");

            // link_msg_data_t *msg = (link_msg_data_t *)header;

            
        }
        else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_DATA ){

            trace_printf("LINK: RX producer DATA\n");

            // link_msg_data_t *msg = (link_msg_data_t *)header;

            
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

            if( services_b_is_server( LINK_SERVICE, link_state->hash ) ){

                if( link_state->mode == LINK_MODE_SEND ){

                    transmit_consumer_query( link_state );
                }
                else if( link_state->mode == LINK_MODE_RECV ){

                    transmit_producer_query( link_state );
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

static void process_link( link_state_t *link_state, uint32_t elapsed_ms ){

    // link leader
    // if( services_b_is_server( LINK_SERVICE, link_state->hash ) ){



    // }
    // // link follower
    // else if( services_b_is_available( LINK_SERVICE, link_state->hash ) ){

    // }

    // SEND link:
    // we are a producer
    if( link_state->mode == LINK_MODE_SEND ){

        update_producer_from_link( link_state );
    }
}

int16_t kv_i16_search_hash_DEBUG( catbus_hash_t32 hash );

static void process_producer( producer_state_t *producer, uint32_t elapsed_ms ){

    producer->ticks -= elapsed_ms;

    if( producer->ticks > 0 ){

        return;
    }

    // update ticks for next iteration
    producer->ticks += producer->rate;

    uint32_t start = tmr_u32_get_system_time_us();

    // get meta data from database
    // catbus_meta_t meta;
    // if( kv_i8_get_meta( producer->source_key, &meta ) < 0 ){

    //     log_v_error_P( PSTR("source key not found!") );

    //     return;
    // }

    kv_i16_search_hash_DEBUG( producer->source_key );

    uint32_t elapsed = tmr_u32_elapsed_time_us( start );
    // trace_printf("LINK: kv lookup %d\n", elapsed);

return;

    // uint16_t data_len = type_u16_size_meta( &meta );    

    // if( data_len > CATBUS_MAX_DATA ){

    //     return;
    // }

    // start = tmr_u32_get_system_time_us();
    // // get data
    // uint8_t buf[CATBUS_MAX_DATA];
    // kv_i8_array_get( producer->source_key, 0, 0, buf, sizeof(buf) );

    // elapsed = tmr_u32_elapsed_time_us( start );
    // trace_printf("LINK: kv get %d\n", elapsed);

    // start = tmr_u32_get_system_time_us();

    // uint32_t data_hash = hash_u32_data( buf, data_len );

    // elapsed = tmr_u32_elapsed_time_us( start );
    // trace_printf("LINK: kv hash %d\n", elapsed);

    // // trace_printf("LINK: produce DATA, hash: %lx\n", data_hash);

    // if( data_hash != producer->data_hash ){

    //     // trace_printf("LINK: data changed!\n");

    //     producer->data_hash = data_hash;
    // }
}

PT_THREAD( link_processor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    static uint32_t prev_alarm;

    THREAD_WAIT_WHILE( pt, link_u8_count() == 0 );

    // init alarm
    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while(1){

        if( tick_rate < LINK_MIN_TICK_RATE ){

            tick_rate = LINK_MIN_TICK_RATE;
        }

        prev_alarm = thread_u32_get_alarm();

        thread_v_set_alarm( prev_alarm + tick_rate );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // check if shutting down
        if( sys_b_shutdown() ){

            THREAD_EXIT( pt );
        }

        uint32_t elapsed_time = tmr_u32_elapsed_time_ms( prev_alarm );

        list_node_t ln = link_list.head;

        while( ln >= 0 ){

            link_state_t *link_state = list_vp_get_data( ln );
            list_node_t next_ln = list_ln_next( ln );

            process_link( link_state, elapsed_time );

            ln = next_ln;
        }

        // update timeouts
        process_consumer_timeouts( elapsed_time );
        process_producer_timeouts( elapsed_time );
        

        ln = producer_list.head;

        while( ln >= 0 ){

            producer_state_t *producer = list_vp_get_data( ln );
            
            process_producer( producer, elapsed_time );
            
            ln = list_ln_next( ln );
        }
    }

PT_END( pt );
}




#endif
