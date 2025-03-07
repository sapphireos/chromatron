
/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2025  Jeremy Billheimer
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


#include "list.h"
#include "sapphire.h"
#include "controller.h"
#include "mqtt_client.h"
#include "mqtt_broker.h"
#include "sockets.h"

#ifdef ENABLE_BROKER

PT_THREAD( mqtt_broker_server_thread( pt_t *pt, void *state ) );
PT_THREAD( mqtt_broker_timeout_thread( pt_t *pt, void *state ) );


/**********************************
			 BROKER
**********************************/

static socket_t broker_sock;

typedef struct __attribute__((packed)){
	char topic[MQTT_MAX_TOPIC_LEN];
	// uint8_t qos;
	sock_addr_t raddr;
	uint8_t timeout;
} mqtt_broker_sub_t;

static list_t broker_sub_list;


int8_t _broker_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        if( hash == __KV__mqtt_broker_sub_count ){

            STORE16(data, list_u8_count( &broker_sub_list ) );
        }
    }
    else if( op == KV_OP_SET ){

    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

static uint32_t mqtt_broker_msgs_publish_recv;
static uint32_t mqtt_broker_msgs_subscribe_recv;
static uint32_t mqtt_broker_msgs_publish_route;
static uint32_t mqtt_broker_msgs_publish_drop;


KV_SECTION_OPT kv_meta_t mqtt_broker_kv[] = {
    { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, 0,									_broker_kv_handler,  "mqtt_broker_sub_count" },
    { CATBUS_TYPE_UINT32, 	0, KV_FLAGS_READ_ONLY, &mqtt_broker_msgs_publish_recv,		0,  				 "mqtt_broker_msgs_publish_recv" },
    { CATBUS_TYPE_UINT32, 	0, KV_FLAGS_READ_ONLY, &mqtt_broker_msgs_subscribe_recv,	0,  				 "mqtt_broker_msgs_subscribe_recv" },
    { CATBUS_TYPE_UINT32, 	0, KV_FLAGS_READ_ONLY, &mqtt_broker_msgs_publish_route,		0,  				 "mqtt_broker_msgs_publish_route" },
    { CATBUS_TYPE_UINT32, 	0, KV_FLAGS_READ_ONLY, &mqtt_broker_msgs_publish_drop,		0,  				 "mqtt_broker_msgs_publish_drop" },
};

static bool broker_running;
static thread_t server_thread;
static thread_t timeout_thread;


void mqtt_broker_v_start( void ){

    if( broker_running ){

        return;
    }

    broker_running = TRUE;

	kv_v_add_db_info( mqtt_broker_kv, sizeof(mqtt_broker_kv) );

   	// create socket
    broker_sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( broker_sock >= 0 );

    sock_v_bind( broker_sock, MQTT_BROKER_PORT );
    sock_v_set_timeout( broker_sock, 1 );

    
    list_v_init( &broker_sub_list );

    server_thread = 
	thread_t_create( mqtt_broker_server_thread,
                     PSTR("mqtt_broker_server"),
                     0,
                     0 );

    timeout_thread = 
	thread_t_create( mqtt_broker_timeout_thread,
                 PSTR("mqtt_broker_timeout"),
                 0,
                 0 );
}

void mqtt_broker_v_stop( void ){

    if( !broker_running ){

        return;
    }

    broker_running = FALSE;

    kv_v_remove_db_info( mqtt_broker_kv );

    sock_v_release( broker_sock );
    broker_sock = -1;

    thread_v_kill( server_thread );
    thread_v_kill( timeout_thread );

    server_thread = -1;
    timeout_thread = -1;

    list_v_destroy( &broker_sub_list );
}


static void broker_process_publish( mqtt_msg_publish_t *msg, sock_addr_t *raddr, mem_handle_t packet_h ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	// uint8_t topic_len = *ptr;
	ptr++;
	const char *topic = (char *)ptr;
		
	// got the topic
	// we don't care about the data

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

        mqtt_broker_sub_t *sub = list_vp_get_data( ln );
        
        // if( strncmp( topic, sub->topic, topic_len ) == 0 ){
        if( mqtt_b_match_topic( topic, sub->topic ) ){

            // match!

            // log_v_debug_P( PSTR("broker match: %s from %d.%d.%d.%d to %d.%d.%d.%d"), 
            // 	topic, 
            // 	raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0, 
            // 	sub->raddr.ipaddr.ip3, sub->raddr.ipaddr.ip2, sub->raddr.ipaddr.ip1, sub->raddr.ipaddr.ip0 );

        	// deref packet handle
        	uint16_t packet_size = mem2_u16_get_size( packet_h );
        	void *packet_ptr = mem2_vp_get_ptr( packet_h );

        	mqtt_broker_msgs_publish_route++;

        	// retransmit this message
    		if( sock_i16_sendto( broker_sock, packet_ptr, packet_size, &sub->raddr ) < 0 ){

    			log_v_error_P( PSTR("MQTT packet flinging failed") );

    			break;
    		}
        }
        else{

			mqtt_broker_msgs_publish_drop++;        	
        }

        ln = list_ln_next( ln );        
    }
}


