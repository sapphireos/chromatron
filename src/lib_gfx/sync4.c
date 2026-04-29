
#include "sapphire.h"

#include "vm4.h"
#include "config.h"
#include "sequencer.h"
#include "sync4.h"
#include "gfx_lib.h"
#include "device_db.h"


static socket_t server_sock;
static uint32_t sync_group_hash;
static uint8_t data_server_count;

static ip_addr4_t leader_ip;

static uint8_t sync_state;
#define SYNC_STATE_IDLE 	0
#define SYNC_STATE_LEADER 	1
#define SYNC_STATE_CONNECT 	2
#define SYNC_STATE_SYNCING 	3
#define SYNC_STATE_DATA 	4
#define SYNC_STATE_SYNCED 	5



int8_t sync4_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__sync_group ){

            sync_group_hash = hash_u32_string( data );    
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t sync4_kv[] = {
    { CATBUS_TYPE_STRING32, 0,		KV_FLAGS_PERSIST,   0,                  sync4_i8_kv_handler,   "sync_group" },
    { CATBUS_TYPE_UINT32,   0,  	KV_FLAGS_READ_ONLY, &sync_group_hash,   0,                     "sync_group_hash" },
    // { CATBUS_TYPE_UINT8,    0,                          KV_FLAGS_READ_ONLY, &sync_state,        0,                      "gfx_sync_state" },
    { CATBUS_TYPE_IPv4,     0,      KV_FLAGS_READ_ONLY, &leader_ip,         0,                      "sync_leader_ip" },
    // { CATBUS_TYPE_UINT8,    0,                          KV_FLAGS_READ_ONLY, &sync_least_hits,   0,                      "gfx_sync_least_hits" },
    // { CATBUS_TYPE_UINT8,    0,                          KV_FLAGS_READ_ONLY, &sync_most_hits,    0,                      "gfx_sync_most_hits" },
    // { CATBUS_TYPE_UINT32,   0,                          KV_FLAGS_READ_ONLY, &sync_losses,       0,                      "gfx_sync_sync_losses" },
};

PT_THREAD( sync4_server_thread( pt_t *pt, void *state ) );
PT_THREAD( sync4_thread( pt_t *pt, void *state ) );

static void serialize_pixels( void ){

	uint16_t pix_count = gfx_u16_get_pix_count();
	uint16_t array_size = sizeof(uint16_t) * pix_count;

	uint16_t *h_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE );
	uint16_t *s_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *v_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *hsfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HS_FADE );	
	uint16_t *vfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_V_FADE );	
	uint16_t *h_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE_STEP );
	uint16_t *s_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT_STEP );	
	uint16_t *v_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_VAL_STEP );

	fs_v_delete_fname_P( PSTR("_pixel.f4b") );

	file_t f = fs_f_open_P( PSTR("_pixel.f4b"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if(f <= 0){

        return;
    }

    uint32_t magic = SYNC4_PROTOCOL_MAGIC;
    fs_i16_write( f, (uint8_t *)&magic, sizeof(magic) );
    fs_i16_write( f, (uint8_t *)&pix_count, sizeof(pix_count) );
    fs_i16_write( f, (uint8_t *)&array_size, sizeof(array_size) );

	fs_i16_write( f, (uint8_t *)h_ptr, array_size );
	fs_i16_write( f, (uint8_t *)s_ptr, array_size );
	fs_i16_write( f, (uint8_t *)v_ptr, array_size );
	fs_i16_write( f, (uint8_t *)hsfade_ptr, array_size );
	fs_i16_write( f, (uint8_t *)vfade_ptr, array_size );
	fs_i16_write( f, (uint8_t *)h_step_ptr, array_size );
	fs_i16_write( f, (uint8_t *)s_step_ptr, array_size );
	fs_i16_write( f, (uint8_t *)v_step_ptr, array_size );
	
}

static void init_group_hash( void ){

    // init sync group hash
    char buf[32];
    memset( buf, 0, sizeof(buf) );
    kv_i8_get( __KV__gfx_sync_group, buf, sizeof(buf) );

    sync_group_hash = hash_u32_string( buf );    
}

void sync4_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

	log_v_debug_P( PSTR("Sync4 init") );

	init_group_hash();
	// serialize_pixels();

	thread_t_create( sync4_thread,
                    PSTR("sync4"),
                    0,
                    0 );    
}


bool _query_leader( ip_addr4_t *ip ){

    // set leader to 0s
    // leader_ip = ip_a_addr( 0, 0, 0, 0 );

    *ip = ip_a_addr( 0, 0, 0, 0 );

    // check if we have a sync group
    if( sync_group_hash == 0 ){

        // then we are we here lol
        return FALSE;
    }

    // set ourselves as best leader so far
    ip_addr4_t my_addr;
    cfg_i8_get( CFG_PARAM_IP_ADDRESS, &my_addr );
    leader_ip = my_addr;

    uint32_t best_ip = ip_u32_to_int( my_addr );

    device_db_v_reset_iter();

    const device_data_t *device = device_db_p_get_next();

    while( device != 0 ){

        // check for sync group match
        if( device->gfx_sync_group != sync_group_hash ){

            goto next;
        }

        uint32_t ip_int = ip_u32_to_int( device->ip );

        // biggest IP wins
        if( ip_int > best_ip ){

            best_ip = ip_int;

            *ip = device->ip;
        }

next:
        device = device_db_p_get_next();
    }    

    return ip_b_is_zeroes( *ip );
}

