// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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



#define SYNC_DEBUG


#include "timesync.h"
#include "vm_sync.h"
#include "vm.h"
#include "hash.h"
#include "esp8266.h"
#include "vm_wifi_cmd.h"
#include "graphics.h"
#include "config.h"

#include "logging.h"

static uint32_t sync_group_hash;
static socket_t sock = -1;

static uint8_t sync_state;
#define STATE_IDLE 			0
#define STATE_MASTER 		1
#define STATE_SLAVE 		2
#define STATE_SLAVE_SYNC 	3


static ip_addr_t master_ip;
static uint64_t master_uptime;

static uint16_t slave_offset;
static uint16_t slave_frame;
static ip_addr_t pending_slave;
static uint32_t slave_net_time;


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
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,   0, vmsync_i8_kv_handler,   "gfx_sync_group" },
    { SAPPHIRE_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,        0,      "vm_sync_master_ip" },
};


PT_THREAD( vm_sync_server_thread( pt_t *pt, void *state ) );
PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) );


#ifdef SYNC_DEBUG
PT_THREAD( vm_sync_debug_thread( pt_t *pt, void *state ) );

static void debug_strobe( void ){
    io_v_digital_write( IO_PIN_PWM_0, TRUE );
    _delay_us( 10 );
    io_v_digital_write( IO_PIN_PWM_0, FALSE );
} 

PT_THREAD( vm_sync_debug_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

        THREAD_WAIT_WHILE( pt, !time_b_is_sync() );

        uint32_t net_time = time_u32_get_network_time();
        
        TMR_WAIT( pt, 1000 - ( net_time % 1000 ) );

        debug_strobe();
    }

PT_END( pt );
}


#endif

void vm_sync_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    #ifdef SYNC_DEBUG
    io_v_set_mode( IO_PIN_PWM_0, IO_MODE_OUTPUT );

    thread_t_create( vm_sync_debug_thread,
                    PSTR("vm_sync_debug"),
                    0,
                    0 );    
    #endif

    // check if time sync is enabled
    // if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

    //     return;
    // }

	// init sync group hash
	char buf[32];
	memset( buf, 0, sizeof(buf) );
	kv_i8_get( __KV__gfx_sync_group, buf, sizeof(buf) );

	sync_group_hash = hash_u32_string( buf );    

	sock = sock_s_create( SOCK_DGRAM );	

	sock_v_bind( sock, SYNC_SERVER_PORT );
	sock_v_set_timeout( sock, 32 );

    thread_t_create( vm_sync_server_thread,
                    PSTR("vm_sync_server"),
                    0,
                    0 );    

    thread_t_create( vm_sync_thread,
                    PSTR("vm_sync"),
                    0,
                    0 );    
}

bool vm_sync_b_is_master( void ){

    return sync_state == STATE_MASTER;
}

bool vm_sync_b_is_slave( void ){
 
    return sync_state == STATE_SLAVE;
}

bool vm_sync_b_is_slave_synced( void ){

    return sync_state == STATE_SLAVE_SYNC;
}

uint32_t vm_sync_u32_get_sync_group_hash( void ){

	return sync_group_hash;
}

static bool vm_sync_wait( void ){

	return ( sync_group_hash == 0 ) || ( !vm_b_is_vm_running( 0 ) ) || ( !time_b_is_sync() );
    // return ( sync_group_hash == 0 ) || ( !vm_b_is_vm_running( 0 ) );
}


static int8_t get_frame_sync( wifi_msg_vm_frame_sync_t *msg ){

	if( wifi_i8_send_msg( WIFI_DATA_ID_VM_FRAME_SYNC, 0, 0 ) < 0 ){

        return -1;
    }

    if( wifi_i8_receive_msg( WIFI_DATA_ID_VM_FRAME_SYNC, (uint8_t *)msg, sizeof(wifi_msg_vm_frame_sync_t), 0 ) < 0 ){

        return -2;
    }

    return 0;
}


