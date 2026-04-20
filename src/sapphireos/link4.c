
#include "system.h"
#include "timers.h"
#include "sockets.h"
#include "threading.h"
#include "fs.h"
#include "keyvalue.h"
#include "catbus.h"
#include "list.h"
#include "hash.h"
#include "util.h"
#include "config.h"
#include "random.h"
#include "device_db.h"

#include "link4.h"

// #define NO_LOGGING
#include "logging.h"



static socket_t sock;
static list_t link_list;


static int32_t link4_test_key;
static int32_t link4_test_key2;

static uint32_t link4_msgs_sent;
static uint32_t link4_msgs_recv;

static int8_t _kv_i8_link_client_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        if( hash == __KV__link4_count ){

        	uint8_t *ptr = (uint8_t *)data;
        	*ptr = list_u8_count( &link_list );
        }
        
        return 0;
    }

    return -1;
}


KV_SECTION_META kv_meta_t link4_kv[] = {
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  0, _kv_i8_link_client_handler,   "link4_count" },

    { CATBUS_TYPE_INT32,   0, 0,                   &link4_test_key,             0,  "link4_test_key" },
    { CATBUS_TYPE_INT32,   0, 0,                   &link4_test_key2,            0,  "link4_test_key2" },

    { CATBUS_TYPE_UINT32,  0, 0,                   &link4_msgs_sent,            0,  "link4_msgs_sent" },
    { CATBUS_TYPE_UINT32,  0, 0,                   &link4_msgs_recv,            0,  "link4_msgs_recv" },
};


PT_THREAD( link_server_thread( pt_t *pt, void *state ) );
PT_THREAD( link_processor_thread( pt_t *pt, void *state ) );


void link4_v_init( void ){

	list_v_init( &link_list );

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

   // 	if( ( cfg_u64_get_device_id() == 154851823073836 ) ||
   // 		( cfg_u64_get_device_id() == 109982513431848 ) ){

// 	   	catbus_query_t query = {0};
// 	   	query.tags[0] = __KV__link4_rx;

// 	   	link4_l_create(
// 	   		LINK4_MODE_SEND,
// 	   		__KV__link4_test_key,
// 	   		__KV__link4_test_key2,
// 	   		&query,
// 	   		__KV__link4_tag,
// 	   		100,
// 	   		LINK4_AGG_LAST
// 	   	);
   // }
}

bool link4_b_compare( const link4_t *link1, const link4_t *link2 ){
 
    // return memcmp( link1, link2, sizeof(link4_t) - sizeof(mem_handle_t) ) == 0;

    if( link1->mode != link2->mode ){

        log_v_debug_P( PSTR("mode") );

        return FALSE;
    }

    if( link1->aggregation != link2->aggregation ){

        log_v_debug_P( PSTR("aggregation") );

        return FALSE;
    }

    if( link1->rate != link2->rate ){

        log_v_debug_P( PSTR("rate") );

        return FALSE;
    }

    if( link1->source_key != link2->source_key ){

        log_v_debug_P( PSTR("source_key") );

        return FALSE;
    }

    if( link1->dest_key != link2->dest_key ){

        log_v_debug_P( PSTR("dest_key") );

        return FALSE;
    }

    if( link1->tag != link2->tag ){

        log_v_debug_P( PSTR("tag") );

        return FALSE;
    }

    // copy queries, sort, and compare
    catbus_query_t query1 = link1->query;
    catbus_query_t query2 = link2->query;

    util_v_bubble_sort_reversed_u32( query1.tags, cnt_of_array(query1.tags) );   
    util_v_bubble_sort_reversed_u32( query2.tags, cnt_of_array(query2.tags) );

    for( uint8_t i = 0; i < cnt_of_array(query1.tags); i++ ){

        if( query1.tags[i] != query2.tags[i] ){

            log_v_debug_P( PSTR("query") );

            return FALSE;
        }
    }

    return TRUE;
}

link4_handle_t link4_l_lookup( link4_t *link ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        link4_state_t *state = list_vp_get_data( ln );

        if( link4_b_compare( link, &state->link ) ){

            return ln;
        }

        ln = list_ln_next( ln );
    }

    return -1;
}


