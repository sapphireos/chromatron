
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

   	if( ( cfg_u64_get_device_id() == 154851823073836 ) ||
   		( cfg_u64_get_device_id() == 109982513431848 ) ){

	   	catbus_query_t query = {0};
	   	query.tags[0] = __KV__link4_rx;

	   	link4_l_create(
	   		LINK4_MODE_SEND,
	   		__KV__link4_test_key,
	   		__KV__link4_test_key2,
	   		&query,
	   		__KV__link4_tag,
	   		100,
	   		LINK4_AGG_LAST
	   	);
   }
}

bool link4_b_compare( const link4_t *link1, const link4_t *link2 ){
 
    return memcmp( link1, link2, sizeof(link4_t) - sizeof(mem_handle_t) ) == 0;
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

	link4_state_t state = {
        .link = *link,
    };

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

    link4_state_t *state = list_vp_get_data( link );

    if( state->database_h > 0 ){

    	mem2_v_free( state->database_h );
    }

    list_v_remove( &link_list, link );
    list_v_release_node( link );
}



static uint8_t database_count( mem_handle_t database_h ){

	return mem2_u16_get_size( database_h ) / sizeof(link4_data_t);
}

static int32_t aggregate( mem_handle_t database_h, link4_aggregation_t8 agg ){

	return 0;
}

