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

#include "sapphire.h"
#include "config.h"

#include "controller.h"
#include "mqtt_client.h"

/*

Chromatron MQTT Bridge System

Supports basic publish and subscribe.
Use a controller elected local UDP broker, or connect
to a Python based bridge to a full MQTT broker.

Our system is connectionless and only holds state for a timeout
period.

Publishes immediately route to the broker/subscribers.  No connection
required.

Subscriptions create a tracking object on the bridge/broker.
On the Python bridge, subscriptions map to a full Paho subscription.
On the UDP broker, subscriptions are used to route publishes.

Subscriptions are also tracked on the client.  Incoming publish data
is routed to a callback given in the subscription.

Subscriptions on the broker/bridge time out if not periodically refreshed.
On the client, they are held until the client unsubscribes from the topic.

Generally topics will live as flash strings.

*/


#ifdef ENABLE_CONTROLLER

typedef struct __attribute__((packed)){
	char topic[MQTT_MAX_TOPIC_LEN];
	uint32_t kv_hash;
	// uint8_t qos;
	mqtt_on_publish_callback_t callback;
} mqtt_sub_t;

static list_t sub_list;

static socket_t sock;

// static bool controller_enabled;

// static uint8_t controller_state;
// #define STATE_IDLE 		0
// #define STATE_VOTER	    1
// #define STATE_FOLLOWER 	2
// #define STATE_CANDIDATE 3
// #define STATE_LEADER 	4


// static catbus_string_t state_name;

// static ip_addr4_t leader_ip;
// static uint8_t leader_flags;
// static uint16_t leader_priority;
// static uint16_t leader_follower_count;
// static uint16_t leader_timeout;
// static uint32_t leader_uptime;

static ip_addr4_t broker_ip;

KV_SECTION_META kv_meta_t mqtt_client_kv[] = {
//     { CATBUS_TYPE_UINT8, 	0, KV_FLAGS_READ_ONLY, &controller_state, 		0,  "controller_state" },
//     { CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, &state_name,				0,  "controller_state_text" },
//     { CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST,   &controller_enabled, 	0,  "controller_enable_leader" },
    { CATBUS_TYPE_IPv4, 	0, KV_FLAGS_PERSIST, &broker_ip, 				0,  "mqtt_broker_ip" },
//     { CATBUS_TYPE_UINT8, 	0, KV_FLAGS_READ_ONLY, &leader_flags, 			0,  "controller_leader_flags" },
//     { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_follower_count, 	0,  "controller_follower_count" },
//     { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_priority, 		0,  "controller_leader_priority" },
//     { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_timeout, 		0,  "controller_leader_timeout" },
//     { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, &leader_uptime,  		0,  "controller_leader_uptime" },
};


PT_THREAD( mqtt_client_thread( pt_t *pt, void *state ) );
PT_THREAD( mqtt_client_server_thread( pt_t *pt, void *state ) );


void mqtt_client_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
    }

    list_v_init( &sub_list );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, MQTT_BRIDGE_PORT );

    thread_t_create( mqtt_client_thread,
                     PSTR("mqtt_client"),
                     0,
                     0 );

    thread_t_create( mqtt_client_server_thread,
                     PSTR("mqtt_client_server"),
                     0,
                     0 );
}

static sock_addr_t get_broker_raddr( void ){
	
	sock_addr_t raddr = {
		.ipaddr = broker_ip,
		.port = MQTT_BRIDGE_PORT
	};

	return raddr;
}

static int16_t send_msg( mem_handle_t h ){

	sock_addr_t raddr = get_broker_raddr();

	return sock_i16_sendto_m( sock, h, &raddr );
}

static int8_t publish( uint8_t msgtype, const char *topic, const void *data, uint16_t data_len, uint8_t qos, bool retain ){

	uint8_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	uint16_t msg_len = sizeof(mqtt_msg_publish_t) + sizeof(uint16_t) + data_len + sizeof(uint8_t) + topic_len;

	mem_handle_t h = mem2_h_alloc( msg_len );

	if( h < 0 ){

		return -1;
	}

	mqtt_msg_publish_t *msg = (mqtt_msg_publish_t *)mem2_vp_get_ptr_fast( h );

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// start with topic
	*ptr = topic_len;
	ptr++;
	memcpy( ptr, topic, topic_len );
	ptr += topic_len;

	// payload len, for generic publish
	if( msgtype == MQTT_MSG_PUBLISH ){

		memcpy( ptr, &data_len, sizeof(data_len) );
		ptr += sizeof(data_len);
	}
	// payload
	memcpy( ptr, data, data_len );
	
	// catbus_meta_t meta = {
	// 	0, // hash
	// 	type, // type,
	// 	0, // array len,
	// 	0, // flags,
	// 	0, // reserved
	// };

	// catbus_data_t *kv_data = (catbus_data_t *)ptr;
	// kv_data->meta = meta;
	
	// memcpy( &kv_data->data, data, data_len );

	mqtt_msg_header_t *header = (mqtt_msg_header_t *)msg;

	header->magic 		= MQTT_MSG_MAGIC;
	header->version 	= MQTT_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->qos    		= qos;
	header->flags       = 0;

	if( send_msg( h ) < 0 ){

		log_v_error_P( PSTR("Send failed") );

		return -2;
	}

	return 0;
}

