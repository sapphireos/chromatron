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

#ifdef ENABLE_CONTROLLER

static socket_t sock;
static list_t link_list;
static list_t binding_list;

typedef struct __attribute__((packed)){
    catbus_hash_t32 key;
    uint16_t rate;
    int16_t ticks;
    uint8_t flags;
    uint8_t timeout;
} binding_state_t;

static int32_t link2_test_key;
static int32_t link2_test_key2;

static uint8_t get_binding_count( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    return list_u8_count( &binding_list );
}


static uint8_t get_link_count( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    return list_u8_count( &link_list );
}


static int8_t _kv_i8_link_client_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        if( hash == __KV__link2_binding_count ){
            
            STORE16(data, get_binding_count());
        }
        else if( hash == __KV__link2_link_count ){
            
            STORE16(data, get_link_count());
        }
        
        return 0;
    }

    return -1;
}

KV_SECTION_META kv_meta_t link2_kv[] = {
    { CATBUS_TYPE_UINT16,  0, 0,               0, _kv_i8_link_client_handler,  "link2_binding_count" },
    { CATBUS_TYPE_UINT16,  0, 0,               0, _kv_i8_link_client_handler,  "link2_link_count" },

    { CATBUS_TYPE_INT32,   0, 0,                   &link2_test_key,        0,  "link2_test_key" },
    { CATBUS_TYPE_INT32,   0, 0,                   &link2_test_key2,       0,  "link2_test_key2" },
};


PT_THREAD( link2_client_thread( pt_t *pt, void *state ) );
PT_THREAD( link2_server_thread( pt_t *pt, void *state ) );

void link2_v_init( void ){

	list_v_init( &link_list );
    list_v_init( &binding_list );

	#ifdef ESP8266
	catbus_query_t query = {
		{
            __KV__controller_test,
        },
	};

	link2_l_create(
		LINK_MODE_SEND,
		__KV__link2_test_key,
		__KV__link2_test_key2,
		&query,
		0,
		LINK_RATE_1000ms,
		LINK_AGG_ANY,
		LINK_FILTER_OFF
	);

	#endif

	thread_t_create( link2_server_thread,
                 PSTR("link2_server"),
                 0,
                 0 );

	thread_t_create( link2_client_thread,
                 PSTR("link2_client"),
                 0,
                 0 );

}

link2_state_t* link2_ls_get_data( link2_handle_t link ){

    return list_vp_get_data( link );    
}

bool link2_b_compare( link2_state_t *link1, link2_state_t *link2 ){
 
	return memcmp( link1, link2, sizeof(link2_state_t) ) == 0;
}

uint64_t link2_u64_hash( link2_t *link ){

	return hash_u64_data( (uint8_t *)link, sizeof(link2_t) );	
}

link2_handle_t link2_l_lookup( link2_state_t *link ){

	list_node_t ln = link_list.head;

	while( ln >= 0 ){

		link2_state_t *state = list_vp_get_data( ln );

		if( link2_b_compare( link, state ) ){

			return ln;
		}

		ln = list_ln_next( ln );
	}

	return -1;
}