bool sync4_b_is_leader( void ){

	_query_leader( &leader_ip );

    return ip_b_check_dest( leader_ip );
}

bool sync4_b_is_follower( void ){

	_query_leader( &leader_ip );

    if( ip_b_is_zeroes( leader_ip ) ){

        return FALSE;
    }

    if( !ip_b_check_dest( leader_ip ) ){

        return TRUE;
    }

    return FALSE;
}


typedef struct{
	socket_t sock;
	sock_addr_t raddr;
	uint8_t vm_pages;
	uint8_t pixel_pages;
	uint16_t vm_size;
	uint16_t pixel_size;
} data_server_state_t;

PT_THREAD( data_server_thread( pt_t *pt, data_server_state_t *state ) )
{
PT_BEGIN( pt );

	sock_v_set_timeout( state->sock, 4 );

	while( sync4_b_is_leader() ){

		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( state->sock ) < 0 );

		if( sock_i16_get_bytes_read( state->sock ) <= 0 ){

			// timeout!
			break; // terminate server
		}

		// check response
		const sync4_msg_header_t *header = (sync4_msg_header_t *)sock_vp_get_data( state->sock );

		if( header->magic != SYNC4_PROTOCOL_MAGIC ){

			continue;
		}

		if( header->version != SYNC4_PROTOCOL_VERSION ){

			continue;
		}

		if( header->type != SYNC4_MSG_TYPE_REQ_DATA ){

			continue;
		}

		const sync4_msg_data_t *msg = (sync4_msg_data_t *)( header + 1 );




	}


	data_server_count--;
	sock_v_release( state->sock );
	
PT_END( pt );
}


static void send_ready( socket_t sock, sock_addr_t *raddr, uint8_t vm_pages, uint8_t pixel_pages ){

	sync4_msg_ready_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_READY;

	msg.vm_pages 		= vm_pages;
	msg.pixel_pages 	= pixel_pages;

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );
}

PT_THREAD( sync4_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    server_sock = sock_s_create( SOS_SOCK_DGRAM ); 

    if( server_sock < 0 ){

    	log_v_error_P( PSTR("alloc fail") );

    	THREAD_EXIT( pt );
    }

    sock_v_bind( server_sock, SYNC4_SERVER_PORT );

    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, 
            ( sock_i8_recvfrom( server_sock ) < 0 ) &&
             ( !sys_b_is_shutting_down() ) );

    	// check if shutting down
    	if( sys_b_is_shutting_down() ){

            log_v_debug_P( PSTR("VM sync server shut down") );

    		THREAD_EXIT( pt );
    	}

        sock_addr_t raddr;
        sock_v_get_raddr( server_sock, &raddr );

        const sync4_msg_header_t *header = (sync4_msg_header_t *)sock_vp_get_data( server_sock );

		if( header->magic != SYNC4_PROTOCOL_MAGIC ){

			continue;
		}

		if( header->version != SYNC4_PROTOCOL_VERSION ){

			continue;
		}

        if( sync4_b_is_leader() ){

        	if( header->type == SYNC4_MSG_TYPE_CONNECT ){

        		data_server_state_t server_state = {0};
        		server_state.sock = sock_s_create( SOS_SOCK_DGRAM );
        		server_state.raddr = raddr;

        		if( server_state.sock < 0 ){

        			log_v_error_P( PSTR("alloc fail") );

        			continue;
        		}

        		vm4_v_freeze_vm( 0 );
        		serialize_pixels();

        		server_state.vm_size = fs_i32_get_size_fname_P( PSTR("_sync.f4b") );
        		server_state.pixel_size = fs_i32_get_size_fname_P( PSTR("_pixel.f4b") );

        		if( server_state.vm_size == 0 ){

        			continue;
        		}

        		if( server_state.pixel_size == 0 ){

        			continue;
        		}

        		if( thread_t_create( 
					THREAD_CAST(data_server_thread),
                    PSTR("sync4_data_server"),
                    &server_state,
                    sizeof(data_server_state_t) ) < 0 ){

        			log_v_error_P( PSTR("alloc fail") );

        			continue;
        		}        		

        		uint16_t vm_pages = server_state.vm_size / SYNC4_MAX_DATA;
        		if( server_state.vm_size % SYNC4_MAX_DATA ){

        			vm_pages++;
        		}

        		uint16_t pixel_pages = server_state.pixel_size / SYNC4_MAX_DATA;
        		if( server_state.pixel_size % SYNC4_MAX_DATA ){

        			pixel_pages++;
        		}

        		server_state.vm_pages = vm_pages;
        		server_state.pixel_pages = pixel_pages;

        		send_ready( server_state.sock, &server_state.raddr, server_state.vm_pages, server_state.pixel_pages );

        		data_server_count++;
        	}
        }
        else if( sync4_b_is_follower() ){


        	
        }
        else{

        	// neither
        }
    }