int8_t mqtt_client_i8_publish_data( const char *topic, catbus_meta_t *meta, const void *data, uint8_t qos, bool retain ){

	// watch for possible stack overflows if we increase this
	uint8_t buf[MQTT_MAX_PAYLOAD_LEN];	

	uint16_t payload_len = sizeof(catbus_meta_t);

	memcpy( buf, meta, payload_len );

	uint16_t data_len = type_u16_size( meta->type );
	payload_len += data_len;

	memcpy( &buf[sizeof(catbus_meta_t)], data, data_len );

	return publish( MQTT_MSG_PUBLISH_KV, topic, buf, payload_len, qos, retain );
}

int8_t mqtt_client_i8_publish( const char *topic, const void *data, uint16_t data_len, uint8_t qos, bool retain ){

	return publish( MQTT_MSG_PUBLISH, topic, data, data_len, qos, retain );
}

int8_t mqtt_client_i8_publish_kv( const char *topic, const char *key, uint8_t qos, bool retain ){

	uint32_t kv_hash = hash_u32_string( (char *)key );	

	catbus_meta_t meta;

	if( kv_i8_get_catbus_meta( kv_hash, &meta ) < 0 ){

		log_v_error_P( PSTR("Key: %s not found"), key );

		return -1;
	}

	// watch for possible stack overflows if we increase this
	uint8_t buf[MQTT_MAX_PAYLOAD_LEN];	

	uint16_t payload_len = sizeof(catbus_meta_t);

	memcpy( buf, &meta, payload_len );

	uint16_t data_len = type_u16_size( meta.type );
	payload_len += data_len;

	if( kv_i8_get( kv_hash, &buf[sizeof(catbus_meta_t)], data_len ) < 0 ){

		log_v_error_P( PSTR("KV error: %s"), key );

		return -2;
	}

	return publish( MQTT_MSG_PUBLISH_KV, topic, buf, payload_len, qos, retain );
}

int8_t transmit_subscribe( uint8_t msgtype, const char *topic, uint8_t qos ){

	uint16_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	uint16_t msg_len = sizeof(mqtt_msg_subscribe_t) + sizeof(uint8_t) + topic_len;

	mem_handle_t h = mem2_h_alloc( msg_len );

	if( h < 0 ){

		return -1;
	}

	mqtt_msg_subscribe_t *msg = (mqtt_msg_subscribe_t *)mem2_vp_get_ptr_fast( h );

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)(msg + 1);

	// start with topic
	*ptr = topic_len;
	ptr++;
	memcpy( ptr, topic, topic_len );
	
	mqtt_msg_header_t *header = (mqtt_msg_header_t *)msg;

	header->magic 		= MQTT_MSG_MAGIC;
	header->version 	= MQTT_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->qos    		= qos;
	header->flags       = 0;

	if( send_msg( h ) < 0 ){

		log_v_error_P( PSTR("Send failed") );

		return -2;
	}

	return 0;
}

int8_t mqtt_client_i8_subscribe( const char *topic, uint8_t qos, mqtt_on_publish_callback_t callback ){

	uint16_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        
        if( strncmp( topic, sub->topic, MQTT_MAX_TOPIC_LEN ) == 0 ){

            return 0; // already subscribed
        }

        ln = list_ln_next( ln );        
    }

    // not subscribed, create new subscription

	mqtt_sub_t new_sub = {
		{ 0 },
		0, // kv hash
		// qos,
		callback,
	}; 

	strncpy( new_sub.topic, topic, topic_len );

    ln = list_ln_create_node2( &new_sub, sizeof(new_sub), MEM_TYPE_MQTT_SUB );

    if( ln < 0 ){

        return -1;
    }

    list_v_insert_tail( &sub_list, ln );

	transmit_subscribe( MQTT_MSG_SUBSCRIBE, topic, qos );

	return 0;
}

int8_t mqtt_client_i8_subscribe_kv( const char *topic, const char *key, uint8_t qos ){

	uint16_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        
        if( strncmp( topic, sub->topic, MQTT_MAX_TOPIC_LEN ) == 0 ){

            return 0; // already subscribed
        }

        ln = list_ln_next( ln );        
    }

    // not subscribed, create new subscription
    uint32_t kv_hash = hash_u32_string( (char *)key );
	
	mqtt_sub_t new_sub = {
		{ 0 },
		kv_hash, // kv hash
		// qos,
		0, // callback
	}; 

	strncpy( new_sub.topic, topic, topic_len );

	log_v_info_P(PSTR("%s %s"), topic, key);

    ln = list_ln_create_node2( &new_sub, sizeof(new_sub), MEM_TYPE_MQTT_SUB );

    if( ln < 0 ){

        return -1;
    }

    list_v_insert_tail( &sub_list, ln );

	transmit_subscribe( MQTT_MSG_SUBSCRIBE_KV, topic, qos );

	return 0;
}