static int8_t set_frame_sync( wifi_msg_vm_frame_sync_t *msg ){

    if( wifi_i8_send_msg( WIFI_DATA_ID_VM_SET_FRAME_SYNC, (uint8_t *)msg, sizeof(wifi_msg_vm_frame_sync_t) ) < 0 ){

        return -1;
    }

    if( wifi_i8_receive_msg( WIFI_DATA_ID_VM_SET_FRAME_SYNC, 0, 0, 0 ) < 0 ){

        return -2;
    }

    return 0;
}


static int16_t get_frame_data( uint16_t offset, wifi_msg_vm_sync_data_t *msg ){

    msg->offset = offset;

    if( wifi_i8_send_msg( WIFI_DATA_ID_VM_SYNC_DATA, (uint8_t *)msg, sizeof(wifi_msg_vm_sync_data_t) ) < 0 ){

        return -1;
    }

    uint16_t bytes_read;

    if( wifi_i8_receive_msg( WIFI_DATA_ID_VM_SYNC_DATA, (uint8_t *)msg, WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_sync_data_t), &bytes_read ) < 0 ){

        return -2;
    }

    return bytes_read;
}


static int16_t set_frame_data( wifi_msg_vm_sync_data_t *msg, uint16_t len ){

    if( wifi_i8_send_msg( WIFI_DATA_ID_VM_SET_SYNC_DATA, (uint8_t *)msg, sizeof(wifi_msg_vm_sync_data_t) + len ) < 0 ){

        return -1;
    }

    uint16_t bytes_read;

    if( wifi_i8_receive_msg( WIFI_DATA_ID_VM_SET_SYNC_DATA, 0, 0, 0 ) < 0 ){

        return -2;
    }

    return bytes_read;
}

static void send_sync_0( wifi_msg_vm_frame_sync_t *sync, sock_addr_t *raddr ){

    vm_sync_msg_sync_0_t msg;
    msg.header.magic            = SYNC_PROTOCOL_MAGIC;
    msg.header.version          = SYNC_PROTOCOL_VERSION;
    msg.header.type             = VM_SYNC_MSG_SYNC_0;
    msg.header.flags            = 0;
    msg.header.sync_group_hash  = sync_group_hash;

    msg.uptime              = tmr_u64_get_system_time_us();
    msg.program_name_hash   = sync->program_name_hash;
    msg.frame_number        = sync->frame_number;
    msg.data_len            = sync->data_len;
    msg.rng_seed            = sync->rng_seed;
    msg.net_time            = time_u32_get_network_time();
    // msg.timestamp           =tmr_u32_get_system_time_ms();

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );   
}

static void send_sync_n( uint16_t offset, uint16_t frame_number, uint8_t *data, uint16_t len, sock_addr_t *raddr ){

    // note on large VM programs this is going to cause us to run out of 
    // memory
    mem_handle_t h = mem2_h_alloc( len + sizeof(vm_sync_msg_sync_n_t) );

    if( h < 0 ){

        return;
    }

    vm_sync_msg_sync_n_t *msg = mem2_vp_get_ptr( h );
    uint8_t *msg_data = (uint8_t *)( msg + 1 );
    memcpy( msg_data, data, len );

    msg->header.magic            = SYNC_PROTOCOL_MAGIC;
    msg->header.version          = SYNC_PROTOCOL_VERSION;
    msg->header.type             = VM_SYNC_MSG_SYNC_N;
    msg->header.flags            = 0;
    msg->header.sync_group_hash  = sync_group_hash;

    msg->offset         = offset;
    msg->frame_number   = frame_number;

    raddr->port = SYNC_SERVER_PORT;
    
    sock_i16_sendto_m( sock, h, raddr );   
}

static void send_request( void ){

    vm_sync_msg_sync_req_t msg;
    msg.header.magic            = SYNC_PROTOCOL_MAGIC;
    msg.header.version          = SYNC_PROTOCOL_VERSION;
    msg.header.type             = VM_SYNC_MSG_SYNC_REQ;
    msg.header.flags            = 0;
    msg.header.sync_group_hash  = sync_group_hash;

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = master_ip;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );   
}

