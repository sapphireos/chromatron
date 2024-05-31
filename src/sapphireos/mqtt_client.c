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

Topic wildcards:




*/



#ifdef ENABLE_CONTROLLER

typedef struct __attribute__((packed)){
	char topic[MQTT_MAX_TOPIC_LEN];
	uint32_t kv_hash;
	// uint8_t qos;
	mqtt_on_publish_callback_t callback;
	uint8_t tag;
} mqtt_sub_t;

static list_t sub_list;

static socket_t sock;

#ifdef ENABLE_BROKER
static socket_t broker_sock;
#endif

static ip_addr4_t broker_ip;
static uint16_t broker_port;

KV_SECTION_META kv_meta_t mqtt_client_kv[] = {
	#ifdef ENABLE_BROKER
	{ CATBUS_TYPE_BOOL, 	0, KV_FLAGS_PERSIST, 0, 						0,  "mqtt_broker_enable" },
    #endif
    { CATBUS_TYPE_IPv4, 	0, KV_FLAGS_PERSIST, &broker_ip, 				0,  "mqtt_broker_ip" },
    { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_PERSIST, &broker_port,				0,  "mqtt_broker_port" },
};


PT_THREAD( mqtt_client_thread( pt_t *pt, void *state ) );
PT_THREAD( mqtt_client_server_thread( pt_t *pt, void *state ) );

#ifdef ENABLE_BROKER
PT_THREAD( mqtt_broker_server_thread( pt_t *pt, void *state ) );
PT_THREAD( mqtt_broker_timeout_thread( pt_t *pt, void *state ) );
#endif

void mqtt_client_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

		return;
    }

    list_v_init( &sub_list );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, MQTT_BRIDGE_PORT );
    sock_v_set_timeout( sock, 1 );

    thread_t_create( mqtt_client_thread,
                     PSTR("mqtt_client"),
                     0,
                     0 );

    thread_t_create( mqtt_client_server_thread,
                     PSTR("mqtt_client_server"),
                     0,
                     0 );

   	#ifdef ENABLE_BROKER
    if( kv_b_get_boolean( __KV__mqtt_broker_enable ) ){

    	thread_t_create( mqtt_broker_server_thread,
                     PSTR("mqtt_broker_server"),
                     0,
                     0 );

    	thread_t_create( mqtt_broker_timeout_thread,
                     PSTR("mqtt_broker_timeout"),
                     0,
                     0 );
    }
    #endif
}

static sock_addr_t get_broker_raddr( void ){
	
	sock_addr_t raddr = {
		.ipaddr = broker_ip,
		.port = broker_port
	};

	return raddr;
}

static int16_t send_msg_to_broker( mem_handle_t h ){

	sock_addr_t raddr = get_broker_raddr();

	if( ip_b_is_zeroes( raddr.ipaddr) ){

		mem2_v_free( h );

		return -1;
	}

	return sock_i16_sendto_m( sock, h, &raddr );
}

static int16_t send_msg_to_broker_ptr( uint8_t *data, uint16_t len ){

	sock_addr_t raddr = get_broker_raddr();

	if( ip_b_is_zeroes( raddr.ipaddr) ){

		return -1;
	}

	return sock_i16_sendto( sock, data, len, &raddr );	
}

static int8_t publish( 
	uint8_t msgtype, 
	const char *topic, 
	const void *data, 
	uint16_t data_len, 
	uint8_t qos, 
	bool retain ){

	uint8_t topic_len = strnlen( topic, MQTT_MAX_TOPIC_LEN );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	uint16_t msg_len = sizeof(mqtt_msg_publish_t) + sizeof(uint16_t) + data_len + sizeof(uint8_t) + topic_len + 1;

	mem_handle_t h = mem2_h_alloc( msg_len );

	if( h < 0 ){

		return -1;
	}

	mqtt_msg_publish_t *msg = (mqtt_msg_publish_t *)mem2_vp_get_ptr_fast( h );

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// start with topic len, adding 1 for the null temr
	*ptr = topic_len + 1;
	ptr++;

	// copy topic
	memcpy( ptr, topic, topic_len );
	ptr += topic_len;

	// null terminate topic
	*ptr = 0;
	ptr++;

	// payload len, for generic publish
	if( msgtype == MQTT_MSG_PUBLISH ){

		memcpy( ptr, &data_len, sizeof(data_len) );
		ptr += sizeof(data_len);
	}

	// payload
	memcpy( ptr, data, data_len );
	
	// header
	mqtt_msg_header_t *header = (mqtt_msg_header_t *)msg;

	header->magic 		= MQTT_MSG_MAGIC;
	header->version 	= MQTT_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->qos    		= qos;
	header->flags       = 0;

	// transmit
	if( send_msg_to_broker( h ) < 0 ){

		log_v_error_P( PSTR("Send failed") );

		return -2;
	}

	return 0;
}