static void transmit_status( void ){

	uint32_t pixel_power = 0;

	kv_i8_get( __KV__pixel_power, &pixel_power, sizeof(pixel_power) );

	catbus_query_t tags;
	catbus_v_get_query( &tags );

	mqtt_msg_publish_status_t msg = {
		{ 0 },
		tags,
		sys_u8_get_mode(),
		tmr_u32_get_system_time_ms(),
		wifi_i8_rssi(),
		thread_u8_get_cpu_percent(),
		mem2_u16_get_used(),
		pixel_power,		
	};

	msg.header.magic 		= MQTT_MSG_MAGIC;
	msg.header.version 		= MQTT_MSG_VERSION;
	msg.header.msg_type 	= MQTT_MSG_PUBLISH_STATUS;
	msg.header.qos    		= 0;
	msg.header.flags       	= 0;

	sock_addr_t raddr = get_broker_raddr();

	sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );	
}

static void mqtt_on_publish_callback( char *topic, uint8_t *data, uint16_t data_len ){

	int32_t value;

	// coert to int32 for debug
	memcpy( &value, data, sizeof(value) );	

	log_v_debug_P( PSTR("%s %ld %ld"), topic, data_len, value );
}

PT_THREAD( mqtt_client_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	static uint32_t counter;
	counter = 0;

	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );
   	
	TMR_WAIT( pt, 1000 );	

	// mqtt_client_i8_subscribe( PSTR("chromatron_mqtt/test_sub"), 0, mqtt_on_publish_callback );

   	while(1){

    	TMR_WAIT( pt, 2000 );

    	// send subscriptions
		list_node_t ln = sub_list.head;	

	    while( ln >= 0 ){

	        mqtt_sub_t *sub = list_vp_get_data( ln );

	        if( transmit_subscribe( MQTT_MSG_SUBSCRIBE_KV, sub->topic, 0 ) < 0 ){
	        // if( transmit_subscribe( sub->topic, sub->qos ) < 0 ){

	        	log_v_debug_P( PSTR("sub fail") );
	        }

	        ln = list_ln_next( ln );        
	    }    	

	   	// uint32_t value = counter;
	   	// counter++;
	   	
	   	// mqtt_client_i8_publish( PSTR("chromatron_mqtt/test_value"), &value, sizeof(value), 0, FALSE );
		

		transmit_status();
	}
    
PT_END( pt );
}

static void process_publish( mqtt_msg_publish_t *msg, sock_addr_t *raddr ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	uint8_t topic_len = *ptr;
	ptr++;
	char *topic = (char *)ptr;
		
	ptr += topic_len;
	
	uint16_t data_len = 0;	
	memcpy( &data_len, ptr, sizeof(data_len) );
	ptr += sizeof(data_len);
	
	uint8_t *data = ptr;

	// mqtt_on_publish_callback( topic, data, data_len );

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        
        if( strncmp( topic, sub->topic, topic_len ) == 0 ){

            // match!

        	// check what to do

        	// if there is a callback, fire it:
        	if( sub->callback != 0 ){

	        	sub->callback( topic, data, data_len );
	        }

	        // check if there is a KV target
	        // if( sub->kv_hash != 0 ){

	        // 	catbus_data_t *catbus_data = (catbus_data_t *)data;

	        // 	uint16_t type_len = type_u16_size( catbus_data->meta.type );
	        	
	        // 	if( type_len != CATBUS_TYPE_SIZE_INVALID ){

	        // 		// apply to KV system:
	        // 		if( catbus_i8_set( sub->kv_hash, catbus_data->meta.type, &catbus_data->data, type_len ) < 0 ){

	        // 			log_v_error_P( PSTR("Error setting KV data for topic: %s"), topic );
	        // 		}
	        // 	}
	        // 	else{

	        // 		log_v_error_P( PSTR("Invalid type: %d on topic: %s"), catbus_data->meta.type, topic );
	        // 	}
	        // }
        }

        ln = list_ln_next( ln );        
    }
}


static void process_publish_kv( mqtt_msg_publish_t *msg, sock_addr_t *raddr ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	uint8_t topic_len = *ptr;
	ptr++;
	char *topic = (char *)ptr;
		
	ptr += topic_len;
	
	uint8_t *data = ptr;

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        
        if( strncmp( topic, sub->topic, topic_len ) == 0 ){

            // match!

        	// check what to do

        	// // if there is a callback, fire it:
        	// if( sub->callback != 0 ){

	        // 	sub->callback( topic, data, data_len );
	        // }

	        // check if there is a KV target
	        if( sub->kv_hash != 0 ){

	        	catbus_data_t *catbus_data = (catbus_data_t *)data;

	        	uint16_t type_len = type_u16_size( catbus_data->meta.type );
	        	
	        	if( type_len != CATBUS_TYPE_SIZE_INVALID ){

	        		// apply to KV system:
	        		if( catbus_i8_set( sub->kv_hash, catbus_data->meta.type, &catbus_data->data, type_len ) < 0 ){

	        			log_v_error_P( PSTR("Error setting KV data for topic: %s"), topic );
	        		}
	        	}
	        	else{

	        		log_v_error_P( PSTR("Invalid type: %d on topic: %s"), catbus_data->meta.type, topic );
	        	}
	        }
        }

        ln = list_ln_next( ln );        
    }
}



