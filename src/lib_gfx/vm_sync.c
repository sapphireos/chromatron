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

#include "sapphire.h"

#ifdef ENABLE_TIME_SYNC

#include "timesync.h"
#include "vm_sync.h"
#include "vm.h"
#include "hash.h"
#include "graphics.h"
#include "config.h"
#include "services.h"
#include "logging.h"
#include "vm_sequencer.h"

static uint32_t sync_group_hash;
static socket_t sock = -1;

static uint8_t sync_state;
#define STATE_IDLE 			0
#define STATE_SYNCING       1
#define STATE_SYNC 	        2

static bool hold_sync;

static int16_t sync_data_remaining;

int8_t vmsync_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__gfx_sync_group ){

            sync_group_hash = hash_u32_string( data );    

            vm_sync_v_reset();
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t vm_sync_kv[] = {
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,   0, vmsync_i8_kv_handler,   "gfx_sync_group" },
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, &sync_state, 0,            "gfx_sync_state" },
};


PT_THREAD( vm_sync_server_thread( pt_t *pt, void *state ) );
PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) );

static void init_group_hash( void ){

    // init sync group hash
    char buf[32];
    memset( buf, 0, sizeof(buf) );
    kv_i8_get( __KV__gfx_sync_group, buf, sizeof(buf) );

    sync_group_hash = hash_u32_string( buf );    
}


void vm_sync_v_init( void ){

    COMPILER_ASSERT( SYNC_MAX_THREADS >= VM_MAX_THREADS );

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // check if time sync is enabled
    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        return;
    }

    init_group_hash();

    thread_t_create( vm_sync_server_thread,
                    PSTR("vm_sync_server"),
                    0,
                    0 );    
}

void vm_sync_v_reset( void ){

    sync_state = STATE_IDLE;

    uint32_t old_hash = sync_group_hash;

    // services_v_cancel( SYNC_SERVICE, sync_group_hash );

    init_group_hash();// init sync group hash    

    // check if hash changed, if so, cancel
    // the previous service
    if( old_hash != sync_group_hash ){

        services_v_cancel( SYNC_SERVICE, old_hash );    
    }

    if( sync_group_hash == 0 ){

        return;
    }

    // log_v_debug_P( PSTR("sync reset") );
}

void vm_sync_v_hold( void ){

    vm_sync_v_reset();

    hold_sync = TRUE;
}

void vm_sync_v_unhold( void ){

    vm_sync_v_reset();

    hold_sync = FALSE;   
}

bool vm_sync_b_is_leader( void ){

    return services_b_is_server( SYNC_SERVICE, sync_group_hash );
}

bool vm_sync_b_is_follower( void ){

    if( services_b_is_available( SYNC_SERVICE, sync_group_hash ) &&
        !services_b_is_server( SYNC_SERVICE, sync_group_hash ) ){

        return TRUE;
    }

    return FALSE;
}

bool vm_sync_b_is_synced( void ){

    return sync_state == STATE_SYNC;
}

bool vm_sync_b_in_progress( void ){

    return sync_state == STATE_SYNCING;
}

static void send_sync( sock_addr_t *raddr ){

    vm_state_t *state = vm_p_get_state();

    if( state == 0 ){

        return;
    }


    vm_sync_msg_sync_t msg = {0};

    msg.header.magic            = SYNC_PROTOCOL_MAGIC;
    msg.header.version          = SYNC_PROTOCOL_VERSION;
    msg.header.type             = VM_SYNC_MSG_SYNC;
    msg.header.flags            = 0;
    msg.header.sync_group_hash  = sync_group_hash;

    msg.header.program_name_hash= state->program_name_hash;
    msg.header.program_file_hash= state->file_hash;
    msg.sync_tick               = vm_u64_get_sync_tick();
    msg.net_time                = vm_u32_get_sync_time();

    msg.tick                    = state->tick;
    msg.loop_tick               = state->loop_tick;
    msg.rng_seed                = state->rng_seed;
    msg.frame_number            = state->frame_number;

    msg.checkpoint              = vm_u32_get_checkpoint();
    msg.checkpoint_hash         = vm_u32_get_checkpoint_hash();

    msg.sequencer_step          = vm_seq_u8_get_step();
    
    msg.data_len                = vm_u16_get_sync_data_len();
    msg.max_threads             = VM_MAX_THREADS;

    if( msg.max_threads > SYNC_MAX_THREADS ){

        msg.max_threads = SYNC_MAX_THREADS;
    }

    memset( msg.threads, 0, sizeof(msg.threads) );

    for( uint8_t i = 0; i < msg.max_threads; i++ ){

        msg.threads[i] = state->threads[i];
    }

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), raddr );
}