PT_THREAD( link_processor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, LINK4_RATE_MIN );

		list_node_t ln = link_list.head;

        while( ln >= 0 ){

            list_node_t next_ln = list_ln_next( ln );

            link4_state_t *link_state = list_vp_get_data( ln );
            link4_t *link = &link_state->link;

            if( link->mode == LINK4_MODE_SEND ){

            	// lookup local data
            	catbus_meta_t meta;
            	if( kv_i8_get_catbus_meta( link->source_key, &meta ) < 0 ){
        		
        			// not found!

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
	            	link_state->database_h = mem2_h_alloc( sizeof(link4_data_t) );

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
            		link_state->timeout 	   = 1;
					link_state->transmit_timer = 0;
            	}

            	if( link_state->transmit_timer > 0 ){

            		link_state->transmit_timer--;
            	}
            	else{
            		
            		// tx timer expired!
            		link_state->transmit_timer = link_state->timeout; // reset timer
					
					// bump timeout up towards max
					if( link_state->timeout < 128){

						link_state->timeout *= 2;
					}
					else if( link_state->timeout > 128 ){

						link_state->timeout = 128;
					}

            		// create message
					link4_msg_send_t msg = {
						.header.magic 		= LINK4_MAGIC,
						.header.msg_type 	= LINK4_MSG_TYPE_SEND,
						.header.version     = LINK4_VERSION,
						.link 				= *link,
						.value 				= database->value,
					};

            		// transmit to target nodes:
					device_db_v_reset_iter();
					device_db_v_set_query( &link->query );

					const device_data_t *device = device_db_p_get_next();

					while( device != 0 ){

						sock_addr_t raddr = {
							.ipaddr = device->ip,
							.port = LINK4_PORT,
						};

						sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

            			device = device_db_p_get_next();
					}
				}
            }
            else if( link->mode == LINK4_MODE_RECV ){

                if( link_state->transmit_timer > 0 ){

                    link_state->transmit_timer--;
                }
                else{

                    link_state->transmit_timer = 100;

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

                    const device_data_t *device = device_db_p_get_next();

                    while( device != 0 ){

                        sock_addr_t raddr = {
                            .ipaddr = device->ip,
                            .port = LINK4_PORT,
                        };

                        sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );

                        device = device_db_p_get_next();
                    }
                }
            }
            else if( link->mode == LINK4_MODE_REMOTE_RECV ){

            	if( link_state->timeout > 0 ){

            		link_state->timeout--;
            	}

            	if( link_state->timeout == 0 ){

            		log_v_info_P( PSTR("Remote receive link timed out") );

            		delete_link( ln );
            	}
            	// not timed out, check database
            	else if( link_state->database_h > 0 ){

            		link4_data_t *database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

            		for( int i = 0; i < database_count( link_state->database_h ); i++ ){

            			if( database->timeout > 0 ){

            				database->timeout--;
            			}

            			if( database->timeout == 0 ){

            				log_v_info_P( PSTR("Data timed out") );

            				uint16_t old_database_size = mem2_u16_get_size( link_state->database_h );
            				uint16_t new_database_size = old_database_size - sizeof(link4_data_t);

            				if( new_database_size == 0 ){

            					// easy path, just release db
            					mem2_v_free( link_state->database_h );
            					link_state->database_h = -1;
            				}
            				else{

	            				mem_handle_t new_database_h = mem2_h_alloc( new_database_size );

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
            	}
                else if( link->mode == LINK4_MODE_REMOTE_SEND ){

                    if( link_state->timeout > 0 ){

                        link_state->timeout--;
                    }

                    if( link_state->timeout == 0 ){

                        log_v_info_P( PSTR("Remote send link timed out") );

                        delete_link( ln );
                    }
                }
            }

next:
            ln = next_ln;
        }   

	}

    // load_links_from_file();

    // if(stats_handle > 0){

    //     mem2_v_free( stats_handle );
    // }

    // THREAD_WAIT_WHILE( pt, ( link_u8_count() == 0 ) &&
    //                        ( producer_count() == 0 ) );

    // if( stats_handle <= 0 ){

    //     // allocate memory for stats
    //     stats_handle = mem2_h_alloc( sizeof(link_stats_t) * LINK_MAX_STATS );

    //     if( stats_handle <= 0 ){

    //         // uh, bummer?
    //     }
    // }
    
    // // init alarm
    // thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    // while(1){

    //     if( link_process_tick_rate < LINK_MIN_TICK_RATE ){

    //         link_process_tick_rate = LINK_MIN_TICK_RATE;
    //     }
    //     else if( link_process_tick_rate > LINK_MAX_TICK_RATE ){

    //         link_process_tick_rate = LINK_MAX_TICK_RATE;
    //     }

    //     uint32_t prev_alarm = thread_u32_get_alarm();

    //     thread_v_set_alarm( prev_alarm + link_process_tick_rate );
    //     THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );

    //     // check if shutting down
    //     if( sys_b_is_shutting_down() ){

    //         transmit_shutdown();
    //         TMR_WAIT( pt, 100 );
    //         transmit_shutdown();
    //         TMR_WAIT( pt, 100 );
    //         transmit_shutdown();

    //         THREAD_EXIT( pt );
    //     }

    //     uint32_t elapsed_time = link_process_tick_rate;

    //     // reset process tick rate.
    //     // existing links and producers will update to the max rate
    //     // needed.  this is here to reduce the rate if a link or producer
    //     // is removed.
    //     link_process_tick_rate = LINK_MIN_TICK_RATE;

    //     // update timeouts
    //     process_consumer_timeouts( elapsed_time );
    //     process_producer_timeouts( elapsed_time );
    //     process_remote_timeouts( elapsed_time );
    //     process_stats_timeouts( elapsed_time );
        
    //     list_node_t ln;

    //     // process producers
    //     ln = producer_list.head;

    //     while( ln >= 0 ){

    //         producer_state_t *producer = list_vp_get_data( ln );
            
    //         process_producer( producer, elapsed_time );
            
    //         ln = list_ln_next( ln );
    //     }

    //     // process links
    //     ln = link_list.head;

    //     while( ln >= 0 ){

    //         process_link( ln, elapsed_time );

    //         ln = list_ln_next( ln );
    //     }

    // }

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

        if( header->msg_type == LINK4_MSG_TYPE_SEND ){

        	link4_msg_send_t *msg = (link4_msg_send_t *)header;

        	// check for matching remote receive link
        	msg->link.mode = LINK4_MODE_REMOTE_RECV;

        	// check for corresponding link
        	link4_handle_t lh = link4_l_lookup( &msg->link );

        	if( lh <= 0 ){

        		// need to create remote receive link
        		lh = link4_l_create2( &msg->link );

        		if( lh <= 0 ){

        			log_v_error_P( PSTR("alloc failed") );

        			continue;
        		}

        		log_v_info_P( PSTR("Created remote receive link") );
        	}

        	ASSERT( lh > 0 );

        	link4_state_t *link_state = (link4_state_t *)list_vp_get_data( lh );

        	// update timeout
        	link_state->timeout = LINK4_LINK_TIMEOUT;

        	link4_data_t *database = 0;

        	// check for database
        	if( link_state->database_h <= 0 ){

        		// create database
	            link_state->database_h = mem2_h_alloc( sizeof(link4_data_t) );

	            if( link_state->database_h < 0 ){
	            		
            		log_v_error_P( PSTR("alloc fail") );

                	continue;
            	}

            	database = (link4_data_t *)mem2_vp_get_ptr( link_state->database_h );

            	// init first item
            	database->value = 0x7fffffff;
            	database->ip    = raddr.ipaddr;
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
        		mem_handle_t new_database_h = mem2_h_alloc( sizeof(link4_data_t) + old_database_size );

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
        	}

        	// now we have a pointer to this data item
        	// make sure IP is tracked
        	database->ip 		= raddr.ipaddr;
        	database->timeout 	= LINK4_DATA_TIMEOUT;

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
            link_state->timeout = LINK4_LINK_TIMEOUT;

            
        }
    }

