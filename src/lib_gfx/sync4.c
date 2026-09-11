
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

static bool enabled;
static bool hold;


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
	{ CATBUS_TYPE_BOOL,     0,		KV_FLAGS_PERSIST,   &enabled,           0,   				   "sync_enable" },
    { CATBUS_TYPE_STRING32, 0,		KV_FLAGS_PERSIST,   0,                  sync4_i8_kv_handler,   "sync_group" },
    { CATBUS_TYPE_UINT32,   0,  	KV_FLAGS_READ_ONLY, &sync_group_hash,   0,                     "sync_group_hash" },
    { CATBUS_TYPE_UINT8,    0,      KV_FLAGS_READ_ONLY, &sync_state,        0,                     "sync_state" },
    { CATBUS_TYPE_IPv4,     0,      KV_FLAGS_READ_ONLY, &leader_ip,         0,                     "sync_leader_ip" },
};

PT_THREAD( sync4_server_thread( pt_t *pt, void *state ) );
PT_THREAD( sync4_thread( pt_t *pt, void *state ) );

static bool is_enabled(void){

	if(sync_group_hash == 0){

		return FALSE;
	}

	if(!vm4_b_is_vm_running( 0 )){

		return FALSE;
	}

	if(hold){

		return FALSE;
	}

	// return kv_b_get_boolean( __KV__sync_enable );
	return enabled;
}

static void serialize_pixels( void ){

	uint16_t pix_count = gfx_u16_get_pix_count();
	uint16_t array_size = sizeof(uint16_t) * pix_count;

	uint16_t *h_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE );
	uint16_t *s_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *v_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_VAL );	
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
	
	fs_f_close( f );
}

static void deserialize_pixels( void ){

	uint16_t pix_count = gfx_u16_get_pix_count();

	uint16_t *h_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE );
	uint16_t *s_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT );	
	uint16_t *v_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_VAL );	
	uint16_t *hsfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HS_FADE );	
	uint16_t *vfade_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_V_FADE );	
	uint16_t *h_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_HUE_STEP );
	uint16_t *s_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_SAT_STEP );	
	uint16_t *v_step_ptr = _gfx_u16p_get_array_ptr( PIX_ARRAY_ATTR_VAL_STEP );


	file_t f = fs_f_open_P( PSTR("_pixel.f4b"), FS_MODE_READ_ONLY );

    if(f <= 0){

        return;
    }

    uint32_t magic = 0;
    fs_i16_read( f, (uint8_t *)&magic, sizeof(magic) );

    uint16_t file_pix_count = 0;
    fs_i16_read( f, (uint8_t *)&file_pix_count, sizeof(file_pix_count) );

    uint16_t file_array_size = 0;
    fs_i16_read( f, (uint8_t *)&file_array_size, sizeof(file_array_size) );

    if( pix_count > file_pix_count ){

    	pix_count = file_pix_count;
    }

    uint16_t array_size = sizeof(uint16_t) * pix_count;

	fs_i16_read( f, (uint8_t *)h_ptr, array_size );
	fs_i16_read( f, (uint8_t *)s_ptr, array_size );
	fs_i16_read( f, (uint8_t *)v_ptr, array_size );
	fs_i16_read( f, (uint8_t *)hsfade_ptr, array_size );
	fs_i16_read( f, (uint8_t *)vfade_ptr, array_size );
	fs_i16_read( f, (uint8_t *)h_step_ptr, array_size );
	fs_i16_read( f, (uint8_t *)s_step_ptr, array_size );
	fs_i16_read( f, (uint8_t *)v_step_ptr, array_size );

    fs_f_close( f );
}