PT_THREAD( mqtt_client_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
   	while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sys_b_is_shutting_down() ){

        	THREAD_EXIT( pt );
        }

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto end;
        }

        mqtt_msg_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != MQTT_MSG_MAGIC ){

            goto end;
        }

        if( header->version != MQTT_MSG_VERSION ){

            goto end;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->msg_type == MQTT_MSG_PUBLISH ){

        	process_publish( (mqtt_msg_publish_t *)header, &raddr );
        }
        else if( header->msg_type == MQTT_MSG_PUBLISH_KV ){

        	process_publish_kv( (mqtt_msg_publish_t *)header, &raddr );
        }
        else{

        	// invalid message
        	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
        }


    end:

    	THREAD_YIELD( pt );
	}
    
PT_END( pt );
}


// typedef struct __attribute__((packed)){
// 	ip_addr4_t ip;
// 	catbus_query_t query;
// 	uint16_t service_flags;
// 	uint16_t timeout;
// } follower_t;

// static list_t follower_list;


// PT_THREAD( controller_state_thread( pt_t *pt, void *state ) );
// PT_THREAD( controller_server_thread( pt_t *pt, void *state ) );
// PT_THREAD( controller_timeout_thread( pt_t *pt, void *state ) );


// static uint32_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

//     // the pos and len values are already bounds checked by the FS driver
//     switch( op ){

//         case FS_VFILE_OP_READ:
//             len = list_u16_flatten( &follower_list, pos, ptr, len );
//             break;

//         case FS_VFILE_OP_SIZE:
//             len = list_u16_size( &follower_list );
//             break;

//         default:
//             len = 0;
//             break;
//     }

//     return len;
// }

// static PGM_P get_state_name( uint8_t state ){

// 	if( state == STATE_IDLE ){

// 		return PSTR("idle");
// 	}
// 	else if( state == STATE_VOTER ){

// 		return PSTR("voter");
// 	}
// 	else if( state == STATE_FOLLOWER ){

// 		return PSTR("follower");
// 	}
// 	else if( state == STATE_CANDIDATE ){

// 		return PSTR("candidate");
// 	}
// 	else if( state == STATE_LEADER ){

// 		return PSTR("leader");
// 	}
// 	else{

// 		return PSTR("unknown");
// 	}
// }

// static void apply_state_name( void ){

// 	strncpy_P( state_name.str, get_state_name( controller_state ), sizeof(state_name.str) );
// }

// static void _set_state( uint8_t state ){

// 	// if( controller_state != state ){

// 	// 	log_v_debug_P( PSTR("changing state from %d to %d"), controller_state, state );
// 	// }

// 	controller_state = state;
// 	apply_state_name();
// }

// #define set_state( state )
// 	if( controller_state != state ){ log_v_debug_P( PSTR("changing state from %s to %s"), get_state_name( controller_state ), get_state_name( state ) ); } 
// 	_set_state( state )

// static void update_follower_timeouts( void ){

// 	list_node_t ln = follower_list.head;

//     while( ln > 0 ){

//         list_node_t next_ln = list_ln_next( ln );

//         follower_t *follower = (follower_t *)list_vp_get_data( ln );

//         if( follower->timeout > 0 ){

//         	follower->timeout--;

//         	if( follower->timeout == 0 ){

// 	    		log_v_debug_P( PSTR("Follower timed out: %d.%d.%d.%d"),
// 					follower->ip.ip3, 
// 					follower->ip.ip2, 
// 					follower->ip.ip1, 
// 					follower->ip.ip0
// 				 );

//         		list_v_remove( &follower_list, ln );
// 	            list_v_release_node( ln );
//         	}
//         }

//         ln = next_ln;
//     }	
// }

// static void remove_follower( ip_addr4_t follower_ip ){

// 	list_node_t ln = follower_list.head;

//     while( ln > 0 ){

//         list_node_t next_ln = list_ln_next( ln );

//         follower_t *follower = (follower_t *)list_vp_get_data( ln );

//         if( ip_b_addr_compare( follower_ip, follower->ip ) ){

//         	log_v_debug_P( PSTR("Removing follower: %d.%d.%d.%d"),
// 				follower_ip.ip3, 
// 				follower_ip.ip2, 
// 				follower_ip.ip1, 
// 				follower_ip.ip0
// 			);

//     		list_v_remove( &follower_list, ln );
//             list_v_release_node( ln );
//         }

//         ln = next_ln;
//     }	
// }

// static void update_follower( ip_addr4_t follower_ip, controller_msg_status_t *msg ){

// 	// search for follower:
// 	// if found, update
// 	// if not found, add
// 	list_node_t ln = follower_list.head;

//     while( ln > 0 ){

//         list_node_t next_ln = list_ln_next( ln );

//         follower_t *follower = (follower_t *)list_vp_get_data( ln );

//         if( ip_b_addr_compare( follower_ip, follower->ip ) ){

//         	follower->service_flags = msg->service_flags;
//         	follower->query 		= msg->query;
//         	follower->timeout 		= CONTROLLER_FOLLOWER_TIMEOUT;

//         	return;
//         }

//         ln = next_ln;
//     }		

//     follower_t follower = {
//     	follower_ip,
//     	msg->query,
//     	msg->service_flags,
// 		CONTROLLER_FOLLOWER_TIMEOUT,
//     };