//         if( header->msg_type == LINK_MSG_TYPE_CONSUMER_QUERY ){

//             // trace_printf("LINK: RX consumer query\n");

//             link_msg_consumer_query_t *msg = (link_msg_consumer_query_t *)header;

//             if( msg->mode == LINK_MODE_SEND ){

//                 // check if we have this link, if so, we are part of the send group,
//                 // not the consumer group, even if we would otherwise match the query.
//                 // this is a bit of a corner case, but it handles the scenario where
//                 // a send producer also matches as a consumer and is receiving data
//                 // it is trying to send.
//                 if( link_l_lookup_by_hash( msg->hash ) > 0 ){

//                     goto end;
//                 }

//                 // check query
//                 if( !catbus_b_query_self( &msg->query ) ){

//                     goto end;
//                 }
//             }
//             else if( msg->mode == LINK_MODE_RECV ){

//                 // consumers on a receive link should
//                 // have the link itself, so we should
//                 // not be receiving this message at all (receive leaders shouldn't be sending it).
//                 // the sender is probably confused.
//                 log_v_error_P( PSTR("receive links should not be sending consumer query") );
                
//                 goto end;
//             }

//             #ifdef TEST_MODE
//             if( test_link_mode == 0 ){

//                 if( ( msg->key == __KV__link_test_key ) ||
//                     ( msg->key == __KV__link_test_key2 ) ){

//                     goto end;
//                 }

//             }
//             #endif

//             // check key
//             if( kv_i16_search_hash( msg->key ) < 0 ){

//                 goto end;
//             }

//             // we are a consumer for this link
            
//             // transmit response
//             transmit_consumer_match( msg->hash, &raddr );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_QUERY ){

//             // trace_printf("LINK: RX producer query\n");

//             link_msg_producer_query_t *msg = (link_msg_producer_query_t *)header;

//             // check query
//             if( !catbus_b_query_self( &msg->query ) ){

//                 goto end;
//             }

//             // check key
//             if( kv_i16_search_hash( msg->key ) < 0 ){

//                 goto end;
//             }

//              #ifdef TEST_MODE
//             if( test_link_mode == 0 ){

//                 if( ( msg->key == __KV__link_test_key ) ||
//                     ( msg->key == __KV__link_test_key2 ) ){

//                     goto end;
//                 }

//             }
//             #endif

//             // we are a producer for this link

//             update_producer_from_query( msg, &raddr );