static void init_group_hash( void ){

    // init sync group hash
    char buf[32];
    memset( buf, 0, sizeof(buf) );
    kv_i8_get( __KV__sync_group, buf, sizeof(buf) );

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

void sync4_v_reset( void ){

	leader_ip = ip_a_addr( 0, 0, 0, 0 );
	sync_state = SYNC_STATE_IDLE;
}

bool sync4_b_is_sync( void ){

	return sync_state == SYNC_STATE_SYNCED;	
}

bool _query_leader( ip_addr4_t *ip ){

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


static void send_ready( socket_t sock, sock_addr_t *raddr, uint8_t vm_pages, uint8_t pixel_pages ){

	sync4_msg_ready_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_READY;

	msg.vm_pages 		= vm_pages;
	msg.pixel_pages 	= pixel_pages;

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );
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

	log_v_debug_P( PSTR("data server start lport: %d"), sock_u16_get_lport( state->sock ) );

	send_ready( state->sock, &state->raddr, state->vm_pages, state->pixel_pages );

	sock_v_set_timeout( state->sock, 4 );

	while( sync4_b_is_leader() ){

		THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( state->sock ) < 0 ) && ( is_enabled() ) );

		// check if enabled
		if(!is_enabled()){

			break;
		}

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

		const sync4_msg_request_data_t *msg = (sync4_msg_request_data_t *)header;

		uint8_t buf[sizeof(sync4_msg_data_t) + SYNC4_MAX_DATA] = {0};
		sync4_msg_data_t *reply = (sync4_msg_data_t *)buf;
		uint8_t *data = (uint8_t *)( reply + 1 );

		reply->header.magic 	= SYNC4_PROTOCOL_MAGIC;
		reply->header.version 	= SYNC4_PROTOCOL_VERSION;
		reply->header.type 		= SYNC4_MSG_TYPE_DATA;

		file_t f = -1;
		uint16_t offset = 0;

		if( msg->page < state->vm_pages ){

			// VM page
			reply->type = SYNC4_DATA_TYPE_VM;

			offset = msg->page * SYNC4_MAX_DATA;
			
			f = fs_f_open_P( PSTR("_sync.f4b"), FS_MODE_READ_ONLY );
		}
		else{

			// pixel page
			reply->type = SYNC4_DATA_TYPE_PIXELS;

			offset = ( msg->page - state->vm_pages ) * SYNC4_MAX_DATA;

			f = fs_f_open_P( PSTR("_pixel.f4b"), FS_MODE_READ_ONLY );
		}

		reply->page = msg->page;
		reply->total = state->vm_pages + state->pixel_pages;

		log_v_debug_P( PSTR("receive req data: page: %d offset: %d"), msg->page, offset );

		if( f < 0 ){

			log_v_error_P( PSTR("file error") );

			goto done;
		}

		fs_v_seek( f, offset );
		int16_t read_len = fs_i16_read( f, data, SYNC4_MAX_DATA );

		fs_f_close( f );

		if( read_len < 0 ){

			log_v_error_P( PSTR("file error: %d"), read_len );

			goto done;
		}

		log_v_debug_P( PSTR("send data") );

		sock_i16_sendto( state->sock, buf, sizeof(sync4_msg_data_t) + read_len, &state->raddr );
	}


done:
	data_server_count--;
	sock_v_release( state->sock );

	log_v_debug_P( PSTR("data server stop") );
	
PT_END( pt );
}



PT_THREAD( sync4_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, 
    		( is_enabled() ) &&
            ( sock_i8_recvfrom( server_sock ) < 0 ) &&
             ( !sys_b_is_shutting_down() ) );

    	// check if shutting down
    	if( sys_b_is_shutting_down() ){

            log_v_debug_P( PSTR("VM sync server shut down") );

    		goto done;
    	}

    	// check if enabled
		if(!is_enabled()){

			// exit thread if not
			goto done;
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

        	if( header->type == SYNC4_MSG_TYPE_REQ_SYNC ){

        		vm_t vm = {0};
        		vm4_v_get_vm_state( &vm, 0 );

        		const sync4_msg_request_sync_t *msg = (sync4_msg_request_sync_t *)header;

        		sync4_msg_sync_t reply = {
        			.header.magic 		= SYNC4_PROTOCOL_MAGIC,
        			.header.version 	= SYNC4_PROTOCOL_VERSION,
        			.header.type 		= SYNC4_MSG_TYPE_SYNC,
        			.header.flags 		= 0,
        			.header.padding		= 0,

        			.current_tick 		= vm.current_tick,
        			.rng_seed 			= vm.rng_seed,
        			.frame_number 		= vm.frame_number,

        			.net_time_client	= msg->net_time,
        			.net_time_server    = tmr_u32_get_system_time_ms()
        		};

        		sock_i16_sendto( server_sock, (uint8_t *)&reply, sizeof(reply), &raddr );

        		// log_v_debug_P( PSTR("request sync current_tick: %lld frame_number: %lld rng: %lld"),
			    //         vm.current_tick,
			    //         vm.frame_number,
			    //         vm.rng_seed
			    //     );
        	}
        	else if( header->type == SYNC4_MSG_TYPE_CONNECT ){

        		if( data_server_count >= SYNC4_MAX_DATA_SERVERS ){

        			log_v_debug_P( PSTR("max data servers") );		

        			continue;
        		}

        		log_v_debug_P( PSTR("receive connect") );

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

        		if( thread_t_create( 
					THREAD_CAST(data_server_thread),
                    PSTR("sync4_data_server"),
                    &server_state,
                    sizeof(data_server_state_t) ) < 0 ){

        			log_v_error_P( PSTR("alloc fail") );

        			continue;
        		}        		

        		data_server_count++;
        	}
        }
        else if( sync4_b_is_follower() ){

        	if( header->type == SYNC4_MSG_TYPE_SYNC ){

        		const sync4_msg_sync_t *msg = (sync4_msg_sync_t *)header;

        		uint32_t now = tmr_u32_get_system_time_ms();

        		int32_t time_delta = (int64_t)now - (int64_t)msg->net_time_client;

        		// compute basic RTT
        		int32_t rtt = time_delta / 2;

    			vm_t vm = {0};
    			vm4_v_get_vm_state( &vm, 0 );

    			int32_t delta = (int64_t)msg->current_tick - (int64_t)vm.current_tick;

    			log_v_debug_P( PSTR("sync: server tick: %12ld local tick: %12ld delta: %4ld time delta: %4ld"),
			            (uint32_t)msg->current_tick,
			            (uint32_t)vm.current_tick,
			            delta,
			            time_delta
			        );

			
        		sync_state = SYNC_STATE_SYNCED;

        		// compensate from RTT milliseconds to GFX ticks (20 ms)
        		uint32_t delay_ticks = rtt / FADER_RATE;

        		// sync
        		vm4_v_sync( now, msg->current_tick + delay_ticks );
        	}
        }
        else{

        	// neither
        }
    }