static void send_data( int32_t *data, uint16_t len, uint64_t tick, uint16_t offset, sock_addr_t *raddr ){

    mem_handle_t h = mem2_h_alloc( ( sizeof(vm_sync_msg_data_t) - 1 ) + len );

    if( h < 0 ){

        return;
    }

    vm_state_t *state = vm_p_get_state();

    if( state == 0 ){

        return;
    }

    vm_sync_msg_data_t *msg = (vm_sync_msg_data_t *)mem2_vp_get_ptr( h );
    msg->header.magic            = SYNC_PROTOCOL_MAGIC;
    msg->header.version          = SYNC_PROTOCOL_VERSION;
    msg->header.type             = VM_SYNC_MSG_DATA;
    msg->header.flags            = 0;
    msg->header.sync_group_hash  = sync_group_hash;
    msg->header.program_name_hash= state->program_name_hash;
    msg->header.program_file_hash= state->file_hash;

    msg->tick                    = tick;
    msg->offset                  = offset;
    msg->padding                 = 0;

    memcpy( &msg->data, data, len );

    sock_i16_sendto_m( sock, h, raddr );
}

static void send_request( bool request_data ){

    vm_state_t *state = vm_p_get_state();

    if( state == 0 ){

        return;
    }

    vm_sync_msg_sync_req_t msg;
    msg.header.magic            = SYNC_PROTOCOL_MAGIC;
    msg.header.version          = SYNC_PROTOCOL_VERSION;
    msg.header.type             = VM_SYNC_MSG_SYNC_REQ;
    msg.header.flags            = 0;
    msg.header.sync_group_hash  = sync_group_hash;
    msg.header.program_name_hash= state->program_name_hash;
    msg.header.program_file_hash= state->file_hash;

    msg.request_data            = request_data;


    sock_addr_t raddr = services_a_get( SYNC_SERVICE, sync_group_hash );
    
    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );
}