link2_handle_t link2_l_lookup_by_hash( uint64_t hash ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link2_state_t *state = list_vp_get_data( ln );

        if( state->hash == hash ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}

link2_t link2_ls_assemble(
	link_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter ){

	link2_t state = {
		.mode 				= mode,
		.source_key 		= source_key,
		.dest_key 			= dest_key,
		.query 				= *query,
		.tag 				= tag,
		.rate 			    = rate,
		.aggregation 		= aggregation,
		// .filter 			= filter,
	};

	return state;
}

link2_handle_t link2_l_create( 
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

	link2_t link = link2_ls_assemble(
							mode,
							source_key,
							dest_key,
							query,
							tag,
							rate,
							aggregation,
							filter );

	link2_state_t state = { 0 };
	state.link = link;

    return link2_l_create2( &state );
}


link2_handle_t link2_l_create2( link2_state_t *state ){ 

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return -1;
    }

    link2_handle_t lh = link2_l_lookup( state );

    if( lh > 0 ){

        return lh;
    }

    if( list_u8_count( &link_list ) >= LINK_MAX_LINKS ){

        log_v_error_P( PSTR("Too many links") );

        return -1;
    }

    catbus_meta_t meta;

    // check if we have the required keys
    if( state->link.mode == LINK_MODE_SEND ){

        if( kv_i8_get_catbus_meta( state->link.source_key, &meta ) < 0 ){
        
            return -1;
        }
    }
    else if( state->link.mode == LINK_MODE_RECV ){

        if( kv_i8_get_catbus_meta( state->link.dest_key, &meta ) < 0 ){
        
            return -1;
        }
    }
    else if( state->link.mode == LINK_MODE_SYNC ){

        if( state->link.source_key != state->link.dest_key ){

            return -1;
        }

        if( kv_i8_get_catbus_meta( state->link.source_key, &meta ) < 0 ){
        
            return -1;
        }

        if( kv_i8_get_catbus_meta( state->link.dest_key, &meta ) < 0 ){
        
            return -1;
        }

        state->link.aggregation = LINK_AGG_ANY;
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
        if( state->link.aggregation != LINK_AGG_ANY ){

            log_v_error_P( PSTR("link system only supports ANY aggregation for array types") );

            return -1;
        }
    }

    if( state->link.rate < LINK_RATE_MIN ){

        state->link.rate = LINK_RATE_MIN;
    }
    else if( state->link.rate > LINK_RATE_MAX ){

        state->link.rate = LINK_RATE_MAX;
    }

    // sort tags from highest to lowest.
    // this is because the tags are an AND logic, so the order doesn't matter.
    // however, when computing the hash, the order WILL matter, so this ensures
    // we maintain the AND logic when constructing the link and that the hashes
    // will match regardless of the order we input the tags.
    util_v_bubble_sort_reversed_u32( state->link.query.tags, cnt_of_array(state->link.query.tags) );

    state->hash = link2_u64_hash( &state->link );

    list_node_t ln = list_ln_create_node2( state, sizeof(link2_state_t), MEM_TYPE_CATBUS_LINK );

    if( ln < 0 ){

        return -1;
    }

    // services_v_join_team( LINK_SERVICE, state->hash, LINK_BASE_PRIORITY - link_u8_count(), LINK_PORT );

    list_v_insert_tail( &link_list, ln );    

    // if( state->mode == LINK_MODE_SEND ){
    //     trace_printf("SEND LINK\n");
    // }
    // else if( state->mode == LINK_MODE_RECV ){
    //     trace_printf("RECV LINK\n");
    // }

    return ln;
}

uint8_t link2_u8_count( void ){

    return list_u8_count( &link_list );
}

static binding_state_t* get_binding_for_key(catbus_hash_t32 key){

    list_node_t ln = binding_list.head;

    while( ln >= 0 ){

        binding_state_t *state = list_vp_get_data( ln );

        if( state->key == key ){

            return state;
        }

        ln = list_ln_next( ln );
    }

    return 0;
}

static void add_or_update_binding( link2_binding_t *link_binding ){

    binding_state_t *state = get_binding_for_key( link_binding->key );

    if( state == 0 ){

        list_node_t ln = list_ln_create_node( 0, sizeof(binding_state_t) );

        if( ln < 0 ){

            return;
        }

        list_v_insert_tail( &binding_list, ln );

        state = list_vp_get_data( ln );
        state->key      = link_binding->key;
        state->rate     = link_binding->rate;
    }

    // apply lowest rate (fastest) to binding
    if( link_binding->rate < state->rate ){

        state->rate = link_binding->rate;    
    }
    state->timeout  = LINK_BINDING_TIMEOUT;
}

PT_THREAD( link2_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK2_PORT );

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
        // if( header->origin_id == catbus_u64_get_origin_id() ){

        //     goto end;
        // }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->msg_type == LINK_MSG_TYPE_BIND ){

            uint8_t count = ( sock_i16_get_bytes_read( sock ) - sizeof(link2_msg_header_t) ) / sizeof(link2_binding_t);

            // iterate through bindings
            link2_binding_t *binding = (link2_binding_t *)( header + 1 );

            while( count > 0 ){

                // check if key is present:
                if( kv_i16_search_hash( binding->key ) >= 0 ){

                    add_or_update_binding( binding );

                    // log_v_debug_P( PSTR("recv binding: 0x%08x 0x%0x"), binding->key, binding->flags );
                    log_v_debug_P( PSTR("recv binding: 0x%08x"), binding->key );
                }

                binding++;
                count--;
            }
        }



