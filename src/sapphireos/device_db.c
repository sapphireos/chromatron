

#include "sapphire.h"

#include "controller.h"
#include "device_db.h"


static list_t device_list;
static uint8_t db_size;

static list_node_t device_ln;

static catbus_query_t current_query;

KV_SECTION_META kv_meta_t devicedb_kv[] = {

	{ CATBUS_TYPE_UINT16, 	0, 0, 				   &db_size,							0,  "devicedb_size" },
};


// static device_data_t* _update_device_status( mqtt_msg_status_t *msg ){

// 	list_node_t ln = device_list.head;

//     while( ln >= 0 ){

//         device_data_t *status = list_vp_get_data( ln );

//         if( ip_b_addr_compare( msg->ip, status->status.ip ) ){

//         	// update
//         	status->status = *msg;
//         	status->timeout = DEVICE_TIMEOUT;

//         	return status;
//         }

//         ln = list_ln_next( ln );     
//     }

//     // device not found

//     device_data_t status = {
//     	*msg,
//     	{ 0 },
//     	DEVICE_TIMEOUT
//     };

//     ln = list_ln_create_node2( &status, sizeof(status), MEM_TYPE_DEVICEDB );

//     if( ln < 0 ){

//     	return 0;
//     } 

//     trace_printf("device DB add: %d.%d.%d.%d\n", status.status.ip.ip3, status.status.ip.ip2, status.status.ip.ip1, status.status.ip.ip0 );
//     // log_v_debug_P("device DB add: %d.%d.%d.%d\n", status.status.ip.ip3, status.status.ip.ip2, status.status.ip.ip1, status.status.ip.ip0 );

//     list_v_insert_tail( &device_list, ln );

//     return list_vp_get_data( ln );
// }



// static void mqtt_on_publish_status_callback( char *topic, uint8_t *data, uint16_t data_len, sock_addr_t *raddr ){

// 	mqtt_msg_status_t *msg = (mqtt_msg_status_t *)data;

// 	if( data_len != sizeof(mqtt_msg_status_t) ){

// 		log_v_error_P( PSTR("Invalid msg: %d != %d"), data_len, sizeof(mqtt_msg_status_t) );
// 		return;
// 	}

// 	_update_device_status( msg );
// }

// static void mqtt_on_publish_batt_callback( char *topic, uint8_t *data, uint16_t data_len, sock_addr_t *raddr ){

// 	batt_status_t *msg = (batt_status_t *)data;

// 	if( data_len != sizeof(batt_status_t) ){

// 		log_v_error_P( PSTR("Invalid msg: %d != %d"), data_len, sizeof(batt_status_t) );
// 		return;
// 	}

// 	// _update_device_status( msg );

// 	device_data_t *status = device_db_p_get_ipaddr( raddr->ipaddr );

// 	if( status == 0 ){

// 		return;
// 	}

// 	status->batt_status = *msg;
// }


PT_THREAD( device_db_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, 1000 );

		// process timeouts
		list_node_t ln = device_list.head;

	    while( ln >= 0 ){

	    	list_node_t next_ln = list_ln_next( ln );

	        device_data_t *device = list_vp_get_data( ln );

	        device->timeout--;

	        if( device->timeout == 0 ){

	        	trace_printf("device DB timeout: %d.%d.%d.%d\n", device->ip.ip3, device->ip.ip2, device->ip.ip1, device->ip.ip0 );

	        	list_v_remove( &device_list, ln );
	        	list_v_release_node( ln );
	        }

	        ln = next_ln;     
	    }

	    db_size = list_u8_count( &device_list );
	}

PT_END( pt );
}

uint8_t device_db_u8_count( void ){

	return list_u8_count( &device_list );
}

bool device_db_b_is_all_query( void ){

	for( uint8_t i = 0; i < cnt_of_array(current_query.tags); i++ ){

		if( current_query.tags[i] != 0 ){

			return FALSE;
		}
	}

	return TRUE;
}

void device_db_v_set_query( const catbus_query_t *query ){

	current_query = *query;
}

void device_db_v_get_query( catbus_query_t *query ){

	*query = current_query;
}

void device_db_v_reset_iter( void ){

	device_ln = device_list.head;
}

device_data_t* device_db_p_get_next( void ){

	if( device_ln <= 0 ){

		return 0;
	}

	device_data_t *device = list_vp_get_data( device_ln );

	device_ln = list_ln_next( device_ln );     

	return device;
}

bool device_db_b_has_hash( catbus_hash_t32 hash ){

	device_db_v_reset_iter();
    const device_data_t *device = device_db_p_get_next_query( 0 );

    while( device != 0 ){

    	for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

    		if( device->tags.tags[i] == hash ){

    			return TRUE;
    		}
    	}

    	device = device_db_p_get_next_query( 0 );
    }

    return FALSE;
}