link4_handle_t link4_l_create2( link4_t *link ){

	link4_state_t state = {0};
    state.link = *link;

    link4_handle_t lh = link4_l_lookup( &state.link );

    if( lh > 0 ){

    	return lh;
    }	

	if( list_u8_count( &link_list ) >= LINK4_MAX_LINKS ){

        log_v_error_P( PSTR("Too many links") );

        return -1;
    }

    if( state.link.rate < LINK4_RATE_MIN ){

        state.link.rate = LINK4_RATE_MIN;
    }
    else if( state.link.rate > LINK4_RATE_MAX ){

        state.link.rate = LINK4_RATE_MAX;
    }

    // sort tags from highest to lowest so that all valid combinations
    // of the query will compare properly
    util_v_bubble_sort_reversed_u32( state.link.query.tags, cnt_of_array(state.link.query.tags) );

	list_node_t ln = list_ln_create_node2( &state, sizeof(link4_state_t), MEM_TYPE_LINK4 );

    if( ln < 0 ){

        return -1;
    }

    list_v_insert_tail( &link_list, ln );    

    return ln;
}


link4_handle_t link4_l_create( 
    link4_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link4_rate_t16 rate,
    link4_aggregation_t8 aggregation ){ 

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return -1;
    }

    link4_t link = {
    	.mode               = mode,
        .source_key         = source_key,
        .dest_key           = dest_key,
        .query              = *query,
        .tag                = tag,
        .rate               = rate,
        .aggregation        = aggregation,
    };

    return link4_l_create2( &link );
}

static void delete_link( link4_handle_t link ){

    const link4_state_t *state = list_vp_get_data( link );

    if( state->database_h > 0 ){

    	mem2_v_free( state->database_h );
    }

    list_v_remove( &link_list, link );
    list_v_release_node( link );
}

void link4_v_delete_by_tag( catbus_hash_t32 tag ){

    list_node_t ln = link_list.head;

    while( ln >= 0 ){

        list_node_t next_ln = list_ln_next( ln );

        link4_state_t *link_state = list_vp_get_data( ln );
        const link4_t *link = &link_state->link;

        // check if link is local
        // remote links don't get deleted here, they must time out
        if( ( link->mode != LINK4_MODE_SEND ) &&
            ( link->mode != LINK4_MODE_RECV ) ){

            goto next;
        }

        if( link->tag == tag ){

            delete_link( ln );
        }

next:
        ln = next_ln;
    }   
}


static uint8_t database_count( mem_handle_t database_h ){

	return mem2_u16_get_size( database_h ) / sizeof(link4_data_t);
}

static int32_t aggregate( mem_handle_t database_h, link4_aggregation_t8 agg ){

    link4_data_t *database = (link4_data_t *)mem2_vp_get_ptr( database_h );

    uint8_t count = database_count( database_h );

    if( count == 0 ){

        return 0;
    }

    int32_t value = 0;

    if( agg == LINK4_AGG_MIN ){

        value = database[0].value;

        for( uint8_t i = 1; i < count; i++ ){

            if( database[i].value < value ){

                value = database[i].value;
            }
        }    
    }
    else if( agg == LINK4_AGG_MAX ){

        value = database[0].value;

        for( uint8_t i = 1; i < count; i++ ){

            if( database[i].value > value ){

                value = database[i].value;
            }
        }    
    }
    else if( agg == LINK4_AGG_SUM ){

        for( uint8_t i = 0; i < count; i++ ){

            value += database[i].value;
        }    
    }
    else if( agg == LINK4_AGG_AVG ){

        for( uint8_t i = 0; i < count; i++ ){

            value += database[i].value;
        }

        value /= count;
    }

	return value;
}