static void send_shutdown( void ){

    vm_sync_msg_shutdown_t msg;
    msg.header.magic            = SYNC_PROTOCOL_MAGIC;
    msg.header.version          = SYNC_PROTOCOL_VERSION;
    msg.header.type             = VM_SYNC_MSG_SHUTDOWN;
    msg.header.flags            = 0;
    msg.header.sync_group_hash  = sync_group_hash;

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );   
}

static void send_sync_to_slave( sock_addr_t *raddr ){

    if( sync_state != STATE_MASTER ){

        return;
    }

    // thread_v_signal( SYNC_SIGNAL );

    wifi_msg_vm_frame_sync_t sync;
    if( get_frame_sync( &sync ) < 0 ){

        return;
    }

    send_sync_0( &sync, raddr );

    // log_v_debug_P( PSTR("sync frame: %u"), sync.frame_number );

    uint8_t buf[WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_sync_data_t)];
    uint8_t *data = &buf[sizeof(wifi_msg_vm_sync_data_t)];

    for( uint16_t i = 0; i < sync.data_len; ){

        int16_t bytes_read = get_frame_data( i, (wifi_msg_vm_sync_data_t *)buf );
        if( bytes_read < 0 ){

            return;
        }

        send_sync_n( i, sync.frame_number, data, bytes_read, raddr );

        i += WIFI_MAX_SYNC_DATA;
    }    
}

void vm_sync_v_trigger( void ){

    if( sync_state != STATE_MASTER ){

        return;
    }

    // thread_v_signal( SYNC_SIGNAL );

    wifi_msg_vm_frame_sync_t sync;
    if( get_frame_sync( &sync ) < 0 ){

        return;
    }

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = SYNC_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    send_sync_0( &sync, &raddr );

    // log_v_debug_P( PSTR("sync frame: %u"), sync.frame_number );
}


void vm_sync_v_frame_trigger( void ){

    if( sync_state != STATE_MASTER ){

        return;
    }

    if( !ip_b_is_zeroes( pending_slave ) ){

        sock_addr_t raddr;
        raddr.port = SYNC_SERVER_PORT;
        raddr.ipaddr = pending_slave;

        send_sync_to_slave( &raddr );

        pending_slave = ip_a_addr(0,0,0,0);
    }

}