//     // not found, add:
//     ln = list_ln_create_node( &follower, sizeof(follower) );

//     if( ln < 0 ){

//         return;
//     }

//  	list_v_insert_tail( &follower_list, ln );   
// }

// static void reset_leader( void ){

// 	leader_ip = ip_a_addr(0,0,0,0);
// 	leader_priority = 0;
// 	leader_follower_count = 0;
// 	leader_flags = 0;
// 	leader_uptime = 0;
// }

// static uint16_t get_follower_count( void ){

// 	return list_u8_count( &follower_list );
// }

// static uint16_t get_priority( void ){

// 	// check if leader is enabled, if not, return 0 and we are follower only
// 	if( !controller_enabled ){

// 		return 0;
// 	}

// 	uint16_t priority = 0;

// 	// set base priorities

// 	#ifdef ESP32
// 	priority = 256;
// 	#else
// 	priority = 128;
// 	#endif

// 	return priority;
// }

// static void vote( ip_addr4_t ip, uint16_t priority, uint16_t follower_count, uint8_t flags ){

// 	bool leader_change = !ip_b_addr_compare( ip, leader_ip );

// 	// check state:
// 	if( controller_state == STATE_IDLE ){

// 		// we have a candidate, now we vote for it:
// 		if( flags & CONTROLLER_FLAGS_IS_LEADER ){
			
// 			set_state( STATE_FOLLOWER );
// 		}
// 		else{

// 			set_state( STATE_VOTER );
// 		}
// 	}
// 	else if( controller_state == STATE_FOLLOWER ){
		
// 		if( leader_change ){

// 			log_v_debug_P( PSTR("leader change, still a follower") );	
// 		}
// 	}
// 	else if( controller_state == STATE_CANDIDATE ){

// 		// if candidate change, and we are no longer the candidate,
// 		// switch to voter state
// 		if( leader_change && 
// 			!ip_b_addr_compare( ip, cfg_ip_get_ipaddr() ) ){

// 			log_v_debug_P( PSTR("voter: %d.%d.%d.%d from %d.%d.%d.%d"),
// 					ip.ip3,
// 					ip.ip2,
// 					ip.ip1,
// 					ip.ip0,
// 					leader_ip.ip3,
// 					leader_ip.ip2,
// 					leader_ip.ip1,
// 					leader_ip.ip0
// 				);

// 			set_state( STATE_VOTER );
// 		}
// 	}
// 	else if( controller_state == STATE_LEADER ){

// 		if( leader_change ){

// 			set_state( STATE_VOTER );
// 		}
// 	}
// 	else if( controller_state == STATE_VOTER ){

// 		if( !leader_change ){

// 			if( flags & CONTROLLER_FLAGS_IS_LEADER ){
		
// 				set_state( STATE_FOLLOWER );	
// 			}
// 		}
// 	}
// 	else{

// 		log_v_debug_P( PSTR("vote on unhandled state: %d"), controller_state );
// 	}


// 	leader_ip = ip;
// 	leader_priority = priority;
// 	leader_follower_count = follower_count;
// 	leader_timeout = CONTROLLER_FOLLOWER_TIMEOUT;
// 	leader_flags = flags;
// }

// static void vote_self( void ){

// 	uint8_t flags = 0;

// 	if( controller_state == STATE_LEADER ){

// 		flags |= CONTROLLER_FLAGS_IS_LEADER;
// 	}

// 	log_v_debug_P( PSTR("voting self") );

// 	vote( cfg_ip_get_ipaddr(), get_priority(), get_follower_count(), flags );
// }

// void controller_v_init( void ){

// 	return;

// 	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

// 		return;
//     }

//     list_v_init( &follower_list );	

//     // create vfile
//     fs_f_create_virtual( PSTR("directory"), vfile );

//     // create socket
//     sock = sock_s_create( SOS_SOCK_DGRAM );

//     ASSERT( sock >= 0 );

//     sock_v_bind( sock, CONTROLLER_PORT );

//     thread_t_create( controller_server_thread,
//                      PSTR("controller_server"),
//                      0,
//                      0 );


//     thread_t_create( controller_state_thread,
//                      PSTR("controller_state_machine"),
//                      0,
//                      0 );

//     thread_t_create( controller_timeout_thread,
//                      PSTR("controller_timeout"),
//                      0,
//                      0 );
// }

// // static bool is_candidate( void ){

// // 	return controller_enabled && ip_b_is_zeroes( leader_ip );
// // }

// static void send_msg( uint8_t msgtype, uint8_t *msg, uint8_t len, sock_addr_t *raddr ){

// 	controller_header_t *header = (controller_header_t *)msg;

// 	header->magic 		= CONTROLLER_MSG_MAGIC;
// 	header->version 	= CONTROLLER_MSG_VERSION;
// 	header->msg_type 	= msgtype;
// 	header->reserved    = 0;

// 	sock_i16_sendto( sock, msg, len, raddr );
// }

// static void send_announce( void ){

// 	if( ( controller_state != STATE_CANDIDATE ) &&
// 		( controller_state != STATE_LEADER ) ){

// 		log_v_error_P( PSTR("cannot send announce, invalid state!") );

// 		return;
// 	}