done:
	sock_v_release( server_sock );
	server_sock = -1;

	log_v_debug_P( PSTR("server stop") );

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

static void send_sync_request( socket_t sock ){

	sync4_msg_request_sync_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_REQ_SYNC;
	// msg.net_time        = time_u32_get_network_time();
	msg.net_time        = tmr_u32_get_system_time_ms();

	sock_addr_t raddr = {
		leader_ip,
		SYNC4_SERVER_PORT
	};

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );
}

static void send_data_request( socket_t sock, sock_addr_t *raddr, uint8_t current_page ){

	sync4_msg_request_data_t msg = {0};

	msg.header.magic 	= SYNC4_PROTOCOL_MAGIC;
	msg.header.version 	= SYNC4_PROTOCOL_VERSION;
	msg.header.type 	= SYNC4_MSG_TYPE_REQ_DATA;

	msg.page = current_page;

	sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );
}

typedef struct{
	socket_t sock;
	sock_addr_t raddr;
	uint8_t current_page;
	uint8_t total_pages;
	uint8_t tries;
	uint8_t vm_pages;
	uint8_t pixel_pages;
} data_client_state_t;


PT_THREAD( data_client_thread( pt_t *pt, data_client_state_t *state ) )
{
PT_BEGIN( pt );

	log_v_debug_P( PSTR("data client start") );

	memset( state, 0, sizeof(data_client_state_t) );
	
	state->sock = sock_s_create( SOS_SOCK_DGRAM );
	sock_v_set_timeout( state->sock, 2 );

	// send connect
	state->tries = SYNC4_MAX_TRIES;
	while( state->tries > 0 ){

		state->tries--;
		send_connect( state->sock );
		log_v_debug_P( PSTR("send connect") );

		THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( state->sock ) < 0 ) && ( is_enabled() ) );

		// check if enabled
		if(!is_enabled()){

			goto done;
		}

		if( sock_i16_get_bytes_read( state->sock ) <= 0 ){

			continue;
		}

		// check response
		const sync4_msg_header_t *header = (sync4_msg_header_t *)sock_vp_get_data( state->sock );

		if( header->magic != SYNC4_PROTOCOL_MAGIC ){

			continue;
		}

		if( header->version != SYNC4_PROTOCOL_VERSION ){

			continue;
		}

		if( header->type != SYNC4_MSG_TYPE_READY ){

			continue;
		}

		sock_v_get_raddr( state->sock, &state->raddr );

		const sync4_msg_ready_t *msg = (sync4_msg_ready_t *)header;

		log_v_debug_P( PSTR("received ready") );

		state->vm_pages 	= msg->vm_pages;
		state->pixel_pages  = msg->pixel_pages;

		state->tries = SYNC4_MAX_TRIES;
		break;
	}

	// retries expired
	if( state->tries == 0 ){

		goto error;
	}

	sync_state = SYNC_STATE_SYNCING;

	while( sync_state == SYNC_STATE_SYNCING ){

		state->tries = SYNC4_MAX_TRIES;
		while( state->tries > 0 ){

			state->tries--;

			log_v_debug_P( PSTR("send data request %d rport: %d"), state->current_page, state->raddr.port );
			send_data_request( state->sock, &state->raddr, state->current_page );
				
			THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( state->sock ) < 0 ) && ( is_enabled() ) );

			// check if enabled
			if(!is_enabled()){

				goto done;
			}

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

			if( header->type != SYNC4_MSG_TYPE_DATA ){

				continue;
			}

			const sync4_msg_data_t *msg = (sync4_msg_data_t *)header;

			if( msg->page != state->current_page ){

				continue;
			}

			if( msg->total == 0 ){

				log_v_error_P( PSTR("bad total") );

				goto error;
			}

			if( state->total_pages == 0 ){

				state->total_pages = msg->total;
			}

			if( state->total_pages != msg->total ){

				log_v_error_P( PSTR("bad total") );

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

			file_t f = -1;
			uint8_t page = 0;

			if( msg->type == SYNC4_DATA_TYPE_VM ){

				page = msg->page;

				if( page == 0 ){

					fs_v_delete_fname_P( PSTR("_sync.f4b") );
				}

				f = fs_f_open_P( PSTR("_sync.f4b"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );
			}
			else if( msg->type == SYNC4_DATA_TYPE_PIXELS ){

				page = msg->page - state->vm_pages;

				if( page == 0 ){

					fs_v_delete_fname_P( PSTR("_pixel.f4b") );
				}

				f = fs_f_open_P( PSTR("_pixel.f4b"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );
			}
			else{

				log_v_error_P( PSTR("bad type") );

				goto error;
			}

			if( f < 0 ){

				log_v_error_P( PSTR("file error") );

				goto error;
			}


			uint16_t offset = page * SYNC4_MAX_DATA;

			fs_v_seek( f, offset );

			int16_t write_len = fs_i16_write( f, data, data_len );

			fs_f_close( f );

			if( write_len < 0 ){

				log_v_error_P( PSTR("file error: %d"), write_len );

				goto error;
			}

			state->current_page++;

			// check for completion
			if( state->current_page >= state->total_pages ){

				sync_state = SYNC_STATE_DATA;

				log_v_debug_P( PSTR("data sync done") );

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
	log_v_debug_P( PSTR("sync failed!") );
	sync4_v_reset();

done:

	sock_v_release( state->sock );

	log_v_debug_P( PSTR("data client stop") );
	
PT_END( pt );
}


PT_THREAD( sync4_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	static uint8_t sync_interval;
	sync_interval = 1;
	
	static uint8_t sync_timer;
	sync_timer = sync_interval;

	sync4_v_reset();

	TMR_WAIT( pt, 1000 );

	THREAD_WAIT_WHILE( pt, !is_enabled() );

	server_sock = sock_s_create( SOS_SOCK_DGRAM ); 

    if( server_sock < 0 ){

    	log_v_error_P( PSTR("alloc fail") );

    	THREAD_EXIT( pt );
    }

    sock_v_bind( server_sock, SYNC4_SERVER_PORT );
	
	thread_t_create( sync4_server_thread,
                    PSTR("sync4_server"),
                    0,
                    0 );

	TMR_WAIT( pt, 200 );
	
	while(1){

		// check if enabled
		if( !is_enabled() ){

			THREAD_RESTART( pt );
		}
		
		// check if leader
		if( sync4_b_is_leader() ){

			log_v_debug_P( PSTR("leader") );

			sync_state = SYNC_STATE_LEADER;

			THREAD_WAIT_WHILE( pt, sync4_b_is_leader() && is_enabled() );
		}
		// check if follower
		else if( sync4_b_is_follower() ){

			log_v_debug_P( PSTR("follower") );

			sync_state = SYNC_STATE_CONNECT;
			
			thread_t_create( 
					THREAD_CAST(data_client_thread),
                    PSTR("sync4_client"),
                    0,
                    sizeof(data_client_state_t) );

			THREAD_WAIT_WHILE( pt, 
				is_enabled() &&
				( sync_state == SYNC_STATE_CONNECT ) &&
				sync4_b_is_follower()
			);

			THREAD_WAIT_WHILE( pt, 
				is_enabled() &&
				( sync_state == SYNC_STATE_SYNCING ) &&
				sync4_b_is_follower()
			);

			if( !is_enabled() ){

				THREAD_RESTART( pt );
			}

			if( sync_state != SYNC_STATE_DATA ){

				log_v_error_P( PSTR("bad state") );

				goto restart;
			}

			// we have sync data at this point:
			// load it!
			vm4_v_unfreeze_vm( 0 );
			deserialize_pixels();

			while( ( sync_state >= SYNC_STATE_DATA ) && sync4_b_is_follower() ){

				thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );
				THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && is_enabled() );

				if(sync_timer > 0){

					sync_timer--;
				}

				if(sync_timer != 0){

					continue;
				}

				if(sync_interval < 8){

					sync_interval++;
				}

				sync_timer = sync_interval;

				if( !is_enabled() ){

					THREAD_RESTART( pt );
				}

				send_sync_request( server_sock );
			}

			THREAD_WAIT_WHILE( pt, 
				is_enabled() &&
				( sync_state == SYNC_STATE_SYNCED ) &&
				sync4_b_is_follower()
			);
		}

restart:
		TMR_WAIT( pt, 1000 );
	}

PT_END( pt );
}

void sync4_v_hold( void ){

	sync4_v_reset();
	hold = true;
}

void sync4_v_unhold( void ){

	hold = false;
}