bool mqtt_b_match_topic( const char *topic, const char *sub ){

    /*

    Wildcards:

    + matches at that level of hierarchy.
    # matches all remaining levels.


    Thus, if we are a match so far and get to a #, we have a match.

    The sub is matched against the topic.  The sub can have wildcards,
    but the topic can't.  Typical usage would be passing the published
    topic as topic and the subscription topic to match against is sub.

    */

    // check lengths of inputs:
    if( strnlen( topic, MQTT_MAX_TOPIC_LEN ) >= MQTT_MAX_TOPIC_LEN ){

        return FALSE;
    }

    if( strnlen( sub, MQTT_MAX_TOPIC_LEN ) >= MQTT_MAX_TOPIC_LEN ){

        return FALSE;
    }


    // check for nulls
    if( ( *topic == 0 ) || ( *sub == 0 ) ){

        return FALSE;
    }

    while( ( *topic != 0 ) || ( *sub != 0 ) ){

        // printf("%c %c\n", *topic, *sub);

        // topic cannot have wildcards, only sub can:
        if( ( *topic == '+' ) || ( *topic == '#' ) ){

            return FALSE;
        }
        // check for #, this matches everything after this point, so we can return now
        else if( *sub == '#' ){

            // # wildcard must be the end of the string
            if( sub[1] != 0 ){

                return FALSE;
            }

            return TRUE;
        }
        else if( *sub == '+'){

            // printf("meow +\n");

            // match everything in topic at this level until the next /
            // and then move on to the next level
            // OR
            // if topic gets to null before a / (IE the string ends), we match
            while( *topic != '/' ){

                if( *topic == 0 ){

                    return TRUE;
                }

                topic++;
            }

            sub++;

            continue;
        }
        // check for character mismatch
        else if( *topic != *sub ){

            return FALSE;
        }

        topic++;
        sub++;
    }

    if( *topic != *sub ){

        return FALSE;
    }

    return TRUE;
}

// int8_t mqtt_client_i8_publish_data( 
// 	const char *topic, 
// 	catbus_meta_t *meta, 
// 	const void *data, 
// 	uint8_t qos, bool 
// 	retain ){

// 	// watch for possible stack overflows if we increase this
// 	uint8_t buf[MQTT_MAX_PAYLOAD_LEN];	

// 	uint16_t payload_len = sizeof(catbus_meta_t);

// 	memcpy( buf, meta, payload_len );

// 	uint16_t data_len = type_u16_size( meta->type );
// 	payload_len += data_len;

// 	memcpy( &buf[sizeof(catbus_meta_t)], data, data_len );

// 	return publish( MQTT_MSG_PUBLISH_KV, topic, buf, payload_len, qos, retain );
// }

int8_t mqtt_client_i8_publish( 
	const char *topic, 
	const void *data, 
	uint16_t data_len, 
	uint8_t qos, 
	bool retain ){

	return publish( MQTT_MSG_PUBLISH, topic, data, data_len, qos, retain );
}

