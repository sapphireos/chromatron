 // <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "sapphire.h"

#ifdef ENABLE_TIME_SYNC

#include "vm_sync.h"
#include "hash.h"
#include "esp8266.h"
#include "vm_wifi_cmd.h"


static uint8_t sync_state;
#define STATE_IDLE 			0
#define STATE_MASTER 		1
#define STATE_SLAVE 		2
#define STATE_SLAVE_SYNC 	3

static uint8_t esp_sync_state;
#define ESP_SYNC_IDLE 			0
#define ESP_SYNC_DOWNLOADING  	1
#define ESP_SYNC_READY  		2

static ip_addr_t master_ip;
static uint64_t master_uptime;

static uint32_t sync_group_hash;

static socket_t sock;

typedef struct{
	sock_addr_t raddr;
	file_t f;
	socket_t sock;
	uint16_t data_len;
	uint16_t offset;
} data_sender_state_t;


int8_t vmsync_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

    	if( hash == __KV__gfx_sync_group ){

		    sync_group_hash = hash_u32_string( data );    
    	}
    }

    return 0;
}


KV_SECTION_META kv_meta_t vm_sync_kv[] = {
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,   0, vmsync_i8_kv_handler, 	"gfx_sync_group" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,    	0, 		"vm_sync_master_ip" },
};


PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) );
PT_THREAD( vm_data_sender_thread( pt_t *pt, data_sender_state_t *state ) );

uint32_t vm_sync_u32_get_sync_group_hash( void ){

	return sync_group_hash;
}


int8_t vm_sync_i8_request_frame_sync( void ){

	esp_sync_state = ESP_SYNC_DOWNLOADING;

	uint32_t vm_id = 0;

	return wifi_i8_send_msg( WIFI_DATA_ID_REQUEST_FRAME_SYNC, (uint8_t *)&vm_id, sizeof(vm_id) );
}

static void init_sync_file( wifi_msg_vm_frame_sync_t *sync ){

	// delete file and recreate
	file_id_t8 id = fs_i8_get_file_id_P( PSTR("vm_sync") );

	if( id > 0 ){

		fs_i8_delete_id( id );
	}

	file_t f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

	if( f < 0 ){

		return;
	}

	fs_i16_write( f, sync, sizeof(wifi_msg_vm_frame_sync_t) );

	f = fs_f_close( f ); 
}

static void write_to_sync_file( uint16_t offset, uint8_t *data, uint16_t len ){

	file_t f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_WRITE_APPEND );

	if( f < 0 ){

		return;
	}

	// check offset
	if( ( fs_i32_get_size( f ) - sizeof(wifi_msg_vm_frame_sync_t) ) != offset ){

		goto done;
	}

	fs_i16_write( f, data, len );

done:
	f = fs_f_close( f ); 
}

uint32_t get_file_hash( void ){

	// check file hash
	uint32_t hash = hash_u32_start();

	file_t f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_READ_ONLY );

	if( f < 0 ){

		return 0;
	}

	fs_v_seek( f, sizeof(wifi_msg_vm_frame_sync_t) );

	uint8_t buf[64];
	int16_t read_len = 0;

	do{

		read_len = fs_i16_read( f, buf, sizeof(buf) );

		hash = hash_u32_partial( hash, buf, read_len );

	} while( read_len == sizeof(buf) );

	f = fs_f_close( f ); 

	return hash;
}

void load_frame_data( void ){

	file_t f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_READ_ONLY );

	if( f < 0 ){

		return;
	}

	wifi_msg_vm_frame_sync_t sync;
	fs_i16_read( f, &sync, sizeof(sync) );

	// send sync
	wifi_i8_send_msg_blocking( WIFI_DATA_ID_VM_FRAME_SYNC, (uint8_t *)&sync, sizeof(sync) );

	uint16_t data_len = sync.data_len;
	uint8_t buf[WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_sync_data_t)];
	wifi_msg_vm_sync_data_t *msg = (wifi_msg_vm_sync_data_t *)buf;
    uint8_t *data = (uint8_t *)( msg + 1 );
    msg->offset = 0;

	while( data_len > 0 ){

		int16_t read_len = fs_i16_read( f, data, WIFI_MAX_SYNC_DATA );

		if( read_len < 0 ){

			goto done;
		}

		wifi_i8_send_msg_blocking( WIFI_DATA_ID_VM_SYNC_DATA, buf, read_len );

		msg->offset += read_len;
		data_len 	-= read_len;
	}


	wifi_msg_vm_sync_done_t done;
	done.hash = get_file_hash();

	// send done
	wifi_i8_send_msg_blocking( WIFI_DATA_ID_VM_SYNC_DONE, (uint8_t *)&done, sizeof(done) );