//     uint16_t flags = 0;

//     if( controller_state == STATE_LEADER ){

//     	flags |= CONTROLLER_FLAGS_IS_LEADER;
//     }

// 	controller_msg_announce_t msg = {
// 		{ 0 },
// 		flags,
// 		get_priority(),
// 		get_follower_count(),
// 		// tmr_u64_get_system_time_us(),
// 		// cfg_u64_get_device_id(),
// 	};

// 	sock_addr_t raddr;
//     raddr.ipaddr = ip_a_addr(255, 255, 255, 255);
//     raddr.port = CONTROLLER_PORT;

// 	send_msg( CONTROLLER_MSG_ANNOUNCE, (uint8_t *)&msg, sizeof(msg), &raddr );
// }

// static void send_status( void ){

// 	if( controller_state == STATE_LEADER ){

// 		log_v_error_P( PSTR("cannot send status as leader!") );

// 		return;
// 	}

// 	if( ip_b_is_zeroes( leader_ip ) ){

// 		log_v_error_P( PSTR("cannot send status, no leader!") );

// 		return;
// 	}

// 	controller_msg_status_t msg = {
// 		{ 0 },
// 	};

// 	catbus_v_get_query( &msg.query );

// 	sock_addr_t raddr;
//     raddr.ipaddr = leader_ip;
//     raddr.port = CONTROLLER_PORT;

// 	send_msg( CONTROLLER_MSG_STATUS, (uint8_t *)&msg, sizeof(msg), &raddr );
// }

// static void send_leave( void ){

// 	if( controller_state == STATE_LEADER ){

// 		log_v_error_P( PSTR("cannot send leave as leader!") );

// 		return;
// 	}

// 	if( ip_b_is_zeroes( leader_ip ) ){

// 		log_v_error_P( PSTR("cannot send leave, no leader!") );

// 		return;
// 	}

// 	controller_msg_leave_t msg = {
// 		{ 0 },
// 	};

// 	sock_addr_t raddr;
//     raddr.ipaddr = leader_ip;
//     raddr.port = CONTROLLER_PORT;

// 	send_msg( CONTROLLER_MSG_LEAVE, (uint8_t *)&msg, sizeof(msg), &raddr );
// }

// static void send_drop( ip_addr4_t ip ){

// 	controller_msg_drop_t msg = {
// 		{ 0 },
// 	};

// 	sock_addr_t raddr;
//     raddr.ipaddr = ip;
//     raddr.port = CONTROLLER_PORT;

// 	send_msg( CONTROLLER_MSG_DROP, (uint8_t *)&msg, sizeof(msg), &raddr );
// }

// static void process_announce( controller_msg_announce_t *msg, sock_addr_t *raddr ){

// 	uint8_t reason = 0;
// 	bool update_leader = FALSE;

// 	// check if this is the first leader/candidate we've seen
// 	if( ip_b_is_zeroes( leader_ip ) ){

// 		reason = 1;
// 		update_leader = TRUE;
// 	}
// 	// check if already tracking this leader:
// 	else if( ip_b_addr_compare( raddr->ipaddr, leader_ip ) ){

// 		reason = 2;
// 		update_leader = TRUE;
// 	}
// 	// check better priority:
// 	else if( msg->priority > leader_priority ){

// 		reason = 3;
// 		update_leader = TRUE;
// 	}
// 	// check same priority
// 	else if( msg->priority == leader_priority ){

// 		// is follower count better?
// 		if( msg->follower_count > leader_follower_count ){

// 			reason = 4;
// 			update_leader = TRUE;	
// 		}
// 		// is follower count the same?
// 		else if( msg->follower_count == leader_follower_count ){

// 			// choose lowest IP to resolve an 
// 			// even follower count.
// 			uint32_t msg_ip_u32 = ip_u32_to_int( raddr->ipaddr );
// 			uint32_t leader_ip_u32 = ip_u32_to_int( leader_ip );

// 			if( msg_ip_u32 < leader_ip_u32 ){

// 				reason = 5;
// 				update_leader = TRUE;	
// 			}
// 		}
// 	}

// 	if( update_leader ){

// 		// check if switching leaders
// 		if( !ip_b_addr_compare( leader_ip, raddr->ipaddr ) ){

// 			log_v_debug_P( PSTR("Switching leader to: %d.%d.%d.%d from %d.%d.%d.%d reason: %d flags: 0x%0x followers: %d"),
// 				raddr->ipaddr.ip3, 
// 				raddr->ipaddr.ip2, 
// 				raddr->ipaddr.ip1, 
// 				raddr->ipaddr.ip0,
// 				leader_ip.ip3,
// 				leader_ip.ip2,
// 				leader_ip.ip1,
// 				leader_ip.ip0,
// 				reason,
// 				msg->flags,
// 				msg->follower_count
// 			);

// 			// check if we were *a* leader, but this one is better
// 			if( ip_b_addr_compare( cfg_ip_get_ipaddr(), leader_ip ) &&
// 					( controller_state == STATE_LEADER ) ){

// 				log_v_debug_P( PSTR("Dropping all followers") );