PT_THREAD( link_processor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, LINK4_PROCESS_RATE );

		list_node_t ln = link_list.head;

        while( ln >= 0 ){

            list_node_t next_ln = list_ln_next( ln );

            link4_state_t *link_state = list_vp_get_data( ln );
            link4_t *link = &link_state->link;

            if( ( link->mode == LINK4_MODE_SEND ) ||
                ( link->mode == LINK4_MODE_REMOTE_SEND ) ){

                // link4_test_key++;

            	// lookup local data
            	catbus_meta_t meta;
            	if( kv_i8_get_catbus_meta( link->source_key, &meta ) < 0 ){
        		
        			// not found!
                    log_v_error_P( PSTR("meta not found!") );

		            goto next;
		        }

		        int64_t data = 0;

	            if( catbus_i8_get_i64( link->source_key, &data ) != 0 ){

	                log_v_error_P( PSTR("data not found!") );

	                goto next;
	            }

	            // check if data is installed in link:
	            if( link_state->database_h <= 0 ){

	            	// create database
	            	link_state->database_h = mem2_h_alloc2( sizeof(link4_data_t), MEM_TYPE_LINK4_DB );

	            	if( link_state->database_h < 0 ){
	            		
	            		log_v_error_P( PSTR("alloc fail") );

	                	goto next;
	            	}

	            	memset( mem2_vp_get_ptr( link_state->database_h ), 0, sizeof(link4_data_t) );

	            	link_state->transmit_timer = 0;
	            }

	            // deref database
	         	link4_data_t *database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

	         	// detect changes:
            	bool changed = data != database->value;

            	// update database
            	database->value = data;

            	if( changed ){

            		// force timer so we transmit now
            		link_state->transmit_timeout = 1;
					link_state->transmit_timer = 0;
            	}

            	if( link_state->transmit_timer > 0 ){

            		link_state->transmit_timer--;
            	}
            	else{

                    if( link_state->transmit_timeout == 0 ){

                        link_state->transmit_timeout = 1;
                    }

                    // log_v_debug_P( PSTR("%d %d"), link_state->transmit_timeout, link_state->transmit_timer );
            		
            		// tx timer expired!
            		link_state->transmit_timer = link_state->transmit_timeout; // reset timer
					
					// bump transmit_timeout up towards max
					if( link_state->transmit_timeout < LINK4_RETRANSMIT_MAX){

						link_state->transmit_timeout *= 2;
					}
					else if( link_state->transmit_timeout > LINK4_RETRANSMIT_MAX ){

						link_state->transmit_timeout = LINK4_RETRANSMIT_MAX;
					}

                    uint8_t msg_type = LINK4_MSG_TYPE_SEND;

                    if( link->mode == LINK4_MODE_REMOTE_SEND ){

                        msg_type = LINK4_MSG_TYPE_REMOTE_SEND;
                    }

            		// create message
					link4_msg_send_t msg = {
						.header.magic 		= LINK4_MAGIC,
						.header.msg_type 	= msg_type,
						.header.version     = LINK4_VERSION,
						.link 				= *link,
						.value 				= database->value,
					};

            		// transmit to target nodes:
					device_db_v_reset_iter();
					device_db_v_set_query( &link->query );

					const device_data_t *device = device_db_p_get_next_query( 0 );

                    // if( device == 0 ){

                    //     log_v_error_P( PSTR("device not found!") );
                    // }

					while( device != 0 ){

						sock_addr_t raddr = {
							.ipaddr = device->ip,
							.port = LINK4_PORT,
						};

						if( sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr ) < 0 ){

                            log_v_error_P( PSTR("socket send failed") );

                            break;
                        }

                        link4_msgs_sent++;

            			device = device_db_p_get_next_query( 0 );
					}
				}
            }
            else if( link->mode == LINK4_MODE_RECV ){

                if( link_state->transmit_timer > 0 ){

                    link_state->transmit_timer--;
                }

                if( link_state->transmit_timer == 0 ){

                    link_state->transmit_timer = LINK4_RECV_TX_RATE;

                    // create message
                    link4_msg_recv_t msg = {
                        .header.magic       = LINK4_MAGIC,
                        .header.msg_type    = LINK4_MSG_TYPE_RECV,
                        .header.version     = LINK4_VERSION,
                        .link               = *link,
                    };

                    // transmit to target nodes:
                    device_db_v_reset_iter();
                    device_db_v_set_query( &link->query );

                    const device_data_t *device = device_db_p_get_next_query( 0 );

                    // if( device == 0 ){

                    //     log_v_error_P( PSTR("device not found!") );
                    // }

                    while( device != 0 ){

                        sock_addr_t raddr = {
                            .ipaddr = device->ip,
                            .port = LINK4_PORT,
                        };

                        if( sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr ) < 0 ){

                            log_v_error_P( PSTR("socket send failed") );

                            break;
                        }

                        link4_msgs_sent++;

                        device = device_db_p_get_next_query( 0 );
                    }
                }
            }
            else if( link->mode == LINK4_MODE_REMOTE_RECV ){

            	if( link_state->remote_timeout > 0 ){

            		link_state->remote_timeout--;
            	}

            	if( link_state->remote_timeout == 0 ){

            		log_v_info_P( PSTR("Remote receive link timed out") );

            		delete_link( ln );

                    goto next;
            	}
            	// not timed out, check database
            	else if( link_state->database_h <= 0 ){

                    // no database present, we are done processing
                    goto next;
                }

                goto next;

        		link4_data_t *database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

        		for( int i = 0; i < database_count( link_state->database_h ); i++ ){

        			if( database->timeout > 0 ){

        				database->timeout--;
        			}

        			if( database->timeout > 0 ){

                        continue;
                    }

    				log_v_info_P( PSTR("Data timed out: %d.%d.%d.%d hash: 0x%08x"),
                        database->ip.ip3,
                        database->ip.ip2,
                        database->ip.ip1,
                        database->ip.ip0,
                        link_state->link.dest_key
                    );

    				uint16_t old_database_size = mem2_u16_get_size( link_state->database_h );
    				uint16_t new_database_size = old_database_size - sizeof(link4_data_t);

    				if( new_database_size == 0 ){

    					// easy path, just release db
    					mem2_v_free( link_state->database_h );
    					link_state->database_h = -1;
    				}
    				else{

        				mem_handle_t new_database_h = mem2_h_alloc2( new_database_size, MEM_TYPE_LINK4_DB );

        				if( new_database_h <= 0 ){

		        			log_v_error_P( PSTR("alloc fail") );

		                	goto next;
		        		}

						link4_data_t *new_database = (link4_data_t *)mem2_vp_get_ptr( new_database_h );

						// copy old data items into new db, skipping this current item we are deleting
		        		for( int j = 0; j < database_count( link_state->database_h ); j++ ){

		        			if( j == i ){

		        				continue;
		        			}

		        			*new_database = database[j];
		        			new_database++;
		        		}

		        		// release old db, set new on
		        		mem2_v_free( link_state->database_h );
		        		link_state->database_h = new_database_h;
		        	}
    			}
            }
            