PT_THREAD( vm_sync_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, sync_group_hash == 0 );

    log_v_debug_P( PSTR("starting VM sync server") );

    sock = sock_s_create( SOS_SOCK_DGRAM ); 

    thread_t_create( vm_sync_thread,
                    PSTR("vm_sync"),
                    0,
                    0 );   


    while( TRUE ){

    	THREAD_WAIT_WHILE( pt, 
            ( sock_i8_recvfrom( sock ) < 0 ) &&
             ( !sys_b_is_shutting_down() ) );

    	// check if shutting down
    	if( sys_b_is_shutting_down() ){

    		THREAD_EXIT( pt );
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

        vm_state_t *vm_state = vm_p_get_state();

        if( vm_state == 0 ){

            continue;
        }

        if( header->type == VM_SYNC_MSG_SYNC ){

            log_v_debug_P( PSTR("VM_SYNC_MSG_SYNC") );

            // are we leader?
            if( vm_sync_b_is_leader() ){

                continue;
            }

            vm_sync_msg_sync_t *msg = (vm_sync_msg_sync_t *)header;

            // confirm program name
            if( header->program_name_hash != vm_state->program_name_hash ){

                vm_sync_v_reset();

                // sync sequencer
                vm_seq_v_set_step( msg->sequencer_step );

                log_v_error_P( PSTR("program name mismatch") );

                continue;
            }

            // confirm program hash
            if( header->program_file_hash != vm_state->file_hash ){

                vm_sync_v_reset();

                log_v_error_P( PSTR("program hash mismatch") );

                continue;
            }

            if( msg->max_threads > VM_MAX_THREADS ) {

                vm_sync_v_reset();

                log_v_error_P( PSTR("too many VM threads") );

                continue;                
            }

            // sync VM
            vm_v_sync( msg->net_time, msg->sync_tick );

            // sync sequencer
            vm_seq_v_set_step( msg->sequencer_step );

            // do we actually get there?
            // a seq step should change the file hash?

            // NEED TO DELAY HERE
            // to allow new VM step to load!
            // ***** vm_state is out of sync at this point!

            if( sync_state == STATE_SYNCING ){

                sync_data_remaining = msg->data_len;

                vm_state->tick         = msg->tick;
                vm_state->loop_tick    = msg->loop_tick;
                vm_state->rng_seed     = msg->rng_seed;
                vm_state->frame_number = msg->frame_number;

                uint8_t thread_count = VM_MAX_THREADS;

                if( thread_count > msg->max_threads ){

                    thread_count = msg->max_threads;
                }

                // sync threads
                for( uint8_t i = 0; i < thread_count; i++ ){

                    vm_state->threads[i] = msg->threads[i];
                }

                // log_v_debug_P( PSTR("sync: vm tick %d sync tick %d"), (int32_t)msg->tick, (int32_t)msg->sync_tick );
            }
            else if( sync_state == STATE_SYNC ){

                // verify checkpoint
                if( vm_u32_get_checkpoint() == msg->checkpoint ){

                    if( vm_u32_get_checkpoint_hash() != msg->checkpoint_hash ){

                        log_v_warn_P( PSTR("checkpoint hash mismatch!: %x -> %x @ %u"), msg->checkpoint_hash, vm_u32_get_checkpoint_hash(), vm_u32_get_checkpoint() );
                        vm_sync_v_reset();
                    }
                }
                else{

                    // log_v_debug_P( PSTR("checkpoint frame mismatch: %u -> %u"), msg->checkpoint, vm_u32_get_checkpoint() );
                }
            }

            log_v_debug_P( PSTR("END VM_SYNC_MSG_SYNC") );
        }
        else if( header->type == VM_SYNC_MSG_SYNC_REQ ){

            log_v_debug_P( PSTR("VM_SYNC_MSG_SYNC_REQ") );

            // are we leader?
            if( !vm_sync_b_is_leader() ){

                continue;
            }

            // confirm program name
            if( header->program_name_hash != vm_state->program_name_hash ){

                log_v_error_P( PSTR("invalid program hash") );

                send_sync( &raddr );

                continue;
            }

            // confirm program hash
            if( header->program_file_hash != vm_state->file_hash ){

                log_v_error_P( PSTR("invalid file hash") );

                send_sync( &raddr );

                continue;
            }
            
            // send sync
            send_sync( &raddr );

            vm_sync_msg_sync_req_t *msg = (vm_sync_msg_sync_req_t *)header;

            if( msg->request_data ){

                // log_v_debug_P( PSTR("msg->request_data") );

                // send data
                uint16_t offset = 0;
                uint16_t data_len = vm_u16_get_sync_data_len();

                // TODO
                // need to split the chunk transmission with some delays.
                // most VM programs only use a single chunk though.

                if( data_len > VM_SYNC_MAX_DATA_LEN ){

                    log_v_debug_P( PSTR("vm sync data multiple chunks: %d"), data_len );
                }

                while( offset < data_len ){

                    uint16_t chunk_size = VM_SYNC_MAX_DATA_LEN;

                    if( chunk_size > data_len ){

                        chunk_size = data_len;
                    }

                    log_v_debug_P( PSTR("sending sync data: %d bytes"), chunk_size );

                    send_data( vm_i32p_get_sync_data(), chunk_size, vm_u64_get_sync_tick(), offset, &raddr );

                    offset += chunk_size;
                } 
            }    

            log_v_debug_P( PSTR("END VM_SYNC_MSG_SYNC_REQ") );
        }
        else if( header->type == VM_SYNC_MSG_DATA ){

            // are we leader?
            if( vm_sync_b_is_leader() ){

                continue;
            }

            log_v_debug_P( PSTR("VM_SYNC_MSG_SYNC_DATA") );

            // are we syncing?
            if( sync_state != STATE_SYNCING ){

                log_v_error_P( PSTR("invalid sync state") );

                continue;
            }

            log_v_debug_P( PSTR("header: %x state %x"), header, vm_state );

            // confirm program name
            if( header->program_name_hash != vm_state->program_name_hash ){

                log_v_error_P( PSTR("invalid program hash") );

                vm_sync_v_reset();

                continue;
            }


            log_v_debug_P( PSTR("hash") );

            // confirm program hash
            if( header->program_file_hash != vm_state->file_hash ){

                log_v_error_P( PSTR("invalid file hash") );

                vm_sync_v_reset();

                continue;
            }


            log_v_debug_P( PSTR("msg") );

            vm_sync_msg_data_t *msg = (vm_sync_msg_data_t *)header;
                
            log_v_debug_P( PSTR("len") );

            int16_t data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(vm_sync_msg_data_t) - 1 );

            if( data_len <= 0 ){

                log_v_error_P( PSTR("invalid data len %d"), data_len );

                continue;
            }

            log_v_debug_P( PSTR("rx") );
            log_v_debug_P( PSTR("rx data offset %d / %d len: %d"), msg->offset, sync_data_remaining, data_len );

            if( data_len > sync_data_remaining ){

                log_v_error_P( PSTR("more data than expected: %d > %d"), data_len, sync_data_remaining );
                continue;
            }

            sync_data_remaining -= data_len;

            // validate remaining data len before copying into our VM:
            if( sync_data_remaining == 0 ){

                log_v_debug_P( PSTR("sync complete") );

                sync_state = STATE_SYNC;
            }
            else if( sync_data_remaining < 0 ){

                log_v_error_P( PSTR("invalid sync data remaining: %d"), sync_data_remaining );
                continue;
            }   


            int32_t *data_ptr = vm_i32p_get_sync_data() + msg->offset;

            // log_v_debug_P( PSTR("offset: %d"), msg->offset );

            memcpy( data_ptr, &msg->data, data_len );

            log_v_debug_P( PSTR("END VM_SYNC_MSG_SYNC_DATA") );
        }
    }