int8_t mqtt_client_i8_publish_kv( 
	const char *topic, 
	const char *key, 
	uint8_t qos, 
	bool retain ){

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

int8_t transmit_subscribe( 
	uint8_t msgtype, 
	const char *topic, 
	catbus_meta_t *meta, 
	uint8_t qos ){

	uint16_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	uint16_t msg_len = sizeof(mqtt_msg_subscribe_t) + sizeof(uint8_t) + topic_len + 1; // add 1 for null terminator

	if( msgtype == MQTT_MSG_SUBSCRIBE_KV ){
		// pad length for metadata

		ASSERT( meta != 0 );

		msg_len += sizeof(catbus_meta_t);
	}

	mem_handle_t h = mem2_h_alloc( msg_len );

	if( h < 0 ){

		return -1;
	}

	mqtt_msg_subscribe_t *msg = (mqtt_msg_subscribe_t *)mem2_vp_get_ptr_fast( h );

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)(msg + 1);

	// start with topic, adding 1 for null terminator
	*ptr = topic_len + 1;
	ptr++;

	// copy topic
	memcpy( ptr, topic, topic_len );
	ptr += topic_len;

	// add null term
	*ptr = 0;
	ptr++;

	// if a KV message, attach meta data
	if( msgtype == MQTT_MSG_SUBSCRIBE_KV ){

		memcpy( ptr, meta, sizeof(catbus_meta_t) );
		ptr += sizeof(catbus_meta_t);
	}
		
	// attach header
	mqtt_msg_header_t *header = (mqtt_msg_header_t *)msg;

	header->magic 		= MQTT_MSG_MAGIC;
	header->version 	= MQTT_MSG_VERSION;
	header->msg_type 	= msgtype;
	header->qos    		= qos;
	header->flags       = 0;

	if( send_msg_to_broker( h ) < 0 ){

		log_v_error_P( PSTR("Send failed") );

		return -2;
	}

	return 0;
}

// subscribe with callback
int8_t mqtt_client_i8_subscribe( 
	const char *topic, 
	uint8_t qos, 
	mqtt_on_publish_callback_t callback,
	uint8_t tag ){

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
		tag, // tag
	}; 

	strncpy( new_sub.topic, topic, topic_len );

    ln = list_ln_create_node2( &new_sub, sizeof(new_sub), MEM_TYPE_MQTT_SUB );

    if( ln < 0 ){

    	log_v_error_P( PSTR("failed to add sub") );

        return -1;
    }

    list_v_insert_tail( &sub_list, ln );

	transmit_subscribe( MQTT_MSG_SUBSCRIBE, topic, 0, qos );

	return 0;
}

// subscribe with KV linkage
int8_t mqtt_client_i8_subscribe_kv( 
	const char *topic, 
	const char *key, 
	uint8_t qos,
	uint8_t tag ){

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

    catbus_meta_t meta;

	if( kv_i8_get_catbus_meta( kv_hash, &meta ) < 0 ){

		log_v_error_P( PSTR("Key: %s not found"), key );

		return -1;
	}
	
	mqtt_sub_t new_sub = {
		{ 0 },
		kv_hash, // kv hash
		// qos,
		0, // callback
		tag, // tag
	}; 

	strncpy( new_sub.topic, topic, topic_len );

	log_v_info_P(PSTR("%s %s"), topic, key);

    ln = list_ln_create_node2( &new_sub, sizeof(new_sub), MEM_TYPE_MQTT_SUB );

    if( ln < 0 ){

    	log_v_error_P( PSTR("failed to add sub") );

        return -1;
    }

    list_v_insert_tail( &sub_list, ln );

	transmit_subscribe( MQTT_MSG_SUBSCRIBE_KV, topic, &meta, qos );

	return 0;
}

void mqtt_client_v_unsubscribe( const char *topic ){

	uint16_t topic_len = strlen( topic );
	ASSERT( topic_len <= MQTT_MAX_TOPIC_LEN );

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        list_node_t next_ln = list_ln_next( ln );

        if( strncmp( topic, sub->topic, MQTT_MAX_TOPIC_LEN ) == 0 ){

            // remove from list
            list_v_remove( &sub_list, ln );
         	list_v_release_node( ln );   
        }

        ln = next_ln; 
    }
}