end:
    
        THREAD_YIELD( pt );
    }

PT_END( pt );
}


void link2_v_init_header( link2_msg_header_t *header, uint8_t msg_type ){

    header->magic       = LINK_MAGIC;
    header->msg_type    = msg_type;
    header->version     = LINK_VERSION;
    header->flags       = 0;
    header->reserved    = 0;
    // header->origin_id   = catbus_u64_get_origin_id();
    header->universe    = 0;
}

PT_THREAD( link2_client_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 1000 );

        // process binding timeouts
        list_node_t ln = binding_list.head;

        while( ln >= 0 ){

            list_node_t next_ln = list_ln_next( ln );

            binding_state_t *binding_state = list_vp_get_data( ln );

            binding_state->timeout--;

            if( binding_state->timeout == 0 ){

                log_v_debug_P( PSTR("binding timeout") );

                list_v_remove( &binding_list, ln );
                list_v_release_node( ln );
            }

            ln = next_ln;
        }   


        // check if controller is available
        sock_addr_t link_mgr_raddr;

        if( controller_i8_get_addr( &link_mgr_raddr ) == 0 ){

            link_mgr_raddr.port = LINK2_MGR_PORT; // need to change to the correct port!

            // process bindings

            ln = binding_list.head;

            uint16_t current_data_count = 0;
            uint8_t data_buf[UDP_MAX_LEN];
            link2_msg_header_t *data_hdr = (link2_msg_header_t *)data_buf;
            link2_data_t *data_ptr = (link2_data_t *)( data_hdr + 1 );

            link2_v_init_header( data_hdr, LINK_MSG_TYPE_DATA );

            while( ln >= 0 ){

                binding_state_t *binding_state = list_vp_get_data( ln );

                binding_state->ticks -= 1000;

                if( binding_state->ticks <= 0 ){

                    binding_state->ticks = binding_state->rate;

                    // get meta data
                    catbus_meta_t meta;

                    if( kv_i8_get_catbus_meta( binding_state->key, &meta ) < 0 ){

                        log_v_debug_P( PSTR("binding key not found") );

                        goto next_binding;
                    }

                    uint16_t array_len = meta.count + 1;
                    if( array_len > 1 ){

                        log_v_error_P( PSTR("arrays not supported!") );

                        goto next_binding;
                    }

                    data_ptr->key = binding_state->key;

                    if( catbus_i8_get_i64( data_ptr->key, &data_ptr->data ) != 0 ){

                        log_v_error_P( PSTR("catbus got wrecked!") );

                        goto next_binding;
                    }

                    data_ptr++;
                    current_data_count++;

                    if( current_data_count >= LINK_MAX_DATA_ENTRIES ){

                        log_v_debug_P( PSTR("data send %d.%d.%d.%d %d %d"), 
                            link_mgr_raddr.ipaddr.ip3,
                            link_mgr_raddr.ipaddr.ip2,
                            link_mgr_raddr.ipaddr.ip1,
                            link_mgr_raddr.ipaddr.ip0,
                            link_mgr_raddr.port,
                            current_data_count
                        );

                        // transmit message
                        if( sock_i16_sendto( sock, data_buf, sizeof(link2_msg_header_t) + current_data_count * sizeof(link2_data_t), &link_mgr_raddr ) < 0 ){

                            log_v_debug_P( PSTR("data send fail") );
                        }                

                        // reset pointers
                        data_ptr = (link2_data_t *)( data_hdr + 1 );
                        current_data_count = 0;
                    }

                    // uint16_t data_len = type_u16_size( meta.type ) * array_len;

                    // // check if the next item will overflow the buffer
                    // if( ( data_len + sizeof(meta) + current_data_len ) >= CATBUS_MAX_DATA ){

                    //     log_v_debug_P( PSTR("data send") );

                    //     // transmit message
                    //     if( sock_i16_sendto( sock, data_buf, sizeof(link2_msg_header_t) + current_data_len, &link_mgr_raddr ) < 0 ){

                    //         log_v_debug_P( PSTR("data send fail") );
                    //     }

                    //     // flush buffer
                    //     data_ptr = (uint8_t *)( data_hdr + 1 );
                    //     current_data_len = 0;
                    // }

                    // // copy meta data
                    // memcpy( data_ptr, &meta, sizeof(meta) );
                    // data_ptr += sizeof(meta);
                    // current_data_len += sizeof(meta);

                    // if( kv_i8_array_get( binding_state->key, 0, meta.count, data_ptr, data_len ) < 0 ){

                    //     log_v_debug_P( PSTR("kv get fail") );
                    // }

                    // data_ptr += data_len;
                    // current_data_len += data_len;

                    log_v_debug_P( PSTR("process binding: 0x%08x %d"), binding_state->key, current_data_count );
                }

next_binding:
                ln = list_ln_next( ln );
            }   

            // check if there is any data left to transmit in the buffer
            if( current_data_count > 0 ){

                log_v_debug_P( PSTR("data send %d.%d.%d.%d %d %d"), 
                    link_mgr_raddr.ipaddr.ip3,
                    link_mgr_raddr.ipaddr.ip2,
                    link_mgr_raddr.ipaddr.ip1,
                    link_mgr_raddr.ipaddr.ip0,
                    link_mgr_raddr.port,
                    current_data_count
                );

                // transmit message
                if( sock_i16_sendto( sock, data_buf, sizeof(link2_msg_header_t) + current_data_count * sizeof(link2_data_t), &link_mgr_raddr ) < 0 ){

                    log_v_debug_P( PSTR("data send fail") );
                }                
            }



            // send link meta data to link manager
         	link_mgr_raddr.port = LINK2_MGR_PORT;

         	uint8_t link_count = link2_u8_count();

         	// no links, nothing to do!
         	if( link_count == 0 ){

         		continue;
         	}

         	uint8_t max_links_per_message = ( UDP_MAX_LEN - sizeof(link2_msg_header_t) ) / sizeof(link2_t);
         	
         	mem_handle_t h = -1;
         	link2_msg_header_t *hdr = 0;
         	link2_t *link_ptr = 0;
         	uint8_t current_links_this_msg = 0;
         	
     		ln = link_list.head;

    	    while( ln >= 0 ){

    	    	// set up message data
    	    	if( h < 0 ){

    	    		current_links_this_msg = link_count;

    		     	if( current_links_this_msg > max_links_per_message ){

    		     		current_links_this_msg = max_links_per_message;
    		     	}

    	    		// allocate new message data

    	    		h = mem2_h_alloc( sizeof(link2_msg_header_t) + current_links_this_msg * sizeof(link2_t) );

    		     	if( h < 0 ){

    		     		THREAD_RESTART( pt );
    		     	}

    		     	hdr = (link2_msg_header_t *)mem2_vp_get_ptr_fast( h );
    		     	
    		     	link2_v_init_header( hdr, LINK_MSG_TYPE_LINK );

    		     	link_ptr = (link2_t *)( hdr + 1 );

    		     	// log_v_debug_P( PSTR("Link msg: %d"), current_links_this_msg );
    	    	}

    	        link2_state_t *link_state = list_vp_get_data( ln );

    	        link_ptr->mode 			= link_state->link.mode;
    	        link_ptr->aggregation 	= link_state->link.aggregation;
    	        link_ptr->rate 			= link_state->link.rate;
    	        link_ptr->source_key 	= link_state->link.source_key;
    	        link_ptr->dest_key 		= link_state->link.dest_key;
    	        link_ptr->tag 			= link_state->link.tag;
    	        link_ptr->query 		= link_state->link.query;


    	        current_links_this_msg--;
    	        link_ptr++;
    	        link_count--;

    	        if( current_links_this_msg == 0 ){

    	        	// send this message:
    	        	if( sock_i16_sendto_m( sock, h, &link_mgr_raddr ) < 0 ){


    	        	}

    	        	h = -1; // clear handle
    	        }

    	        ln = list_ln_next( ln );     
    	    } 	
        } // controller available
    }

PT_END( pt );
}

#ifndef ENABLE_CATBUS_LINK

bool link_b_is_linked( catbus_hash_t32 key ){

    return FALSE;
}

bool link_b_is_synced( catbus_hash_t32 key ){

    return FALSE;
}

bool link_b_is_synced_leader( catbus_hash_t32 key ){

    return FALSE;
}

bool link_b_is_synced_follower( catbus_hash_t32 key ){

    return FALSE;
}

#endif

#endif