done:
	f = fs_f_close( f );
}

void vm_sync_v_process_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

	if( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ){

		if( len != sizeof(wifi_msg_vm_frame_sync_t) ){

			return;
		}

		// log_v_debug_P( PSTR("sync") );

		wifi_msg_vm_frame_sync_t *msg = (wifi_msg_vm_frame_sync_t *)data;

		init_sync_file( msg );
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DATA ){

        wifi_msg_vm_sync_data_t *msg = (wifi_msg_vm_sync_data_t *)data;
        data += sizeof(wifi_msg_vm_sync_data_t);

        // log_v_debug_P( PSTR("sync offset: %u len %u"), msg->offset, len );

        write_to_sync_file( msg->offset, data, len - sizeof(wifi_msg_vm_sync_data_t) );
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DONE ){

    	wifi_msg_vm_sync_done_t *msg = (wifi_msg_vm_sync_done_t *)data;

    	// log_v_debug_P( PSTR("done") );

    	uint32_t hash = get_file_hash();

		if( hash == msg->hash ){

			// log_v_debug_P( PSTR("Verified %lx"), hash );

			// load_frame_data();

			esp_sync_state = ESP_SYNC_READY;
		}
    }
}


static void send_master( void ){

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    vm_sync_msg_master_t msg;
    msg.header.magic           = SYNC_PROTOCOL_MAGIC;
    msg.header.version         = SYNC_PROTOCOL_VERSION;
    msg.header.type            = VM_SYNC_MSG_MASTER;
    msg.header.flags 		   = 0;
    msg.header.sync_group_hash = sync_group_hash;

    msg.uptime = master_uptime;
    
    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}


static void send_request_sync( void ){

    // set up address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = master_ip;

    vm_sync_msg_get_sync_data_t msg;
    msg.header.magic           = SYNC_PROTOCOL_MAGIC;
    msg.header.version         = SYNC_PROTOCOL_VERSION;
    msg.header.type            = VM_SYNC_MSG_GET_SYNC_DATA;
    msg.header.flags 		   = 0;
    msg.header.sync_group_hash = sync_group_hash;
    
    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}

static void send_request_ts( void ){

    // set up address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = master_ip;

    vm_sync_msg_get_ts_t msg;
    msg.header.magic           = SYNC_PROTOCOL_MAGIC;
    msg.header.version         = SYNC_PROTOCOL_VERSION;
    msg.header.type            = VM_SYNC_MSG_GET_TIMESTAMP;
    msg.header.flags 		   = 0;
    msg.header.sync_group_hash = sync_group_hash;
    
    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}


PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static uint32_t last_run;
	static uint32_t last_sync;

	sock = sock_s_create( SOCK_DGRAM );

	if( sock < 0 ){

		THREAD_EXIT( pt );
	}

	sock_v_bind( sock, SYNC_SERVER_PORT );
	sock_v_set_timeout( sock, 8 );
    

    THREAD_WAIT_WHILE( pt, sync_group_hash == 0 );


    last_run = tmr_u32_get_system_time_ms();

    // TMR_WAIT( pt, 8000 + ( rnd_u16_get_int() >> 5 ) );

    // vm_sync_i8_request_frame_sync();


    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, sync_group_hash == 0 );

		if( tmr_u32_elapsed_time_ms( last_run ) > 7000 ){

			if( sync_state == STATE_IDLE ){

				// start out as master
			    sync_state = STATE_MASTER;

			    log_v_debug_P( PSTR("we are sync master") );
	    	}
	    	else if( sync_state == STATE_MASTER ){

	    		master_uptime = tmr_u64_get_system_time_us();

	    		send_master();

	    		// vm_sync_i8_request_frame_sync();
	    	}
	    	else if( sync_state >= STATE_SLAVE ){

	    		// check for master timeout
	    		if( tmr_u32_elapsed_time_ms( last_sync ) > SYNC_MASTER_TIMEOUT ){

	    			sync_state = STATE_IDLE;
	    			master_ip = ip_a_addr(0,0,0,0);
	    			master_uptime = 0;
	    		}
	    		else if( sync_state == STATE_SLAVE ){

	    			// not synced
	    			send_request_sync();
	    		}
	    		else if( sync_state == STATE_SLAVE_SYNC ){

	    			// synced
	    			send_request_ts();
	    		}
	    	}

	    	last_run = tmr_u32_get_system_time_ms();
		}
	

    	// prevent runaway thread
    	// THREAD_YIELD( pt );
    	TMR_WAIT( pt, 100 );

    	THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );
	
		// check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

	        vm_sync_msg_header_t *header = sock_vp_get_data( sock );

	        if( header->magic != SYNC_PROTOCOL_MAGIC ){

	            continue;
	        }

	        
	        if( header->version != SYNC_PROTOCOL_VERSION ){

	            continue;
	        }

	        // check sync group
	        if( header->sync_group_hash != sync_group_hash ){

	        	continue;
	        }

	        sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );
	        

	        if( header->type == VM_SYNC_MSG_MASTER ){

	        	vm_sync_msg_master_t *msg = (vm_sync_msg_master_t *)header;

				// check if this message is from the current master
				if( ip_b_addr_compare( raddr.ipaddr, master_ip ) ){

					continue;
				}					        	

	        	// check if we are master
	        	if( sync_state == STATE_MASTER ){

	        		// update uptime
		    		master_uptime = tmr_u64_get_system_time_us();
		    	}

	        	// master selection
	        	if( msg->uptime > master_uptime ){

	        		// ok, this master is better than ours
	        		master_ip = raddr.ipaddr;
                    master_uptime = msg->uptime;

                    sync_state = STATE_SLAVE;

                    log_v_debug_P( PSTR("assigning sync master: %d.%d.%d.%d"), 
                        master_ip.ip3, 
                        master_ip.ip2, 
                        master_ip.ip1, 
                        master_ip.ip0 );

                    last_sync = tmr_u32_get_system_time_ms();
	        	}
	        }
	        else if( header->type == VM_SYNC_MSG_GET_TIMESTAMP ){

	        	if( sync_state != STATE_MASTER ){

	        		continue;
	        	}

	        	// vm_sync_msg_get_ts_t *msg = (vm_sync_msg_get_ts_t *)header;
	        }
	        else if( header->type == VM_SYNC_MSG_TIMESTAMP ){

	        	if( sync_state != STATE_SLAVE ){

	        		continue;
	        	}

	        	// vm_sync_msg_ts_t *msg = (vm_sync_msg_ts_t *)header;
	        }
	        else if( header->type == VM_SYNC_MSG_GET_SYNC_DATA ){

	        	if( sync_state != STATE_MASTER ){

	        		continue;
	        	}

	        	if( esp_sync_state != ESP_SYNC_IDLE ){

	        		// we're only syncing one at a time for now

	        		continue;
	        	}

	        	// vm_sync_msg_get_sync_data_t *msg = (vm_sync_msg_get_sync_data_t *)header;

	        	data_sender_state_t sender_state;
	        	memset( &sender_state, 0, sizeof(sender_state) );
	        	sender_state.raddr = raddr;

        	    thread_t_create( THREAD_CAST(vm_data_sender_thread),
                    			 PSTR("vm_sync_data_sender"),
                    			 &sender_state,
                    			 sizeof(sender_state) );    

	        }
	        else if( header->type == VM_SYNC_MSG_SYNC_INIT ){

	        	if( sync_state != STATE_SLAVE ){

	        		continue;
	        	}

	        	vm_sync_msg_sync_init_t *msg = (vm_sync_msg_sync_init_t *)header;

	        	init_sync_file( &msg->sync );
	        }
	        else if( header->type == VM_SYNC_MSG_SYNC_DATA ){

	        	if( sync_state != STATE_SLAVE ){

	        		continue;
	        	}

	        	vm_sync_msg_sync_data_t *msg = (vm_sync_msg_sync_data_t *)header;
	        	uint8_t *data = (uint8_t *)( msg + 1 );

	        	write_to_sync_file( msg->offset, data, sock_i16_get_bytes_read( sock ) - sizeof(vm_sync_msg_sync_data_t) );
	        }
	        else if( header->type == VM_SYNC_MSG_SYNC_DONE ){

	        	if( sync_state != STATE_SLAVE ){

	        		continue;
	        	}

				vm_sync_msg_sync_done_t *msg = (vm_sync_msg_sync_done_t *)header;

				if( get_file_hash() == msg->hash ){

					log_v_debug_P( PSTR("slave hash verified!") );

					sync_state = STATE_SLAVE_SYNC;

					load_frame_data();
				}
	        }
    	}
    }