void mqtt_client_v_unsubscribe_tag( uint8_t tag ){

	list_node_t ln = sub_list.head;

    while( ln >= 0 ){

        mqtt_sub_t *sub = list_vp_get_data( ln );
        list_node_t next_ln = list_ln_next( ln );

        if( sub->tag == tag ){

            // remove from list
            list_v_remove( &sub_list, ln );
         	list_v_release_node( ln );   
        }

        ln = next_ln; 
    }
}

static void transmit_status( void ){

	uint32_t pixel_power = 0;

	kv_i8_get( __KV__pixel_power, &pixel_power, sizeof(pixel_power) );

	catbus_query_t tags;
	catbus_v_get_query( &tags );

	// mqtt_msg_publish_status_t msg = {
	mqtt_msg_status_t msg = {
		// { 0 },
		tags,
		cfg_ip_get_ipaddr(),
		sys_u8_get_mode(),
		tmr_u32_get_system_time_ms(),
		wifi_i8_rssi(),
		wifi_i8_get_channel(),
		thread_u8_get_cpu_percent(),
		mem2_u16_get_used(),
		pixel_power,		
	};

	mqtt_client_i8_publish( PSTR("chromatron_mqtt/status"), (uint8_t *)&msg, sizeof(msg), 0, 1 );

	// msg.header.magic 		= MQTT_MSG_MAGIC;
	// msg.header.version 		= MQTT_MSG_VERSION;
	// msg.header.msg_type 	= MQTT_MSG_PUBLISH_STATUS;
	// msg.header.qos    		= 0;
	// msg.header.flags       	= 0;

	// send_msg_to_broker_ptr( (uint8_t *)&msg, sizeof(msg) );
}

static void transmit_shutdown( void ){

	mqtt_msg_shutdown_t msg = {
		{ 0 },
	};

	msg.header.magic 		= MQTT_MSG_MAGIC;
	msg.header.version 		= MQTT_MSG_VERSION;
	msg.header.msg_type 	= MQTT_MSG_SHUTDOWN;
	msg.header.qos    		= 0;
	msg.header.flags       	= 0;

	send_msg_to_broker_ptr( (uint8_t *)&msg, sizeof(msg) );
}

