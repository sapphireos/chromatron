

#include "sapphire.h"

#include "device_db.h"

static list_t device_list;
static uint8_t db_size;

static list_node_t device_ln;

static catbus_query_t current_query;

static socket_t sock;

KV_SECTION_META kv_meta_t devicedb_kv[] = {

	{ CATBUS_TYPE_UINT16, 	0, 0, 				   &db_size,							0,  "devicedb_size" },
};



static void send_device_msg( void );

PT_THREAD( device_server_thread( pt_t *pt, void *state ) );

static uint32_t device_db_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &device_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &device_list );
            break;

        default:
            len = 0;
            break;
    }

    return len;
}

PT_THREAD( device_db_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, DEVICE_DB_TICK * 1000 );

		send_device_msg();

		// process timeouts
		list_node_t ln = device_list.head;

	    while( ln >= 0 ){

	    	list_node_t next_ln = list_ln_next( ln );

	        device_data_t *device = list_vp_get_data( ln );

	        device->timeout -= DEVICE_DB_TICK;

	        if( device->timeout <= 0 ){

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

	// if( sys_u8_get_mode() == SYS_MODE_SAFE ){

	// 	return;
	// }

	fs_v_create_virtual( PSTR("device_db"), device_db_vfile );

	// create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );
    // yeah... we're fucked if an alloc fails here
    if( sock < 0 ){

    	log_v_critical_P( PSTR("super bad if this gets logged") );

    	return;
    }

    thread_t_create( device_db_thread,
                     PSTR("device_db"),
                     0,
                     0 );

   	thread_t_create( device_server_thread,
                PSTR("device_server"),
                0,
                0 );
}


// void device_db_v_process_announce( const catbus_msg_announce_t *announce, const sock_addr_t *raddr ){

// 	// log_v_debug_P( PSTR("announce %d.%d.%d.%d"), raddr->ipaddr.ip3, raddr->ipaddr.ip2, raddr->ipaddr.ip1, raddr->ipaddr.ip0 );

// 	list_node_t ln = device_list.head;

//     while( ln >= 0 ){

//         device_data_t *device = list_vp_get_data( ln );

//         if( ip_b_addr_compare( raddr->ipaddr, device->ip ) ){

//         	// update
//         	device->tags = announce->query;
//         	device->timeout = DEVICE_DB_TIMEOUT;

//         	return;
//         }

//         ln = list_ln_next( ln );     
//     }

//     // device not found

//     device_data_t device = {
//     	announce->query,
//     	raddr->ipaddr,
//     	DEVICE_DB_TIMEOUT
//     };

//     ln = list_ln_create_node2( &device, sizeof(device), MEM_TYPE_DEVICEDB );

//     if( ln < 0 ){

//     	return;
//     } 

//     list_v_insert_tail( &device_list, ln );

//     db_size = list_u8_count( &device_list );
// }


static void send_device_msg( void ){

	const catbus_hash_t32* tag_hashes_ptr = catbus_hp_get_tag_hashes();

	device_msg_t msg = {
		.magic = DEVICE_DB_MAGIC,
		.version = DEVICE_DB_VERSION,
		// .tags -> see below
		// .gfx_sync_group = vm_sync_u32_get_sync_group_hash(),
		.uptime = tmr_u64_get_system_time_ms(),
		.mode = sys_u8_get_mode(),
		.rssi = wifi_i8_rssi(),

		.gfx = {0},
		.batt = {0},
		.solar = {0},
	};

	// attach query tags
	memcpy( msg.query.tags, tag_hashes_ptr, sizeof(msg.query) );

	if( sys_u8_get_mode() != SYS_MODE_SAFE ){

		// attach gfx group
		// kv_i8_get( __KV__gfx_sync_group_hash, 	&msg.gfx.gfx_sync_group, 		sizeof(msg.gfx.gfx_sync_group) );
		kv_i8_get( __KV__sync_group_hash, 		&msg.gfx.gfx_sync_group, 		sizeof(msg.gfx.gfx_sync_group) );
		kv_i8_get( __KV__gfx_master_dimmer, 	&msg.gfx.gfx_master_dimmer, 	sizeof(msg.gfx.gfx_master_dimmer) );
		kv_i8_get( __KV__gfx_sub_dimmer, 		&msg.gfx.gfx_sub_dimmer, 		sizeof(msg.gfx.gfx_sub_dimmer) );
		kv_i8_get( __KV__gfx_enable, 			&msg.gfx.gfx_enable, 			sizeof(msg.gfx.gfx_enable) );
		kv_i8_get( __KV__superconductor_enabled,&msg.gfx.superconductor,		sizeof(msg.gfx.superconductor) );
		kv_i8_get( __KV__pixel_power, 			&msg.gfx.pixel_power, 			sizeof(msg.gfx.pixel_power) );

		if( kv_b_get_boolean( __KV__batt_enable ) ){

			kv_i8_get( __KV__batt_volts, 			&msg.batt.batt_volts, 			sizeof(msg.batt.batt_volts) );
			kv_i8_get( __KV__batt_charge_current,   &msg.batt.batt_charge_current,  sizeof(msg.batt.batt_charge_current) );
			kv_i8_get( __KV__batt_temp,   			&msg.batt.batt_temp,  			sizeof(msg.batt.batt_temp) );
			kv_i8_get( __KV__batt_charging,			&msg.batt.batt_status,  		sizeof(msg.batt.batt_status) );
			kv_i8_get( __KV__light_level,  			&msg.batt.light_level,  		sizeof(msg.batt.light_level) );
		}

		if( kv_b_get_boolean( __KV__solar_enable ) ){

			kv_i8_get( __KV__batt_aux_vbus_volts,		&msg.solar.solar_volts, 			sizeof(msg.solar.solar_volts) );
			kv_i8_get( __KV__batt_aux_charge_current,	&msg.solar.solar_charge_current, 	sizeof(msg.solar.solar_charge_current) );

			kv_i8_get( __KV__batt_case_temp,			&msg.solar.ambient_temp, 			sizeof(msg.solar.ambient_temp) );
			kv_i8_get( __KV__batt_ambient_temp,			&msg.solar.case_temp, 				sizeof(msg.solar.case_temp) );
		}
	}

	sock_addr_t raddr = {
        .ipaddr = ip_a_addr(255, 255, 255, 255),
        .port = DEVICE_DB_PORT
    };

    sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );
}