PT_THREAD( vm_sync_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && ( !sys_b_shutdown() ) );
        // uint32_t now = time_u32_get_network_time();
        // uint32_t now = tmr_u32_get_system_time_ms();

    	// check if shutting down
    	if( sys_b_shutdown() ){

    		// if we're a master, signal that we are shutting down
    		if( sync_state == STATE_MASTER ){

	    		send_shutdown();
	    		TMR_WAIT( pt, 200 );
	    		send_shutdown();
	    		TMR_WAIT( pt, 200 );
	    		send_shutdown();
	    	}

    		THREAD_EXIT( pt );
    	}

		// check if data received
        int16_t sock_data_len = sock_i16_get_bytes_read( sock );
        if( sock_data_len <= 0 ){

        	// socket timeout

        	if( ( sync_state != STATE_MASTER ) && ( sync_state != STATE_IDLE ) ){

                log_v_debug_P( PSTR("vm sync timed out, resetting state") );

                sync_state = STATE_IDLE;
            }

        	continue;
        }

        if( vm_sync_wait() ){

            continue;
        }

        vm_sync_msg_header_t *header = sock_vp_get_data( sock );

        if( header->magic != SYNC_PROTOCOL_MAGIC ){

        	continue;
        }

        if( header->version != SYNC_PROTOCOL_VERSION ){

        	continue;
        }

        if( header->sync_group_hash != sync_group_hash ){

        	continue;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( header->type == VM_SYNC_MSG_SYNC_0 ){

        	vm_sync_msg_sync_0_t *msg = (vm_sync_msg_sync_0_t *)header;
        	
        	if( sync_state == STATE_IDLE ){

        		master_ip = raddr.ipaddr;
                master_uptime = msg->uptime;

        		log_v_debug_P( PSTR("assigning vm sync master: %d.%d.%d.%d"), 
                        master_ip.ip3, 
                        master_ip.ip2, 
                        master_ip.ip1, 
                        master_ip.ip0 );

        		sync_state = STATE_SLAVE;

                // done processing
                continue;
        	}
            else if( sync_state == STATE_SLAVE ){

                // slave, not synced 
                slave_offset    = 0;
                slave_frame     = msg->frame_number;
                // slave_net_time  = msg->net_time;
                // slave_net_time  = msg->uptime / 1000;

                log_v_debug_P( PSTR("starting slave sync, frame: %u"), msg->frame_number );

                wifi_msg_vm_frame_sync_t sync;
                sync.program_name_hash  = msg->program_name_hash;
                sync.frame_number       = msg->frame_number;
                sync.data_len           = msg->data_len;
                sync.rng_seed           = msg->rng_seed;

                set_frame_sync( &sync );                

                continue;
            }

        	uint64_t temp_master_uptime = master_uptime;

        	// are we a master?
        	if( sync_state == STATE_MASTER ){

        		// use our current uptime
        		temp_master_uptime = tmr_u64_get_system_time_us();
        	}

        	// compare uptimes - longest uptime wins election
        	if( msg->uptime > temp_master_uptime ){

                master_uptime = msg->uptime;

                if( !ip_b_addr_compare( master_ip, raddr.ipaddr ) ){

        		    // set new master
                    master_ip = raddr.ipaddr;
                    

            		log_v_debug_P( PSTR("assigning NEW vm sync master: %d.%d.%d.%d"), 
                        master_ip.ip3, 
                        master_ip.ip2, 
                        master_ip.ip1, 
                        master_ip.ip0 );      

        		    sync_state = STATE_SLAVE;

                    // slave, not synced 
                    slave_offset    = 0;
                    slave_frame     = msg->frame_number;
                    slave_net_time  = msg->net_time;
                }
        	}

            if( sync_state == STATE_SLAVE_SYNC ){

                // uint32_t rate = slave_frame - msg->frame_number;
                // slave_frame = msg->frame_number;

                // log_v_debug_P( PSTR("updating slave sync, frame: %u net: %lu"), msg->frame_number, now );
                                
                slave_frame     = msg->frame_number;
                slave_net_time  = msg->net_time;
                gfx_v_set_sync( slave_frame, slave_net_time );
            }
        }
        else if( header->type == VM_SYNC_MSG_SYNC_N ){

            if( sync_state == STATE_SLAVE ){

            	vm_sync_msg_sync_n_t *msg = (vm_sync_msg_sync_n_t *)header;

                int16_t data_len = sock_data_len - sizeof(vm_sync_msg_sync_n_t);

                if( data_len > (int16_t)WIFI_MAX_SYNC_DATA ){

                    log_v_debug_P( PSTR("invalid len") );
                    continue;
                }
            	log_v_debug_P( PSTR("received sync offset: %u frame: %u len: %d"), msg->offset, msg->frame_number, data_len );

                uint8_t *msg_data = (uint8_t *)( msg + 1 );

                uint8_t buf[WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_sync_data_t)];
                wifi_msg_vm_sync_data_t *sync   = (wifi_msg_vm_sync_data_t *)buf;
                
                sync->offset = msg->offset;

                memcpy( &buf[sizeof(wifi_msg_vm_sync_data_t)], msg_data, data_len );

                set_frame_data( sync, data_len );

                // check if sync is finished
                if( msg->offset == slave_offset ){

                    slave_offset += data_len;

                    if( data_len < (int16_t)WIFI_MAX_SYNC_DATA ){

                        sync_state = STATE_SLAVE_SYNC;

                        log_v_debug_P( PSTR("finished sync data") );
                        gfx_v_set_sync0( slave_frame, slave_net_time );
                    }
                }
            }
        }
        else if( header->type == VM_SYNC_MSG_SHUTDOWN ){

            // check if message is from master
            if( ip_b_addr_compare( master_ip, raddr.ipaddr ) ){

                if( sync_state != STATE_IDLE ){

                    log_v_debug_P( PSTR("sync master shutting down") );
                    sync_state = STATE_IDLE;
                }
            }
        }
        else if( header->type == VM_SYNC_MSG_SYNC_REQ ){

        	if( sync_state != STATE_MASTER ){

        		// this message can only be processed by a master
        		continue;
        	}

        	// vm_sync_msg_sync_req_t *msg = (vm_sync_msg_sync_req_t *)header;

        	// log_v_debug_P( PSTR("sync requested") );

            if( ip_b_is_zeroes( pending_slave ) ){

                pending_slave = raddr.ipaddr;
            }
        }
    }