// static void broker_process_publish_kv( mqtt_msg_publish_t *msg, sock_addr_t *raddr ){

	
// }

static void broker_process_subscribe( mqtt_msg_subscribe_t *msg, const sock_addr_t *raddr ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	uint8_t topic_len = *ptr;
	ptr++;
	const char *topic = (char *)ptr;

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

        mqtt_broker_sub_t *sub = list_vp_get_data( ln );

        log_v_debug_P( PSTR("%s %d %d.%d.%d.%d"), sub->topic, mqtt_b_match_topic( topic, sub->topic ), sub->raddr.ipaddr.ip3, sub->raddr.ipaddr.ip2, sub->raddr.ipaddr.ip1, sub->raddr.ipaddr.ip0 );
        
        if( ( mqtt_b_match_topic( topic, sub->topic ) ) &&
        	( ip_b_addr_compare( raddr->ipaddr, sub->raddr.ipaddr ) ) ){

        	// already subscribed

        	// update timeout
        	sub->timeout = MQTT_BROKER_SUB_TIMEOUT;

            return; 
        }

        ln = list_ln_next( ln );     
    }

    // not subscribed, create new subscription

    log_v_debug_P( PSTR("new sub: %s %d.%d.%d.%d"), topic, raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

	mqtt_broker_sub_t new_sub = {
		{ 0 }, // topic
		// qos,
		*raddr, // remote host address
		MQTT_BROKER_SUB_TIMEOUT, // timeout
	}; 

	strncpy( new_sub.topic, topic, topic_len );

    ln = list_ln_create_node2( &new_sub, sizeof(new_sub), MEM_TYPE_MQTT_BROKER_SUB );

    if( ln < 0 ){

    	log_v_error_P( PSTR("failed to add sub") );

        return;
    }

    list_v_insert_tail( &broker_sub_list, ln );
}


static void broker_process_unsubscribe( mqtt_msg_subscribe_t *msg, const sock_addr_t *raddr ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	// uint8_t topic_len = *ptr;
	ptr++;
	const char *topic = (char *)ptr;

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

    	list_node_t next_ln = list_ln_next( ln );

        const mqtt_broker_sub_t *sub = list_vp_get_data( ln );

        if( ( strncmp( topic, sub->topic, MQTT_MAX_TOPIC_LEN ) == 0 ) &&
        	( ip_b_addr_compare( raddr->ipaddr, sub->raddr.ipaddr ) ) ){

			// remove from list
            list_v_remove( &broker_sub_list, ln );
         	list_v_release_node( ln );         	
        }

        ln = next_ln;   
    }
}


static void clear_subs_by_ip( const sock_addr_t *raddr ){

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

    	list_node_t next_ln = list_ln_next( ln );

        const mqtt_broker_sub_t *sub = list_vp_get_data( ln );

        if( ip_b_addr_compare( raddr->ipaddr, sub->raddr.ipaddr ) ){
        
        	// remove from list
            list_v_remove( &broker_sub_list, ln);
         	list_v_release_node( ln );         	
        }

        ln = next_ln;
    }	  
}



// static void broker_process_subscribe_kv( mqtt_msg_subscribe_t *msg, sock_addr_t *raddr ){

// 	// get byte pointer after headers:
// 	uint8_t *ptr = (uint8_t *)( msg + 1 );

// 	// get topic length
// 	uint8_t topic_len = *ptr;
// 	ptr++;
// 	char *topic = (char *)ptr;
		
// }