PT_END( pt );
}

PT_THREAD( vm_sync_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

        vm_sync_v_reset();

        TMR_WAIT( pt, 100 );

        THREAD_WAIT_WHILE( pt, hold_sync );

        // wait until time sync
        THREAD_WAIT_WHILE( pt, !time_b_is_sync() );

        // wait while VM 0 is stopped
        THREAD_WAIT_WHILE( pt, !vm_b_is_vm_running( 0 ) );

        services_v_join_team( SYNC_SERVICE, sync_group_hash, 1, sock_u16_get_lport( sock ) );

        THREAD_WAIT_WHILE( pt, !services_b_is_available( SYNC_SERVICE, sync_group_hash ) );

        if( services_b_is_server( SYNC_SERVICE, sync_group_hash ) ){

            log_v_debug_P( PSTR("VM sync leader") );

            sync_state = STATE_SYNC;

            THREAD_WAIT_WHILE( pt, services_b_is_server( SYNC_SERVICE, sync_group_hash ) && vm_b_is_vm_running( 0 ) );

            THREAD_RESTART( pt );
        }

        if( sync_state == STATE_IDLE ){

            TMR_WAIT( pt, rnd_u16_get_int() >> 5 );

            log_v_debug_P( PSTR("starting VM sync") );
            
            sync_state = STATE_SYNCING;

            while( sync_state == STATE_SYNCING ){

                if( ( !services_b_is_available( SYNC_SERVICE, sync_group_hash ) ) ||
                    ( services_b_is_server( SYNC_SERVICE, sync_group_hash ) ) ||
                    ( !vm_b_is_vm_running( 0 ) ) ){
                    
                    THREAD_RESTART( pt );
                }

                send_request( TRUE );

                TMR_WAIT( pt, 2000 );
            }
        }

        // periodic resync
        while( sync_state == STATE_SYNC ){

            uint16_t interval = SYNC_INTERVAL;

            if( vm_seq_b_running() ){

                interval = SYNC_INTERVAL_SEQ;
            }
            
            thread_v_set_alarm( tmr_u32_get_system_time_ms() + interval );

            THREAD_WAIT_WHILE( pt, 
                services_b_is_available( SYNC_SERVICE, sync_group_hash ) && 
                vm_b_is_vm_running( 0 ) &&
                ( sync_state == STATE_SYNC ) &&
                thread_b_alarm_set() );

            if( services_b_is_available( SYNC_SERVICE, sync_group_hash ) && vm_b_is_vm_running( 0 ) ){

                send_request( FALSE );
            }
            else{

                break;
            }
        }
    }

PT_END( pt );
}


#endif