PT_END( pt );
}

PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, vm_sync_wait() );

    	// if( sync_state == STATE_IDLE ){

            // random delay, see if other masters show up
            // TMR_WAIT( pt, 4000 + ( rnd_u16_get_int() >> 4 ) );
    	// }

        // no masters, elect ourselves
        if( sync_state == STATE_IDLE ){

            sync_state = STATE_MASTER;
            master_uptime = 0;

            log_v_debug_P( PSTR("we are sync master") );
        }

    	while( ( sync_state == STATE_MASTER ) && !vm_sync_wait() ){

    		if( sys_b_shutdown() ){

    			THREAD_EXIT( pt );
    		}

            TMR_WAIT( pt, 500 );

//             THREAD_WAIT_SIGNAL( pt, SYNC_SIGNAL );

//             wifi_msg_vm_frame_sync_t sync;
//             if( get_frame_sync( &sync ) < 0 ){

//                 goto master_error;
//             }

//             send_sync_0( &sync );

//             log_v_debug_P( PSTR("sync frame: %u"), sync.frame_number );

//             uint8_t buf[WIFI_MAX_SYNC_DATA + sizeof(wifi_msg_vm_sync_data_t)];
//             uint8_t *data = &buf[sizeof(wifi_msg_vm_sync_data_t)];

//             for( uint16_t i = 0; i < sync.data_len; ){

//                 int16_t bytes_read = get_frame_data( i, (wifi_msg_vm_sync_data_t *)buf );
//                 if( bytes_read < 0 ){

//                     goto master_error;
//                 }

//                 send_sync_n( i, sync.frame_number, data, bytes_read );

//                 i += WIFI_MAX_SYNC_DATA;
//             }

// master_error:
//     		TMR_WAIT( pt, 8 * 1000 );
//             thread_v_clear_signal( SYNC_SIGNAL );
    	}

        while( ( sync_state == STATE_SLAVE ) && !vm_sync_wait() ){

            // random wait a bit so we don't all transmit to master at the same time
            TMR_WAIT( pt, rnd_u16_get_int() >> 4 );

            log_v_debug_P( PSTR("request slave sync") );
            send_request();

            // wait some time
            TMR_WAIT( pt, 500 );

            // check if we got a sync
            if( sync_state != STATE_SLAVE ){

                break;
            }

            // wait some time before trying again
            TMR_WAIT( pt, 4000 + ( rnd_u16_get_int() >> 4 ) );
        }

    	if( sync_state == STATE_SLAVE_SYNC ){

    		log_v_debug_P( PSTR("vm sync!") );
    	}

        if( sync_state != STATE_SLAVE_SYNC ){

            TMR_WAIT( pt, 8000 );
        }

    	while( ( sync_state == STATE_SLAVE_SYNC ) && !vm_sync_wait() ){

    		TMR_WAIT( pt, 1 * 1000 );
    	}
    }

PT_END( pt );
}


#endif
