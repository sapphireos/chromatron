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
#define STATE_IDLE 		0
#define STATE_MASTER 	1
#define STATE_SLAVE 	2

static ip_addr_t master_ip;
static uint64_t master_uptime;


KV_SECTION_META kv_meta_t vm_sync_kv[] = {
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,   0, 			0, "gfx_sync_group" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,    0, "vm_sync_master_ip" },
};


PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) );

uint32_t vm_sync_u32_get_sync_group_hash( void ){

    char sync_group[32];
    if( kv_i8_get( __KV__gfx_sync_group, sync_group, sizeof(sync_group) ) < 0 ){

        return 0;
    }

    return hash_u32_string( sync_group );    
}


int8_t vm_sync_i8_request_frame_sync( void ){

	return wifi_i8_send_msg( WIFI_DATA_ID_REQUEST_FRAME_SYNC, 0, 0 );
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
	uint8_t buf[WIFI_MAX_SYNC_DATA];

	while( data_len > 0 ){

		int16_t read_len = fs_i16_read( f, buf, sizeof(buf) );

		if( read_len < 0 ){

			goto done;
		}

		wifi_i8_send_msg_blocking( WIFI_DATA_ID_VM_SYNC_DATA, buf, read_len );

		data_len -= read_len;
	}


	wifi_msg_vm_sync_done_t done;
	done.hash = get_file_hash();;

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

		wifi_msg_vm_frame_sync_t *msg = (wifi_msg_vm_frame_sync_t *)data;

		// delete file and recreate
		file_id_t8 id = fs_i8_get_file_id_P( PSTR("vm_sync") );

		if( id > 0 ){

			fs_i8_delete_id( id );
		}

		file_t f = fs_f_open_P( PSTR("vm_sync"), FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

		if( f < 0 ){

			return;
		}

		fs_i16_write( f, msg, sizeof(wifi_msg_vm_frame_sync_t) );

		f = fs_f_close( f ); 
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DATA ){

        wifi_msg_vm_sync_data_t *msg = (wifi_msg_vm_sync_data_t *)data;
        data += sizeof(wifi_msg_vm_sync_data_t);

        log_v_debug_P( PSTR("sync offset: %u"), msg->offset );

        write_to_sync_file( msg->offset, data, len - sizeof(wifi_msg_vm_sync_data_t) );
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DONE ){

    	wifi_msg_vm_sync_done_t *msg = (wifi_msg_vm_sync_done_t *)data;

    	uint32_t hash = get_file_hash();

		if( hash == msg->hash ){

			log_v_debug_P( PSTR("Verified") );

			load_frame_data();
		}
    }
}




PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static socket_t sock;
	sock = sock_s_create( SOCK_DGRAM );

	if( sock < 0 ){

		THREAD_EXIT( pt );
	}

	sock_v_bind( sock, SYNC_SERVER_PORT );
    
    TMR_WAIT( pt, 4000 );

    vm_sync_i8_request_frame_sync();


    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, vm_sync_u32_get_sync_group_hash() == 0 );

    	THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );
	
		// check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

	        uint32_t *magic = sock_vp_get_data( sock );

	        if( *magic != SYNC_PROTOCOL_MAGIC ){

	            continue;
	        }

	        uint8_t *version = (uint8_t *)(magic + 1);

	        if( *version != SYNC_PROTOCOL_VERSION ){

	            continue;
	        }

	        uint8_t *type = version + 1;

	        if( *type == VM_SYNC_MSG_MASTER ){

	        	vm_sync_msg_master_t *msg = (vm_sync_msg_master_t *)magic;

	        }
	        else if( *type == VM_SYNC_MSG_GET_TIMESTAMP ){

	        	vm_sync_msg_get_ts_t *msg = (vm_sync_msg_get_ts_t *)magic;
	        }
	        else if( *type == VM_SYNC_MSG_TIMESTAMP ){

	        	vm_sync_msg_ts_t *msg = (vm_sync_msg_ts_t *)magic;
	        }
	        else if( *type == VM_SYNC_MSG_GET_SYNC_DATA ){

	        	vm_sync_msg_get_sync_data_t *msg = (vm_sync_msg_get_sync_data_t *)magic;
	        }
	        else if( *type == VM_SYNC_MSG_SYNC_INIT ){

	        	vm_sync_msg_sync_init_t *msg = (vm_sync_msg_sync_init_t *)magic;
	        }
	        else if( *type == VM_SYNC_MSG_SYNC_DATA ){

	        	vm_sync_msg_sync_data_t *msg = (vm_sync_msg_sync_data_t *)magic;
	        }
	        else if( *type == VM_SYNC_MSG_SYNC_DONE ){

				vm_sync_msg_sync_done_t *msg = (vm_sync_msg_sync_done_t *)magic;	        	
	        }
    	}


    	if( sync_state == STATE_IDLE ){


    	}
    	else if( sync_state == STATE_MASTER ){

    		
    	}
    	else if( sync_state == STATE_SLAVE ){

    		
    	}

    	// prevent runaway thread
    	THREAD_YIELD( pt );
    }


PT_END( pt );
}



void vm_sync_v_init( void ){

    thread_t_create( vm_sync_thread,
                    PSTR("vm_sync"),
                    0,
                    0 );    
}


#endif