void device_db_v_sort_name( void ){

	bool swapped = TRUE;

	while( swapped ){

		swapped = FALSE;

		list_node_t cur = device_list.head;

		if( cur <= 0 ){

			return;
		}

		list_node_t next = list_ln_next( cur );
	
		while( ( cur > 0 ) && ( next > 0 ) ){

			device_data_t *cur_device = list_vp_get_data( cur );
			device_data_t *next_device = list_vp_get_data( next );

			catbus_string_t name1 = { 0 };
			catbus_i8_get_string_for_hash( cur_device->tags.tags[0], name1.str,  &cur_device->ip );

			catbus_string_t name2 = { 0 };
			catbus_i8_get_string_for_hash( next_device->tags.tags[0], name2.str,  &next_device->ip );

			if( strcmp( name1.str, name2.str ) > 0 ){

				list_node_t prev = list_ln_prev( cur );

				list_v_remove( &device_list, cur );
				list_v_remove( &device_list, next );		

				if( prev > 0 ){

					list_v_insert_after( &device_list, prev, next );
				}
				else{

					list_v_insert_head( &device_list, next );
				}

				list_v_insert_after( &device_list, next, cur );

				swapped = TRUE;
			}

			cur = next;
			next = list_ln_next( next );
		}
	}
}

// bool query_single( catbus_hash_t32 hash, catbus_query_t *tags ){

// 	if( hash == 0 ){

// 		return TRUE;
// 	}

// 	for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

// 		if( hash == tags->tags[i] ){

// 			return TRUE;
// 		}
// 	}

// 	return FALSE;
// }

// // check if tags matches query
// bool query_tags( catbus_query_t *query, catbus_query_t *tags ){

// 	// trace_printf("query\n");

// 	// trace_printf("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
//     //         query->tags[0],
//     //         query->tags[1],
//     //         query->tags[2],
//     //         query->tags[3],
//     //         query->tags[4],
//     //         query->tags[5],
//     //         query->tags[6],
//     //         query->tags[7]
//     //     );
// 	// trace_printf("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
//     //         tags->tags[0],
//     //         tags->tags[1],
//     //         tags->tags[2],
//     //         tags->tags[3],
//     //         tags->tags[4],
//     //         tags->tags[5],
//     //         tags->tags[6],
//     //         tags->tags[7]
//     //     );

// 	for( uint8_t i = 0; i < CATBUS_QUERY_LEN; i++ ){

// 		if( !query_single( query->tags[i], tags ) ){

// 			return FALSE;
// 		}
// 	}

// 	return TRUE;
// }

device_data_t* device_db_p_get_next_query( catbus_query_t *query ){

	if( query == 0 ){

		query = &current_query;
	}

	device_data_t *device = device_db_p_get_next();

	while( device != 0 ){

		// trace_printf("query device %d.%d.%d.%d\n", device->status.ip.ip3, device->status.ip.ip2, device->status.ip.ip1, device->status.ip.ip0);

		if( catbus_b_query_tags( query, &device->tags ) ){

			return device;
		}

		device = device_db_p_get_next();
	}

	return 0;
}


uint8_t device_db_u8_query_count( catbus_query_t *query ){
	
	if( query == 0 ){

		query = &current_query;
	}

	uint8_t count = 0;

	device_db_v_reset_iter();

	device_data_t *device = device_db_p_get_next();

	while( device != 0 ){

		// trace_printf("query device %d.%d.%d.%d\n", device->status.ip.ip3, device->status.ip.ip2, device->status.ip.ip1, device->status.ip.ip0);

		if( catbus_b_query_tags( query, &device->tags ) ){

			count++;
		}

		device = device_db_p_get_next();
	}

	return count;
}


device_data_t* device_db_p_get_ipaddr( ip_addr4_t ip ){

	list_node_t ln = device_list.head;

    while( ln >= 0 ){

    	list_node_t next_ln = list_ln_next( ln );

        device_data_t *device = list_vp_get_data( ln );

        if( ip_b_addr_compare( device->ip, ip ) ){

        	return device;
        }

        ln = next_ln;     
    }

    return 0;
}

void device_db_v_init( void ){

	list_v_init( &device_list );

    thread_t_create( device_db_thread,
                     PSTR("device_db"),
                     0,
                     0 );
}

// void device_db_v_get_file_hash_list( catbus_file_hash_list_callback_t callback ){

// 	device_db_v_reset_iter();

// 	const device_data_t *device = device_db_p_get_next_query( 0 );

// 	while( device != 0 ){

// 		catbus_v_get_file_hash_list( device->status.ip, callback );

// 		device = device_db_p_get_next_query( 0 );
// 	}
// }

// void device_db_v_get_key( catbus_hash_t32 hash, catbus_get_key_callback_t callback ){

// 	device_db_v_reset_iter();

// 	const device_data_t *device = device_db_p_get_next_query( 0 );

// 	while( device != 0 ){

// 		catbus_v_get_key( device->status.ip, hash, callback );

// 		device = device_db_p_get_next_query( 0 );
// 	}
// }

// void device_db_v_set_key( catbus_hash_t32 hash, catbus_type_t8 type, uint8_t *data, uint16_t data_len ){

// 	device_db_v_reset_iter();

// 	const device_data_t *device = device_db_p_get_next_query( 0 );

// 	while( device != 0 ){

// 		catbus_v_set_key( device->status.ip, hash, type, data );

// 		device = device_db_p_get_next_query( 0 );
// 	}
// }



void device_db_v_process_announce( catbus_msg_announce_t *annouce ){

	
}