// 				// drop followers
// 				send_drop( ip_a_addr(255,255,255,255) );
// 				send_drop( ip_a_addr(255,255,255,255) );
// 				send_drop( ip_a_addr(255,255,255,255) );

// 				list_v_destroy( &follower_list );
// 			}
// 			// if we were tracking a pre-existing leader (that is not us),
// 			// inform it that we are leaving.
// 			else if( !ip_b_is_zeroes( leader_ip ) && !ip_b_addr_compare( cfg_ip_get_ipaddr(), leader_ip ) ){

// 				log_v_debug_P( PSTR("Leaving previous leader: %d.%d.%d.%d"),
// 					leader_ip.ip3,
// 					leader_ip.ip2,
// 					leader_ip.ip1,
// 					leader_ip.ip0
// 				);

// 				// byeeeeeeeeee
// 				send_leave();	
// 			}
// 		}
// 	}

// 	// changing leader, or updating current leader
// 	// avoids changing leader just because we received an announce
// 	if( update_leader || ip_b_addr_compare( raddr->ipaddr, leader_ip ) ){

// 		// if( !ip_b_addr_compare( cfg_ip_get_ipaddr(), raddr->ipaddr ) ){
			
// 		// 	log_v_debug_P( PSTR("vote: %d.%d.%d.%d"),
// 		// 		raddr->ipaddr.ip3, 
// 		// 		raddr->ipaddr.ip2, 
// 		// 		raddr->ipaddr.ip1, 
// 		// 		raddr->ipaddr.ip0
// 		// 	);
// 		// }

// 		vote( raddr->ipaddr, msg->priority, msg->follower_count, msg->flags );
// 	}
// }


// static void process_status( controller_msg_status_t *msg, sock_addr_t *raddr ){

// 	if( controller_state != STATE_LEADER ){

// 		// we are not a leader, we don't care about status.

// 		// this is mostly erroneous, the sender will
// 		// figure out we aren't a leader eventually.
// 		// we won't send a drop message in this case
// 		// since we aren't changing our own state.

// 		return;
// 	}

// 	// add or update this node's tracking information
// 	update_follower( raddr->ipaddr, msg );
// }

// static void process_leave( controller_msg_leave_t *msg, sock_addr_t *raddr ){

// 	if( controller_state != STATE_LEADER ){

// 		// we are are not a leader, we don't care about leave.

// 		return;
// 	}

// 	// remove this node from tracking
// 	remove_follower( raddr->ipaddr );
// }

// static void process_drop( controller_msg_drop_t *msg, sock_addr_t *raddr ){

// 	// check if this is from our leader
// 	if( ip_b_addr_compare( leader_ip, raddr->ipaddr ) ){

// 		log_v_debug_P( PSTR("Dropping leader: %d.%d.%d.%d"),
// 			raddr->ipaddr.ip3, 
// 			raddr->ipaddr.ip2, 
// 			raddr->ipaddr.ip1, 
// 			raddr->ipaddr.ip0
// 		);

// 		reset_leader();
// 		set_state( STATE_IDLE );
// 	}
// }

// PT_THREAD( controller_server_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
   	
//    	while(1){

//         THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

//         if( sys_b_is_shutting_down() ){

//         	THREAD_EXIT( pt );
//         }

//         if( sock_i16_get_bytes_read( sock ) <= 0 ){

//             goto end;
//         }

//         controller_header_t *header = sock_vp_get_data( sock );

//         // verify message
//         if( header->magic != CONTROLLER_MSG_MAGIC ){

//             goto end;
//         }

//         if( header->version != CONTROLLER_MSG_VERSION ){

//             goto end;
//         }

//         sock_addr_t raddr;
//         sock_v_get_raddr( sock, &raddr );

//         if( header->msg_type == CONTROLLER_MSG_ANNOUNCE ){

//         	process_announce( (controller_msg_announce_t *)header, &raddr );
//         }
//         else if( header->msg_type == CONTROLLER_MSG_DROP ){

//         	process_drop( (controller_msg_drop_t *)header, &raddr );
//         }
//         else if( header->msg_type == CONTROLLER_MSG_STATUS ){

//         	process_status( (controller_msg_status_t *)header, &raddr );
//         }
//         else if( header->msg_type == CONTROLLER_MSG_LEAVE ){

//         	process_leave( (controller_msg_leave_t *)header, &raddr );
//         }
//         else{

//         	// invalid message
//         	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
//         }


//     end:

//     	THREAD_YIELD( pt );
// 	}
    
// PT_END( pt );
// }


// PT_THREAD( controller_state_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

// 	static uint32_t start_time;

// 	// start in idle state, to wait for announce messages
// 	set_state( STATE_IDLE );
// 	reset_leader();

// 	// wait for wifi
// 	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

// 	log_v_debug_P( PSTR("controller idle") );

// 	// wait for timeout or leader/candidate is available
// 	thread_v_set_alarm( tmr_u32_get_system_time_ms() + CONTROLLER_IDLE_TIMEOUT * 1000 );
// 	// THREAD_WAIT_WHILE( thread_b_alarm_set() );
// 	THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && controller_state == STATE_IDLE );

// 	// on timeout, if still in idle state
// 	if( thread_b_alarm() && ( controller_state == STATE_IDLE ) ){
// 		// timeout!