PT_THREAD( device_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	sock_v_bind( sock, DEVICE_DB_PORT );

	while(1){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        const device_msg_t *msg = sock_vp_get_data( sock );

        if( msg->magic != DEVICE_DB_MAGIC ){

            continue;
        }

        if( msg->version != DEVICE_DB_VERSION ){

        	continue;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        list_node_t ln = device_list.head;

	    while( ln >= 0 ){

	        device_data_t *device = list_vp_get_data( ln );

	        if( ip_b_addr_compare( raddr.ipaddr, device->ip ) ){

	        	// update
	        	device->tags 			= msg->query;
	        	device->gfx_sync_group 	= msg->gfx.gfx_sync_group;
	        	device->superconductor 	= msg->gfx.superconductor;
	        	device->uptime          = msg->uptime;
	        	device->mode          	= msg->mode;
	        	device->timeout 		= DEVICE_DB_TIMEOUT;

	        	goto done;
	        }

	        ln = list_ln_next( ln );     
	    }

	    // device not found

	    device_data_t device = {
	    	msg->query,
	    	raddr.ipaddr,
	    	msg->gfx.gfx_sync_group,
	    	msg->gfx.superconductor,
	    	msg->uptime,
	    	msg->mode,
	    	DEVICE_DB_TIMEOUT
	    };

	    ln = list_ln_create_node2( &device, sizeof(device), MEM_TYPE_DEVICEDB );

	    if( ln < 0 ){

	    	goto done;
	    } 

	    list_v_insert_tail( &device_list, ln );

done:
	    db_size = list_u8_count( &device_list );
	}

PT_END( pt );
}