next:
            ln = next_ln;
        }   

	}

PT_END( pt );
}




PT_THREAD( link_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    ASSERT( sock >= 0 );

    sock_v_bind( sock, LINK4_PORT );


    // sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if shutting down
        if( sys_b_is_shutting_down() ){

            sock_v_release( sock );

            THREAD_EXIT( pt );
        }

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        link4_msg_header_t *header = sock_vp_get_data( sock );

        // verify message
        if( header->magic != LINK4_MAGIC ){

            continue;
        }

        if( header->version != LINK4_VERSION ){

            continue;
        }

        // filter our own messages
        // if( header->origin_id == catbus_u64_get_origin_id() ){

        //     continue;
        // }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( ( header->msg_type == LINK4_MSG_TYPE_SEND ) ||
            ( header->msg_type == LINK4_MSG_TYPE_REMOTE_SEND ) ){

        	link4_msg_send_t *msg = (link4_msg_send_t *)header;

            // check if our query matches:
            if( !catbus_b_query_self( &msg->link.query ) ){

                log_v_debug_P( PSTR("Received send link from: %d.%d.%d.%d with unmatched query!"),
                              raddr.ipaddr.ip3,
                              raddr.ipaddr.ip2,
                              raddr.ipaddr.ip1,
                              raddr.ipaddr.ip0 );
                
                continue;
            }

            link4_msgs_recv++;

        	// check for matching remote receive link
            if( header->msg_type == LINK4_MSG_TYPE_SEND ){

            	msg->link.mode = LINK4_MODE_REMOTE_RECV;
            }
            else if( header->msg_type == LINK4_MSG_TYPE_REMOTE_SEND ){

                msg->link.mode = LINK4_MODE_RECV;
            }
            else{

                log_v_error_P( PSTR("Invalid link pairing") );

                continue;
            }

        	// check for corresponding link
        	link4_handle_t lh = link4_l_lookup( &msg->link );

        	if( lh <= 0 ){

        		// need to create remote receive link
        		lh = link4_l_create2( &msg->link );

        		if( lh <= 0 ){

        			log_v_error_P( PSTR("alloc failed") );

        			continue;
        		}

        		log_v_info_P( PSTR("Created remote receive link from: %d.%d.%d.%d"),
                              raddr.ipaddr.ip3,
                              raddr.ipaddr.ip2,
                              raddr.ipaddr.ip1,
                              raddr.ipaddr.ip0 );
        	}

        	ASSERT( lh > 0 );

        	link4_state_t *link_state = (link4_state_t *)list_vp_get_data( lh );

        	// update timeout
        	link_state->remote_timeout = LINK4_LINK_TIMEOUT;

        	link4_data_t *database = 0;

        	// check for database
        	if( link_state->database_h <= 0 ){

        		// create database
	            link_state->database_h = mem2_h_alloc2( sizeof(link4_data_t), MEM_TYPE_LINK4_DB );

	            if( link_state->database_h < 0 ){
	            		
            		log_v_error_P( PSTR("alloc fail") );

                	continue;
            	}

            	database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

            	// init first item
            	database->value = 0x7fffffff;
            	database->ip    = raddr.ipaddr;

                log_v_info_P( PSTR("Create database: %d.%d.%d.%d hash: 0x%08x"),
                    database->ip.ip3,
                    database->ip.ip2,
                    database->ip.ip1,
                    database->ip.ip0,
                    link_state->link.dest_key
                );
        	}

        	// search for matching node in database
        	database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

        	bool match = FALSE;
        	for( int i = 0; i < database_count( link_state->database_h ); i++ ){

        		if( ip_b_addr_compare( database->ip, raddr.ipaddr ) ){

        			match = TRUE;

        			break;
        		}

        		database++;
        	}

        	if( !match ){

        		uint16_t old_database_size = mem2_u16_get_size( link_state->database_h );

        		// no match, create item
        		mem_handle_t new_database_h = mem2_h_alloc2( sizeof(link4_data_t) + old_database_size, MEM_TYPE_LINK4_DB );

        		if( new_database_h <= 0 ){

        			log_v_error_P( PSTR("alloc fail") );

                	continue;
        		}

        		// copy old data
        		memcpy( 
        			mem2_vp_get_ptr( new_database_h ), 
        			mem2_vp_get_ptr( link_state->database_h ), 
        			old_database_size 
        		);

        		// free old handle
        		mem2_v_free( link_state->database_h );

        		// assign handle
        		link_state->database_h = new_database_h;

        		// get new pointer
        		database = (link4_data_t *)mem2_vp_get_ptr( new_database_h ) + old_database_size;

                log_v_info_P( PSTR("Add database: %d.%d.%d.%d hash: 0x%08x"),
                    raddr.ipaddr.ip3,
                    raddr.ipaddr.ip2,
                    raddr.ipaddr.ip1,
                    raddr.ipaddr.ip0,
                    link_state->link.dest_key
                );
        	}

        	// now we have a pointer to this data item
        	// make sure IP is tracked
        	database->ip 		= raddr.ipaddr;
        	database->timeout 	= LINK4_DATA_TIMEOUT;

            log_v_info_P( PSTR("Update database: %d.%d.%d.%d hash: 0x%08x"),
                database->ip.ip3,
                database->ip.ip2,
                database->ip.ip1,
                database->ip.ip0,
                link_state->link.dest_key
            );

        	// detect changes:
            bool changed = msg->value != database->value;

         	if( changed ){

         		// assign value
         		database->value = msg->value;

         		int32_t computed_value;
         		if( link_state->link.aggregation == LINK4_AGG_LAST ){

         			computed_value = msg->value;
         		}
         		else{

         			computed_value = aggregate( link_state->database_h, link_state->link.aggregation );	
         		}

         		// set value in DB
         		if( catbus_i8_set_i64( link_state->link.dest_key, (int64_t)computed_value ) < 0 ){

         			// error path
         		}
         	}
        }
        else if( header->msg_type == LINK4_MSG_TYPE_RECV ){

            link4_msgs_recv++;

            link4_msg_recv_t *msg = (link4_msg_recv_t *)header;

            // check for matching remote send link
            msg->link.mode = LINK4_MODE_REMOTE_SEND;

            // check for corresponding link
            link4_handle_t lh = link4_l_lookup( &msg->link );

            if( lh <= 0 ){

                // need to create remote receive link
                lh = link4_l_create2( &msg->link );

                if( lh <= 0 ){

                    log_v_error_P( PSTR("alloc failed") );

                    continue;
                }

                log_v_info_P( PSTR("Created remote send link") );
            }

            ASSERT( lh > 0 );

            link4_state_t *link_state = (link4_state_t *)list_vp_get_data( lh );

            // update timeout
            link_state->remote_timeout = LINK4_LINK_TIMEOUT;
        }
    }

PT_END( pt );
}