PT_THREAD( mqtt_broker_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	// hash lookup test:
	// while(1){

	// 	char name[CATBUS_STRING_LEN];
	// 	memset( name, 0, sizeof(name) );
	// 	ip_addr4_t ip = ip_a_addr(10,0,0,211);

	// 	int8_t status = catbus_i8_get_string_for_hash( 0x86b026c3, name, &ip );

	// 	log_v_debug_P( PSTR("resolve %d %s"), status, name );

	// 	if( status == 0 ){

	// 		break;
	// 	}

	// 	TMR_WAIT( pt, 1000 );
	// }
 	

   	while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( broker_sock ) < 0 );

        if( sys_b_is_shutting_down() ){

        	// transmit_shutdown();
        	// TMR_WAIT( pt, 100 );
        	// transmit_shutdown();
        	// TMR_WAIT( pt, 100 );
        	// transmit_shutdown();

        	THREAD_EXIT( pt );
        }

        if( sock_i16_get_bytes_read( broker_sock ) <= 0 ){

            goto end;
        }

        mqtt_msg_header_t *header = sock_vp_get_data( broker_sock );

        // verify message
        if( header->magic != MQTT_MSG_MAGIC ){

            goto end;
        }

        if( header->version != MQTT_MSG_VERSION ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( broker_sock, &raddr );

        // log_v_debug_P( PSTR("broker recv: %d from %d.%d.%d.%d"), header->msg_type, raddr.ipaddr.ip3, raddr.ipaddr.ip2, raddr.ipaddr.ip1, raddr.ipaddr.ip0 );

        mem_handle_t packet_h = sock_h_get_data_handle( broker_sock );

        ASSERT( packet_h > 0 );

        if( header->msg_type == MQTT_MSG_PUBLISH ){

        	mqtt_broker_msgs_publish_recv++;

        	broker_process_publish( (mqtt_msg_publish_t *)header, &raddr, packet_h );
        }
        else if( header->msg_type == MQTT_MSG_PUBLISH_KV ){

        	mqtt_broker_msgs_publish_recv++;

        	broker_process_publish( (mqtt_msg_publish_t *)header, &raddr, packet_h );
        }
        else if( header->msg_type == MQTT_MSG_SUBSCRIBE ){

        	mqtt_broker_msgs_subscribe_recv++;

        	broker_process_subscribe( (mqtt_msg_subscribe_t *)header, &raddr );
        }
        else if( header->msg_type == MQTT_MSG_SUBSCRIBE_KV ){

        	mqtt_broker_msgs_subscribe_recv++;

        	broker_process_subscribe( (mqtt_msg_subscribe_t *)header, &raddr );
        }
        else if( header->msg_type == MQTT_MSG_UNSUBSCRIBE ){

        	broker_process_unsubscribe( (mqtt_msg_subscribe_t *)header, &raddr );
        }
        else if( header->msg_type == MQTT_MSG_BRIDGE ){

        	// broker_ip = raddr.ipaddr;
        }
        else if( header->msg_type == MQTT_MSG_SHUTDOWN ){

        	// broker_ip = raddr.ipaddr;
        	clear_subs_by_ip( &raddr );
        }
        else{

        	// invalid message
        	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
        }

        // release original handle
		mem2_v_free( packet_h );

    end:

    	// THREAD_YIELD( pt );
    	{};
	}
    
PT_END( pt );
}

PT_THREAD( mqtt_broker_timeout_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

   	while(1){

        TMR_WAIT( pt, 1000 );

 		list_node_t ln = broker_sub_list.head;

	    while( ln >= 0 ){

	    	list_node_t next_ln = list_ln_next( ln );

	        mqtt_broker_sub_t *sub = list_vp_get_data( ln );
	        
	        sub->timeout--;

	        if( sub->timeout == 0 ){

	        	log_v_info_P( PSTR("Sub timeout: %d.%d.%d.%d %s"), 
	        		sub->raddr.ipaddr.ip3,
	        		sub->raddr.ipaddr.ip2,
	        		sub->raddr.ipaddr.ip1,
	        		sub->raddr.ipaddr.ip0,
	        		sub->topic
	        	);

				// remove from list
	            list_v_remove( &broker_sub_list, ln);
	         	list_v_release_node( ln );         	
	        }

	        ln = next_ln;
	    }	   
	}
    
PT_END( pt );
}


#endif