// 		// check if leader is enabled:
// 		if( controller_enabled ){

// 			set_state( STATE_CANDIDATE );
// 			reset_leader();	
			
// 			vote_self();		
// 		}
// 		// else if( !ip_b_is_zeroes( leader_ip ) ){

// 		// 	// we have a candidate leader
// 		// 	set_state( STATE_VOTER );
			

// 		// }
// 		else{

// 			set_state( STATE_IDLE );
// 			reset_leader();	
// 		}
// 	}
// 	else{

// 		// state change
// 	}

// 	while( controller_state != STATE_IDLE ){

// 		// VOTER
// 		while( controller_state == STATE_VOTER ){

// 			// thread_v_set_alarm( tmr_u32_get_system_time_ms() + CONTROLLER_ELECTION_TIMEOUT * 1000 );
// 			// THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( controller_state == STATE_VOTER ) );
// 			THREAD_WAIT_WHILE( pt, ( controller_state == STATE_VOTER )  );

// 			// if( thread_b_alarm() ){
				
// 			// 	log_v_debug_P( PSTR("timeout") );

// 			// 	// reset to idle
// 			// 	set_state( STATE_IDLE );
// 			// 	reset_leader();
// 			// }
// 			// else{

// 			// 	log_v_debug_P( PSTR("state: %d"), controller_state );
// 			// }	
// 		}

// 		// FOLLOWER
// 		while( controller_state == STATE_FOLLOWER ){

// 			thread_v_set_alarm( tmr_u32_get_system_time_ms() + 2000 + ( rnd_u16_get_int() >> 5 )  ); // 2000 - 4048 ms
// 			THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( controller_state == STATE_FOLLOWER ) );

// 			if( controller_state == STATE_FOLLOWER ){
			
// 				send_status();
// 			}
// 		}

// 		// CANDIDATE
// 		start_time = tmr_u32_get_system_time_ms();
// 		while( controller_state == STATE_CANDIDATE ){

// 			// broadcast announcement
// 			send_announce();

// 			// random delay:
// 			thread_v_set_alarm( tmr_u32_get_system_time_ms() + 
// 				500 + ( rnd_u16_get_int() >> 7 )  ); // 500 - 1012 ms
			
// 			THREAD_WAIT_WHILE( pt, 
// 				thread_b_alarm_set() && 
// 				( controller_state == STATE_CANDIDATE ) );

// 			// check if we are leader after timeout
// 			if( ip_b_addr_compare( leader_ip, cfg_ip_get_ipaddr() ) &&
// 				tmr_u32_elapsed_time_ms( start_time ) > CONTROLLER_ELECTION_TIMEOUT * 1000 ){

// 				log_v_debug_P( PSTR("Electing self as leader") );

// 				set_state( STATE_LEADER );
// 			}
// 		}

// 		// LEADER
// 		while( controller_state == STATE_LEADER ){

// 			// broadcast announcement
// 			send_announce();

// 			// random delay:
// 			thread_v_set_alarm( tmr_u32_get_system_time_ms() + 
// 				500 + ( rnd_u16_get_int() >> 7 )  ); // 500 - 1012 ms

// 			THREAD_WAIT_WHILE( pt, 
// 				thread_b_alarm_set() && 
// 				( controller_state == STATE_LEADER ) );

// 		}
// 	}

// 	THREAD_RESTART( pt );
    
// PT_END( pt );
// }


// PT_THREAD( controller_timeout_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
   	
//    	while(1){

//    		THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

//    		thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );
//    		THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() && wifi_b_connected() );
   		
//    		if( sys_b_is_shutting_down() ){

//    			if( controller_state == STATE_FOLLOWER ){

//    				log_v_debug_P( PSTR("follower shutdown") );

//    				send_leave();
//    			}
//    			else if( controller_state == STATE_LEADER ){

//    				log_v_debug_P( PSTR("leader shutdown") );

//    				// drop followers
// 				send_drop( ip_a_addr(255,255,255,255) );
// 				send_drop( ip_a_addr(255,255,255,255) );
// 				send_drop( ip_a_addr(255,255,255,255) );
//    			}

//    			set_state( STATE_IDLE );
//    			reset_leader();

//         	THREAD_EXIT( pt );
//         }
//         else if( !wifi_b_connected() ){

//         	// wifi disconnected
//         	set_state( STATE_IDLE );
//    			reset_leader();

//    			THREAD_RESTART( pt );
//         }

//    		update_follower_timeouts();

//    		if( controller_state == STATE_LEADER ){
   			
//    			leader_follower_count = get_follower_count();
//    		}

//    		if( ( leader_timeout > 0 ) && 
//    			( !ip_b_addr_compare( leader_ip, cfg_ip_get_ipaddr() ) ) ){

//    			leader_timeout--;

//    			if( leader_timeout == 0 ){

//    				log_v_debug_P( PSTR("leader timeout") );

//    				set_state( STATE_IDLE );

//    				reset_leader();
//    			}
//    		}

//    		if( ( controller_state == STATE_LEADER ) ||
// 			( controller_state == STATE_FOLLOWER ) ){

// 			leader_uptime++;
// 		}
// 	}
    
// PT_END( pt );
// }




#endif