//             // log_v_debug_P("LINK: %s() producer match\n", __FUNCTION__);
//             // trace_printf("LINK: %s() producer match\n", __FUNCTION__);
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_MATCH ){

//             // trace_printf("LINK: RX consumer match\n");

//             link_msg_consumer_match_t *msg = (link_msg_consumer_match_t *)header;

//             // received a match
//             update_consumer( msg->hash, &raddr );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_CONSUMER_DATA ){

//             // trace_printf("LINK: RX consumer DATA\n");

//             link_msg_data_t *msg = (link_msg_data_t *)header;

//             catbus_meta_t meta;

//             if( kv_i8_get_catbus_meta( msg->hash, &meta ) < 0 ){

//                 log_v_error_P( PSTR("rx hash 0x%08x not found!"), msg->hash );

//                 goto end;
//             }

//             // if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

//             //     log_v_error_P( PSTR("rx meta does not match!") );

//             //     goto end;
//             // }

//             // verify data lengths
//             uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
//             uint16_t array_len = meta.count + 1;
//             // uint16_t type_len = type_u16_size( meta.type );
//             // uint16_t data_len = array_len * type_len;

//             // if( data_len != msg_data_len ){

//             //     log_v_error_P( PSTR("rx len does not match!") );

//             //     goto end;
//             // }

//             if( catbus_i8_array_set( msg->hash, msg->data.meta.type, 0, array_len, &msg->data.data, msg_data_len ) < 0 ){

//                 log_v_error_P( PSTR("data fail: 0c%08x"), msg->hash );

//                 goto end;
//             }

//             update_stats_received_key( msg->hash );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_PRODUCER_DATA ){

//             // trace_printf("LINK: RX producer DATA\n");

//             link_msg_data_t *msg = (link_msg_data_t *)header;

//             // get link
//             link_handle_t link = link_l_lookup_by_hash( msg->hash );

//             if( link < 0 ){

//                 log_v_error_P( PSTR("link not found!") );

//                 goto end;
//             }

//             // are we leader?
//             if( !is_link_leader( link ) ){

//                 log_v_error_P( PSTR("not a leader!") );

//                 goto end;
//             }

//             link_state_t *link_state = link_ls_get_data( link );

//             // get meta data from database
//             catbus_meta_t meta;
//             if( kv_i8_get_catbus_meta( link_state->dest_key, &meta ) < 0 ){

//                 log_v_error_P( PSTR("dest key not found!") );

//                 goto end;
//             }

//             // check keys.  the producer should be sending us the 
//             // source key (they don't know the destination key)
//             if( msg->data.meta.hash != link_state->source_key ){

//                 log_v_error_P( PSTR("producer sent wrong source key!") );

//                 goto end;
//             }

//             // now change the key in the msg meta data to the 
//             // dest key, which is what we're using from here on out
//             msg->data.meta.hash = link_state->dest_key;

//             // compare meta data, all producers need to match the leader
//             // if( memcmp( &meta, &msg->data.meta, sizeof(meta) ) != 0 ){

//             //     log_v_error_P( PSTR("meta data mismatch!") );

//             //     goto end;
//             // }

//             // verify data lengths
//             uint16_t msg_data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(link_msg_data_t) - 1 );
//             // uint16_t array_len = meta.count + 1;
//             // uint16_t type_len = type_u16_size( meta.type );
//             // uint16_t data_len = array_len * type_len;

//             // if( data_len != msg_data_len ){

//             //     log_v_error_P( PSTR("rx len does not match!") );

//             //     goto end;
//             // }

//             // update remote data and timeout
//             // update_remote( &raddr, link, &msg->data.data, data_len );
//             update_remote( &raddr, link, &msg->data, msg_data_len );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_ADD ){

//             link_msg_add_t *msg = (link_msg_add_t *)header;

//             link_handle_t link = link_l_create(
//                 msg->mode,
//                 msg->source_key,
//                 msg->dest_key,
//                 &msg->query,
//                 msg->tag,
//                 msg->rate,
//                 msg->aggregation,
//                 msg->filter );

//             link_msg_confirm_t reply;

//             if( link > 0 ){

//                 save_link_to_file( link );
//                 reply.status = 0;
//             }
//             else{

//                 reply.status = -1;
//             }
            
//             init_header( &reply.header, LINK_MSG_TYPE_CONFIRM );

//             sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_DELETE ){

//             link_msg_delete_t *msg = (link_msg_delete_t *)header;

//             if( msg->tag != 0 ){
                
//                 link_v_delete_by_tag( msg->tag );
//             }
//             else{

//                 link_v_delete_by_hash( msg->hash );
//             }

//             link_msg_confirm_t reply;
//             init_header( &reply.header, LINK_MSG_TYPE_CONFIRM );
//             reply.status = 0;

//             sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );
//         }
//         else if( header->msg_type == LINK_MSG_TYPE_SHUTDOWN ){

//             list_node_t ln = producer_list.head;

//             while( ln >= 0 ){

//                 list_node_t next_ln = list_ln_next( ln );

//                 producer_state_t *producer = list_vp_get_data( ln );

//                 if( sock_b_addr_compare( &raddr, &producer->leader_addr ) ){

//                     // remove producer
//                     list_v_remove( &producer_list, ln );
//                     list_v_release_node( ln );

//                     trace_printf("LINK: producer leader shutdown\n");
//                 }

//                 ln = next_ln;
//             }   


//             ln = remote_list.head;

//             while( ln >= 0 ){

//                 list_node_t next_ln = list_ln_next( ln );

//                 remote_state_t *remote = list_vp_get_data( ln );

//                 if( sock_b_addr_compare( &raddr, &remote->addr ) ){

//                     // remove remote
//                     list_v_remove( &remote_list, ln );
//                     list_v_release_node( ln );

//                     trace_printf("LINK: remote shutdown\n");
//                 }

//                 ln = next_ln;
//             }


//             ln = consumer_list.head;

//             while( ln >= 0 ){

//                 list_node_t next_ln = list_ln_next( ln );

//                 consumer_state_t *consumer = list_vp_get_data( ln );

//                 // if timeout expires, or we are not link leader
//                 if( sock_b_addr_compare( &raddr, &consumer->addr ) ){

//                     // remove consumer
//                     list_v_remove( &consumer_list, ln );
//                     list_v_release_node( ln );

//                     trace_printf("LINK: consumer shutdown\n");
//                 }

//                 ln = next_ln;
//             }

//         }


// end:
    
//         THREAD_YIELD( pt );
//     }

PT_END( pt );
}