static void mqtt_on_publish_status_callback( char *topic, uint8_t *data, uint16_t data_len ){

	// int32_t value;

	// coert to int32 for debug
	// memcpy( &value, data, sizeof(value) );	

	log_v_debug_P( PSTR("%s %ld"), topic, data_len );
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


PT_THREAD( mqtt_client_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	static uint32_t counter;
	counter = 0;

	THREAD_WAIT_WHILE( pt, !wifi_b_connected() );
   	
	TMR_WAIT( pt, 1000 );	

	// mqtt_client_i8_subscribe( PSTR("chromatron_mqtt/test_sub"), 0, mqtt_on_publish_callback );
	mqtt_client_i8_subscribe( PSTR("chromatron_mqtt/status"), 0, mqtt_on_publish_status_callback, 0 );

   	while(1){

    	TMR_WAIT( pt, 2000 );

    	// send subscriptions
		list_node_t ln = sub_list.head;	

	    while( ln >= 0 ){

	        mqtt_sub_t *sub = list_vp_get_data( ln );

	        catbus_meta_t meta = { 0 };

	        if( sub->callback != 0 ){

	        	if( transmit_subscribe( MQTT_MSG_SUBSCRIBE, sub->topic, 0, 0 ) < 0 ){
		        
		        	log_v_debug_P( PSTR("sub fail") );

		        	goto next_sub;
		       	}
	        }
	        else if( sub->kv_hash != 0 ){

				if( kv_i8_get_catbus_meta( sub->kv_hash, &meta ) < 0 ){

					log_v_error_P( PSTR("Key for topic: %s not found"), sub->topic );

					goto next_sub;
				}
			
		        if( transmit_subscribe( MQTT_MSG_SUBSCRIBE_KV, sub->topic, &meta, 0 ) < 0 ){
		        
		        	log_v_debug_P( PSTR("sub fail") );

		        	goto next_sub;
		       	}
		    }
		    else{

		    	log_v_error_P( PSTR("invalid sub config") );
		    }

next_sub:
	        ln = list_ln_next( ln );        
	    }    	

	    // char *test_data = "{data:1.0}";
	    // mqtt_client_i8_publish( "chromatron_mqtt/publish", test_data, strlen(test_data), 0, FALSE );

		transmit_status();
	}
    
PT_END( pt );
}


PT_THREAD( mqtt_client_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
   	
   	while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sys_b_is_shutting_down() ){

        	transmit_shutdown();
        	TMR_WAIT( pt, 100 );
        	transmit_shutdown();
        	TMR_WAIT( pt, 100 );
        	transmit_shutdown();

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
        else if( header->msg_type == MQTT_MSG_BRIDGE ){

        	broker_ip = raddr.ipaddr;
        	broker_port = raddr.port;
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



#ifdef ENABLE_BROKER
/**********************************
			 BROKER
**********************************/
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


KV_SECTION_OPT kv_meta_t mqtt_broker_kv[] = {
    { CATBUS_TYPE_UINT16, 	0, KV_FLAGS_READ_ONLY, 0,				_broker_kv_handler,  "mqtt_broker_sub_count" },
};


static void broker_process_publish( mqtt_msg_publish_t *msg, sock_addr_t *raddr, mem_handle_t packet_h ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	uint8_t topic_len = *ptr;
	ptr++;
	char *topic = (char *)ptr;
		
	// got the topic
	// we don't care about the data

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

        mqtt_broker_sub_t *sub = list_vp_get_data( ln );
        
        if( strncmp( topic, sub->topic, topic_len ) == 0 ){

            // match!

        	// deref packet handle
        	uint16_t packet_size = mem2_u16_get_size( packet_h );
        	void *packet_ptr = mem2_vp_get_ptr( packet_h );

        	// retransmit this message
    		if( sock_i16_sendto( broker_sock, packet_ptr, packet_size, &sub->raddr ) < 0 ){

    			log_v_error_P( PSTR("MQTT packet flinging failed") );

    			break;
    		}
        }

        ln = list_ln_next( ln );        
    }
}


// static void broker_process_publish_kv( mqtt_msg_publish_t *msg, sock_addr_t *raddr ){

	
// }

static void broker_process_subscribe( mqtt_msg_subscribe_t *msg, sock_addr_t *raddr ){

	// get byte pointer after headers:
	uint8_t *ptr = (uint8_t *)( msg + 1 );

	// get topic length
	uint8_t topic_len = *ptr;
	ptr++;
	char *topic = (char *)ptr;

	list_node_t ln = broker_sub_list.head;

    while( ln >= 0 ){

        mqtt_broker_sub_t *sub = list_vp_get_data( ln );

        // log_v_debug_P( PSTR("%s %d %d.%d.%d.%d"), sub->topic, mqtt_b_match_topic( topic, sub->topic ), sub->raddr.ipaddr.ip3, sub->raddr.ipaddr.ip2, sub->raddr.ipaddr.ip1, sub->raddr.ipaddr.ip0 );
        
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
 	
   	kv_v_add_db_info( mqtt_broker_kv, sizeof(mqtt_broker_kv) );

   	// create socket
    broker_sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( broker_sock >= 0 );

    sock_v_bind( broker_sock, MQTT_BROKER_PORT );
    sock_v_set_timeout( broker_sock, 1 );

    
    list_v_init( &broker_sub_list );


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

        mem_handle_t packet_h = sock_h_get_data_handle( broker_sock );

        ASSERT( packet_h > 0 );

        if( header->msg_type == MQTT_MSG_PUBLISH ){

        	broker_process_publish( (mqtt_msg_publish_t *)header, &raddr, packet_h );
        }
        // else if( header->msg_type == MQTT_MSG_PUBLISH_KV ){

        // 	broker_process_publish_kv( (mqtt_msg_publish_t *)header, &raddr );
        // }
        else if( header->msg_type == MQTT_MSG_SUBSCRIBE ){

        	broker_process_subscribe( (mqtt_msg_subscribe_t *)header, &raddr );
        }
        // else if( header->msg_type == MQTT_MSG_SUBSCRIBE_KV ){

        // 	broker_process_subscribe_kv( (mqtt_msg_subscribe_t *)header, &raddr );
        // }
        else if( header->msg_type == MQTT_MSG_BRIDGE ){

        	// broker_ip = raddr.ipaddr;
        }
        else{

        	// invalid message
        	log_v_error_P( PSTR("Invalid msg: %d"), header->msg_type );
        }

        // release original handle
		mem2_v_free( packet_h );

    end:

    	THREAD_YIELD( pt );
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


/*

Test cases for match topic:


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define FALSE false
#define TRUE true

#define MQTT_MAX_TOPIC_LEN      16

typedef struct{
    char *topic;
    char *sub;
    bool should_match;
} mqtt_match_test_t;

static mqtt_match_test_t tests[] = {
    {"a/b/c/d", "a/b/c/d", TRUE},
    {"a/b/c/d", "+/b/c/d", TRUE},
    {"a/b/c/d", "a/+/c/d", TRUE},
    {"a/b/c/d", "a/+/+/d", TRUE},
    {"a/b/c/d", "+/+/+/+", TRUE},
    {"a/b/c/d", "a/b/c",   FALSE},
    {"a/b/c/d", "b/+/c/d", FALSE},
    {"a/b/c/d", "+/+/",    FALSE},
    {"a/b/c/d", "#",       TRUE},
    {"a/b/c/d", "a/#",     TRUE},
    {"a/b/c/d", "a/b/#",   TRUE},
    {"a/b/c/d", "a/b/c/#", TRUE},
    {"a/b/c/d", "+/b/c/#", TRUE},

    {"aa/bb/cc/dd", "aa/bb/cc/dd", TRUE},
    {"aa/bb/cc/dd", "+/bb/cc/dd", TRUE},
    {"aa/bb/cc/dd", "aa/+/cc/dd", TRUE},
    {"aa/bb/cc/dd", "aa/+/+/dd", TRUE},
    {"aa/bb/cc/dd", "+/+/+/+", TRUE},
    {"aa/bb/cc/dd", "aa/bb/c",   FALSE},
    {"aa/bb/cc/dd", "bb/+/cc/dd", FALSE},
    {"aa/bb/cc/dd", "+/+/",    FALSE},
    {"aa/bb/cc/dd", "#",       TRUE},
    {"aa/bb/cc/dd", "aa/#",     TRUE},
    {"aa/bb/cc/dd", "aa/bb/#",   TRUE},
    {"aa/bb/cc/dd", "aa/bb/cc/#", TRUE},
    {"aa/bb/cc/dd", "+/bb/cc/#", TRUE},

    {"aa/bb/cc/dd", "+/+/#",    TRUE},
    {"aa/bb/cc/dd", "aa/bb/cc/#d", FALSE},

    {"aa/bb/cc/dd", "aa/bb/cc/dd", TRUE},
    {"aa/bb/cc/dd", "aa/+/cc/dd", TRUE},
    {"aa/bb/cc/dd", "aa/bb/+/dd", TRUE},
    {"aa/bb/cc/de", "aa/bb/cc/+", TRUE},
    {"aa/bb/cc/dd", "aa/#", TRUE},

    {"a//topic", "a/+/topic", TRUE},
    {"/a/topic", "+/a/topic", TRUE},
    {"/a/topic", "#", TRUE},
    {"/a/topic", "/#", TRUE},

    {"a/topic/", "a/topic/+", TRUE},
    {"a/topic/", "a/topic/#", TRUE},

    {"aaaabbbbccccdddd", "#", FALSE},
    {"aaaabbbbccccddd", "aaaabbbbccccddd#", FALSE},
    
};

int main(void) {
    
    for(int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++){

        char* pass_fail = "FAIL";

        if(mqtt_b_match_topic(tests[i].topic, tests[i].sub) == tests[i].should_match){

            pass_fail = "PASS";
        }

        printf("%16s -> %16s: %s\n", tests[i].topic, tests[i].sub, pass_fail);
    }

    return 0;
}

*/

#endif