PT_END( pt );
}




static void send_connect( socket_t sock ){

	sync4_msg_connect_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_CONNECT;

	sock_addr_t raddr = {
		leader_ip,
		SYNC4_SERVER_PORT
	};

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void send_data_request( socket_t sock, uint8_t current_page ){

	sync4_msg_request_data_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_REQ_DATA;

	msg.page = current_page;

	sock_addr_t raddr = {
		leader_ip,
		SYNC4_SERVER_PORT
	};

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );
}

typedef struct{
	socket_t sock;
	uint8_t current_page;
	uint8_t total_pages;
	uint8_t tries;
} data_client_state_t;


PT_THREAD( data_client_thread( pt_t *pt, data_client_state_t *state ) )
{
PT_BEGIN( pt );

	memset( state, 0, sizeof(data_client_state_t) );
	
	state->sock = sock_s_create( SOS_SOCK_DGRAM );
	sock_v_set_timeout( state->sock, 1 );

	while( sync_state == SYNC_STATE_SYNCING ){

		state->tries = SYNC4_MAX_TRIES;
		while( state->tries > 0 ){

			state->tries--;

			send_data_request( state->sock, state->current_page );
				
			THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( state->sock ) < 0 );

			if( sock_i16_get_bytes_read( state->sock ) <= 0 ){

				continue;
			}

			if( sync_state != SYNC_STATE_SYNCING ){

				break;
			}

			// check response
			const sync4_msg_header_t *header = (sync4_msg_header_t *)sock_vp_get_data( state->sock );

			if( header->magic != SYNC4_PROTOCOL_MAGIC ){

				continue;
			}

			if( header->version != SYNC4_PROTOCOL_VERSION ){

				continue;
			}

			if( header->type != SYNC4_MSG_TYPE_SYNC_DATA ){

				continue;
			}

			const sync4_msg_data_t *msg = (sync4_msg_data_t *)( header + 1 );

			if( msg->page != state->current_page ){

				continue;
			}

			if( msg->total == 0 ){

				goto error;
			}

			if( state->total_pages == 0 ){

				state->total_pages = msg->total;
			}

			if( state->total_pages != msg->total ){

				goto error;
			}

			// get data
			uint8_t *data = (uint8_t *)( msg + 1 );
			uint16_t data_len = sock_i16_get_bytes_read( state->sock ) - sizeof(sync4_msg_data_t);

			log_v_debug_P( PSTR("received %d bytes type %d page: %d total: %d"), 
				data_len,
				msg->type,
				state->current_page,
				state->total_pages
			);

			state->current_page++;

			if( state->current_page >= state->total_pages ){

				sync_state = SYNC_STATE_DATA;

				goto done;
			}
			
			state->tries = SYNC4_MAX_TRIES;
		}


		// retries expired
		if( state->tries == 0 ){

			goto error;
		}
	}


error:
	sync_state = SYNC_STATE_IDLE;


done:

	sock_v_release( state->sock );
	
PT_END( pt );
}















PT_THREAD( sync4_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	sync_state = SYNC_STATE_IDLE;

	THREAD_WAIT_WHILE( pt, sync_group_hash == 0 );
	
	thread_t_create( sync4_server_thread,
                    PSTR("sync4_server"),
                    0,
                    0 );
	
	while(1){

		// // check for leader
		// ip_addr4_t next_leader;
		// if( !_query_leader( &next_leader ) ){

		// 	// no leader
		// 	leader_ip = ip_a_addr( 0, 0, 0, 0 );

		// 	TMR_WAIT( pt, 1000 );

		// 	THREAD_RESTART( pt );
		// }

		// // check for leader change
		// if( !ip_b_addr_compare( next_leader, leader_ip ) ){

		// 	// leader changed

		// 	leader_ip = next_leader;

		// 	log_v_debug_P( PSTR("Leader changed to %d.%d.%d.%d"),
		// 		leader_ip.ip3,
		// 		leader_ip.ip2,
		// 		leader_ip.ip1,
		// 		leader_ip.ip0
		// 	);


		// }

		// check if leader
		if( sync4_b_is_leader() ){

			sync_state = SYNC_STATE_LEADER;

			THREAD_WAIT_WHILE( pt, sync4_b_is_leader() );

			// while( sync4_b_is_leader() ){
			// }
		}

		// check if follower
		if( sync4_b_is_follower() ){

			sync_state = SYNC_STATE_CONNECT;

			send_connect( server_sock );

			THREAD_WAIT_WHILE( pt, 
				( sync_state == SYNC_STATE_CONNECT ) &&
				sync4_b_is_follower()
			);
			
				
			
			
			
			// thread_t_create( 
			// 		THREAD_CAST(data_client_thread),
            //         PSTR("sync4_client"),
            //         0,
            //         sizeof(data_client_state_t) );

			// THREAD_WAIT_WHILE( pt, SYNC_STATE_SYNCING );

			// THREAD_WAIT_WHILE( pt, sync4_b_is_follower() && );

			// while( sync4_b_is_follower() ){




			// }
		}



	}


	
	
PT_END( pt );
}