PT_END( pt );
}


PT_THREAD( vm_data_sender_thread( pt_t *pt, data_sender_state_t *state ) )
{
PT_BEGIN( pt );
	
	state->sock = sock_s_create( SOCK_DGRAM );

	if( sock < 0 ){

		THREAD_EXIT( pt );
	}

	vm_sync_i8_request_frame_sync();

	THREAD_WAIT_WHILE( pt, esp_sync_state == ESP_SYNC_READY );


	state->f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_READ_ONLY );

	if( state->f < 0 ){

		log_v_debug_P( PSTR("sync file not ready") );

		sock_v_release( state->sock );

		THREAD_EXIT( pt );
	}

	TMR_WAIT( pt, 50 );

	state->offset = 0;
	
	vm_sync_msg_sync_init_t sync;
	sync.header.magic           = SYNC_PROTOCOL_MAGIC;
    sync.header.version         = SYNC_PROTOCOL_VERSION;
    sync.header.type            = VM_SYNC_MSG_SYNC_INIT;
    sync.header.flags 		    = 0;
    sync.header.sync_group_hash = sync_group_hash;

    fs_i16_read( state->f, &sync.sync, sizeof(sync.sync) );

    state->data_len = sync.sync.data_len;

    sock_i16_sendto( state->sock, (uint8_t *)&sync, sizeof(sync), &state->raddr );

    TMR_WAIT( pt, 50 );

    while( state->data_len > 0 ){

    	uint16_t copy_len = state->data_len;
    	if( copy_len > SYNC_DATA_MAX ){

    		copy_len = SYNC_DATA_MAX;
    	}

    	mem_handle_t h = mem2_h_alloc( sizeof(vm_sync_msg_sync_data_t) + copy_len );

    	if( h < 0 ){

    		goto done;
    	}

    	vm_sync_msg_sync_data_t *msg = mem2_vp_get_ptr( h );
    	uint8_t *dest = (uint8_t *)( msg + 1 );

		fs_i16_read( state->f, dest, copy_len );

    	msg->header.magic           = SYNC_PROTOCOL_MAGIC;
	    msg->header.version         = SYNC_PROTOCOL_VERSION;
	    msg->header.type            = VM_SYNC_MSG_SYNC_DATA;
	    msg->header.flags 		    = 0;
	    msg->header.sync_group_hash = sync_group_hash;

	    msg->offset = state->offset;

	    sock_i16_sendto_m( state->sock, h, &state->raddr );

		state->offset 		+= copy_len;
		state->data_len 	-= copy_len;

		TMR_WAIT( pt, 50 );
    }

    // send done message
    vm_sync_msg_sync_done_t done;
    done.header.magic           = SYNC_PROTOCOL_MAGIC;
    done.header.version         = SYNC_PROTOCOL_VERSION;
    done.header.type            = VM_SYNC_MSG_SYNC_DONE;
    done.header.flags 		    = 0;
    done.header.sync_group_hash = sync_group_hash;

    done.hash = get_file_hash();

    sock_i16_sendto( state->sock, (uint8_t *)&done, sizeof(done), &state->raddr );

done:
	fs_f_close( state->f );
	sock_v_release( state->sock );

	esp_sync_state = ESP_SYNC_IDLE;

PT_END( pt );
}



void vm_sync_v_init( void ){

	// init sync group hash
	char buf[32];
	memset( buf, 0, sizeof(buf) );
	kv_i8_get( __KV__gfx_sync_group, buf, sizeof(buf) );

	sync_group_hash = hash_u32_string( buf );    


    thread_t_create( vm_sync_thread,
                    PSTR("vm_sync"),
                    0,
                    0 );    
}


#endif
