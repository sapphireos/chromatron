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

#include "controller.h"
#include "link.h"

static socket_t sock;
static list_t link_list;

typedef struct __attribute__((packed)){
    uint8_t timeout;
    ip_addr4_t ip;
} link2_node_t;

typedef struct __attribute__((packed)){
    link2_t link;
    // link2_node_t follows
} link2_meta_t;


static list_t data_list;

typedef struct __attribute__((packed)){
    ip_addr4_t ip;
    uint8_t timeout;
    catbus_hash_t32 key;
	catbus_type_t8 data_type;
	// variable length data follows    
} link2_data_cache_t;


static uint8_t count_nodes_for_link( link2_meta_t *meta, uint16_t len ){

	ASSERT( len >= sizeof(link2_meta_t) );

	len -= sizeof(link2_meta_t);

	return len / sizeof(link2_node_t);
}

static bool link_has_ip( link2_meta_t *meta, uint16_t len, ip_addr4_t ip ){

	uint8_t count = count_nodes_for_link( meta, len );
	link2_node_t *node = (link2_node_t *)( meta + 1 );

	while( count > 0 ){

		if( ip_b_addr_compare( ip, node->ip ) ){

			return TRUE;
		}

		node++;
		count--;
	}

	return FALSE;
}

list_node_t _link2_mgr_l_lookup_by_hash( uint64_t hash ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link2_meta_t *state = list_vp_get_data( ln );

        if( link2_u64_hash( &state->link ) == hash ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

list_node_t _link2_mgr_update_link( link2_t *link, sock_addr_t *raddr ){

	link2_meta_t *meta = 0;
	list_node_t ln = _link2_mgr_l_lookup_by_hash( link2_u64_hash( link ) );

	if( ln < 0 ){

		// link not found
		ln = list_ln_create_node( 0, sizeof(link2_meta_t) + sizeof(link2_node_t) );

	    if( ln < 0 ){

	        return -1;
	    }

	    meta = (link2_meta_t *)list_vp_get_data( ln );
	    meta->link = *link;
	    
	    link2_node_t *node = (link2_node_t *)( meta + 1 );
	    node->ip = raddr->ipaddr;
	    node->timeout = LINK2_MGR_LINK_TIMEOUT;

	    list_v_insert_tail( &link_list, ln );    

	    log_v_debug_P( PSTR("Add new link: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );
	}
	

	// link found
	meta = (link2_meta_t *)list_vp_get_data( ln );
	// update node` list
	uint8_t node_count = count_nodes_for_link( meta, list_u16_node_size( ln ) );
	link2_node_t *node = (link2_node_t *)( meta + 1 );
	bool ip_found = FALSE;

	for( uint8_t i = 0; i < node_count; i++ ){

		if( ip_b_addr_compare( node->ip, raddr->ipaddr ) ){

			// log_v_debug_P( PSTR("found: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

			ip_found = TRUE;
			break;
		}

		node++;
	}

	if( !ip_found ){

		// reallocate and add new IP

		node_count++;
		list_node_t new_ln = list_ln_create_node( 0, sizeof(link2_meta_t) + node_count * sizeof(link2_node_t) );

		if( new_ln < 0 ){

	        return ln;
	    }

		link2_meta_t *new_meta = (link2_meta_t *)list_vp_get_data( new_ln );

		memcpy( new_meta, meta, list_u16_node_size( ln ) );

		// add node to end of list
		node = (link2_node_t *)( new_meta + 1 ) + ( node_count - 1 );
		node->ip = raddr->ipaddr;
		node->timeout = LINK2_MGR_LINK_TIMEOUT;

		log_v_debug_P( PSTR("Add new IP: %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

		list_v_insert_tail( &link_list, new_ln );    

		// release old node
		list_v_remove( &link_list, ln );
		list_v_release_node( ln );

		ln = new_ln;
	}	

	// reset timeout
	node->timeout = LINK2_MGR_LINK_TIMEOUT;


	return ln;
}



static int8_t _kv_i8_link_mgr_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){
		
		if( hash == __KV__link2_mgr_link_count ){
            
            STORE16(data, list_u8_count( &link_list ));
        }
        
        return 0;
    }

    return -1;
}

KV_SECTION_OPT kv_meta_t link_mgr_kv[] = {
    { CATBUS_TYPE_UINT16,   0, 0,               0, _kv_i8_link_mgr_handler,  "link2_mgr_link_count" }, 
};


static bool link_mgr_running = FALSE;

PT_THREAD( link2_mgr_server_thread( pt_t *pt, void *state ) );
PT_THREAD( link2_mgr_process_thread( pt_t *pt, void *state ) );

void link_mgr_v_start( void ){

	if( link_mgr_running ){

		return;
	}

	link_mgr_running = TRUE;

	list_v_init( &link_list );
	list_v_init( &data_list );

	kv_v_add_db_info( link_mgr_kv, sizeof(link_mgr_kv) );

	thread_t_create( link2_mgr_server_thread,
                 PSTR("link2_mgr_server"),
                 0,
                 0 );

	thread_t_create( link2_mgr_process_thread,
                 PSTR("link2_mgr_process"),
                 0,
                 0 );
}

void link_mgr_v_stop( void ){

	if( !link_mgr_running ){

		return;
	}

	link_mgr_running = FALSE;

	list_v_destroy( &link_list );
}


PT_THREAD( link2_mgr_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK2_MGR_PORT );

    // sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && ( link_mgr_running ) );

        if( !link_mgr_running ){

        	log_v_debug_P( PSTR("link mgr server stop") );

        	THREAD_EXIT( pt );
        }

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }

        int16_t bytes_read = sock_i16_get_bytes_read( sock );

        if( bytes_read <= 0 ){

            goto end;
        }

        link2_msg_header_t *header = sock_vp_get_data( sock );

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

        if( header->msg_type == LINK_MSG_TYPE_LINK ){

        	uint8_t count = ( sock_i16_get_bytes_read( sock ) - sizeof(link2_msg_header_t) ) / sizeof(link2_t);

        	// iterate through links
        	link2_t *link = (link2_t *)( header + 1 );

        	while( count > 0 ){

        		_link2_mgr_update_link( link, &raddr );

        		link++;
        		count--;
        	}
        }
        else if( header->msg_type == LINK_MSG_TYPE_DATA ){

        	bytes_read -= sizeof(link2_msg_header_t);
	        	
        	catbus_meta_t *meta = (catbus_meta_t *)( header + 1 );
        	uint8_t *data_ptr = (uint8_t *)( meta + 1 );

        	while( bytes_read > 0 ){

        		uint16_t data_len = type_u16_size( meta->type ) * ( meta->count + 1 );
        		int32_t *data = (int32_t *)data_ptr;

        		log_v_debug_P( PSTR("recv: 0x%0x %d %ld %d %d"), meta->hash, meta->type, *data, bytes_read, data_len );

        		data_ptr += data_len;
        		meta = (catbus_meta_t *)data_ptr;
        		data_ptr += sizeof(catbus_meta_t);
        		bytes_read -= sizeof(catbus_meta_t);
        		bytes_read -= data_len;

        		if( bytes_read < 0 ){

        			log_v_error_P( PSTR("data unpack error") );
        		}
        	}
        }
        else{

        	log_v_error_P( PSTR("unknown msg: %d"), header->msg_type );
        }

end:
    
        THREAD_YIELD( pt );
    }

PT_END( pt );
}



static void send_bind_msg( link2_binding_t *bindings, uint8_t count, ip_addr4_t ip ){

	mem_handle_t h = mem2_h_alloc( sizeof(link2_msg_header_t) + count * sizeof(link2_binding_t) );

 	if( h < 0 ){

 		log_v_error_P( PSTR("alloc fail") );

 		return;
 	}

	link2_msg_header_t *hdr = (link2_msg_header_t *)mem2_vp_get_ptr_fast( h );

	link2_v_init_header( hdr, LINK_MSG_TYPE_BIND );

	link2_binding_t *binding_ptr = (link2_binding_t *)( hdr + 1 );

	memcpy( binding_ptr, bindings, count * sizeof(link2_binding_t) );

	sock_addr_t raddr = {
		ip,
		LINK2_PORT
	};

		// send this message:
	if( sock_i16_sendto_m( sock, h, &raddr ) < 0 ){

		log_v_error_P( PSTR("msg fail") );
	}

	// log_v_debug_P( PSTR("send bind: %d.%d.%d.%d"), ip.ip3, ip.ip2, ip.ip1, ip.ip0 );
}

PT_THREAD( link2_mgr_process_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

    	thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );

    	THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && link_mgr_running );

    	if( !link_mgr_running ){

        	log_v_debug_P( PSTR("link mgr server stop") );

        	THREAD_EXIT( pt );
        }

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            THREAD_EXIT( pt );
        }

    	/*
		
		Sources and Sinks

		Source: a source node for a link.  Sends data to manager.

		Sink: a destination node for a link. Receives data from manager.	
		

		Send link:
			ip: source node
			query: sink node
		
		Recv Link:
			ip: sink node
			query: source node
		
		
		
		Messages:
		LINK: This node has these links
		BIND:
			SOURCE: This node becomes a source for target key
			SINK: This node becomes a destination for target key
	
			The BIND message contains a list of source keys
			and sink keys.

		DECLINE: Node indicates a key not available
		DATA: KV data from source or manager
			Data flows source -> manager -> sink
		SHUTDOWN:  Shutdown a specific link, or all links (node going offline)
			If received by manager, removes node from all or specified links.
			If received by a node from manager, cancels all sources/sinks.

		Nodes track if they are a source or sink for an item.
		Source/sink status times out and must be refreshed.
					
		
		Set up sources and sinks:

		For each node in controller DB:
			# lists of keys
			sources = []
			sinks = [] 

			For each send link:
				If IP matches link:
					add to sources

			For each recv link:
				If query matches link:
					add to sinks			
			
			Deduplicate source/sink lists
			
			Transmit BIND to node.
		
		N nodes
		S send links
		R recv links

		O(N * (S + R))
		

		*/

    	// set up bindings
    	link2_binding_t bindings[LINK_MAX_BIND_ENTRIES];

    	controller_db_v_reset_iter();

    	follower_t *node = controller_db_p_get_next();

    	while(node != 0){

    		uint16_t count = 0;
    		memset( bindings, 0, sizeof(bindings) );

    		// loop through links

		    list_node_t ln = link_list.head;

		    while( ln >= 0 ){

		        link2_meta_t *meta = list_vp_get_data( ln );

				if( meta->link.mode == LINK_MODE_SEND ){

					// check if node IP matches this link
					if( link_has_ip( meta, list_u16_node_size( ln ), node->ip ) ){

						link2_binding_t binding = {
							meta->link.source_key,
							meta->link.rate,
							LINK_BIND_FLAG_SOURCE,
							0
						};

						bindings[count] = binding;

						count++;
					}
				}
				else if( meta->link.mode == LINK_MODE_RECV ){

					// check if node query matches this link
					if( catbus_b_query_tags( &meta->link.query, &node->tags ) ){

						link2_binding_t binding = {
							meta->link.dest_key,
							meta->link.rate,
							LINK_BIND_FLAG_SINK,
							0
						};

						bindings[count] = binding;

						count++;
					}
				}

				if( count >= LINK_MAX_BIND_ENTRIES ){

					send_bind_msg( bindings, count, node->ip );

			     	count = 0;
				}

		        ln = list_ln_next( ln );
		    }

		    if( count > 0 ){

		    	// transmit
		    	send_bind_msg( bindings, count, node->ip );

		    	count = 0;
		    }

    		node = controller_db_p_get_next();
    	}


		/*

		Manager receives a DATA message:
	
		Data came from a source.  We need to find matching links
		and store/aggregate the data.
		The data routing is done in two steps, so for the first step
		we don't need to worry about transmissions to sinks.

		For each data item:
			For each send link:
				If item and IP is source for link:
					Record data for link and aggregate.

			For each recv link:
				If item and query matches link:
					Record data for link and aggregate.

		
		Periodic transmissions to sinks:
		
		Manager is sending data to sinks.
		This is the second step and is decoupled from the first.
		

		For each node in controller DB:
		
			# list of data items
			data = [] 

			For each send link:
				If query matches link and data timer is ready:
					add link data to data set

			For each recv link:
				If IP matches link and data timer is ready:
					add link data to data set

			Deduplicate data set.

			Transmit data set to node
			Split into multiple messages as needed.
			
			
			
		
			
			
    	*/








    }

PT_END( pt );
}





