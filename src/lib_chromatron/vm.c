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

#include "system.h"
#include "threading.h"
#include "logging.h"
#include "timers.h"
#include "keyvalue.h"
#include "fs.h"
#include "random.h"
#include "graphics.h"
#include "crc.h"
#include "esp8266.h"
#include "hash.h"
#include "kvdb.h"

#include "vm.h"
#include "vm_core.h"
#include "vm_config.h"


static bool vm_reset;
static bool vm_run;

#define VM_STARTUP              0
#define VM_RUNNING              1
#define VM_SHUTDOWN_REQUESTED   2
#define VM_SHUTDOWN             3
#define VM_FINISHED             4
static uint8_t vm_mode;

static bool vm_running;

static vm_info_t vm_info;

int8_t vm_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){

    if( op == KV_OP_SET ){

        if( hash == __KV__vm_reset ){

            vm_mode = VM_STARTUP;
        }
        else if( hash == __KV__vm_shutdown ){

            vm_v_shutdown();
        }
    }
    else if( op == KV_OP_GET ){

        if( hash == __KV__vm_isa ){

            *(uint8_t *)data = VM_ISA_VERSION;
        }
    }

    return 0;
}


KV_SECTION_META kv_meta_t vm_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,     0, 0,                   &vm_reset,             vm_i8_kv_handler,   "vm_reset" },
    { SAPPHIRE_TYPE_BOOL,     0, 0,                   0,                     vm_i8_kv_handler,   "vm_shutdown" },
    { SAPPHIRE_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run,               0,                  "vm_run" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_startup_prog" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_shutdown_prog" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_info.status,       0,                  "vm_status" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_info.return_code,  0,                  "vm_retval" },
    { SAPPHIRE_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,  &vm_info.loop_time,    0,                  "vm_loop_time" },
    { SAPPHIRE_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,  &vm_info.fader_time,   0,                  "vm_fade_time" },
    { SAPPHIRE_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  0,                     vm_i8_kv_handler,   "vm_isa" },
};

#ifndef VM_TARGET_ESP
static thread_t vm_thread;
#endif


static void reset_published_data( void ){

    kvdb_v_delete_tag( VM_KV_TAG );
}

// PT_THREAD( vm_runner_thread( pt_t *pt, vm_state_t *state ) )
// {
// PT_BEGIN( pt );

//     int8_t status = vm_i8_load_program(
//                         &state->byte0,
//                         state->prog_size,
//                         state );

//     if( status < 0 ){

//         log_v_debug_P( PSTR("VM error: %d"), status );

//         THREAD_EXIT( pt );
//     }

//     log_v_debug_P( PSTR("Starts: Code: %d Data: %d"),
//         state->code_start,
//         state->data_start );

//     uint8_t *ptr = (uint8_t *)&state->byte0;
//     uint8_t *code_start = (uint8_t *)( ptr + state->code_start );
//     int32_t *data_start = (int32_t *)( ptr + state->data_start );

//     // run init
//     int8_t init_status = vm_i8_run( code_start, state->init_start, state, data_start );

//     if( init_status < 0 ){

//         log_v_debug_P( PSTR("VM init exit: %d"), init_status );

//         THREAD_EXIT( pt );
//     }

//     // state->start_time = tmr_u64_get_system_time_us();

//     // infinite loop - this thread will be killed by the control thread.
//     while( TRUE ){

//         // state->start_time += ( (uint32_t)gfx_u16_get_vm_frame_rate() * 1000 );
//         // thread_v_set_alarm( state->start_time );

//         // rebuild pointers, as actual memory may have moved around

//         uint8_t *ptr = (uint8_t *)&state->byte0;
//         uint8_t *code_start = (uint8_t *)( ptr + state->code_start );
//         int32_t *data_start = (int32_t *)( ptr + state->data_start );

//         // run loop
//         int8_t status = vm_i8_run( code_start, state->loop_start, state, data_start );
//         if( status < 0 ){

//             log_v_debug_P( PSTR("VM loop exit: %d"), status );

//             THREAD_EXIT( pt );
//         }

//         THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );
//     }

// PT_END( pt );
// }


static file_t get_program_handle( catbus_hash_t32 hash ){

    char program_fname[KV_NAME_LEN];

    if( kv_i8_get_by_hash( hash, program_fname, sizeof(program_fname) ) < 0 ){

        return -1;
    }

    file_t f = fs_f_open( program_fname, FS_MODE_READ_ONLY );

    if( f < 0 ){

        return -1;
    }

    return f;
}


#ifdef VM_TARGET_ESP

static int8_t load_vm_wifi( catbus_hash_t32 hash ){

    gfx_v_reset_subscribed();

    file_t f = get_program_handle( hash );

    if( f < 0 ){

        return -1;
    }

    if( wifi_i8_send_msg_blocking( WIFI_DATA_ID_RESET_VM, 0, 0 ) < 0 ){

        goto error;
    }

    reset_published_data();

    gfx_v_pixel_bridge_enable();

    gfx_v_sync_params();

    log_v_debug_P( PSTR("Loading VM") );

    // file found, get program size from file header
    int32_t vm_size;
    fs_i16_read( f, (uint8_t *)&vm_size, sizeof(vm_size) );

    // sanity check
    if( vm_size > VM_MAX_IMAGE_SIZE ){

        goto error;
    }

    // read header
    vm_program_header_t header;
    fs_i16_read( f, (uint8_t *)&header, sizeof(header) );

    vm_state_t state;

    int8_t status = vm_i8_load_program( VM_LOAD_FLAGS_CHECK_HEADER, (uint8_t *)&header, sizeof(header), &state );

    if( status < 0 ){

        log_v_debug_P( PSTR("VM load error: %d"), status );
        goto error;
    }

    // seek back to program start
    fs_v_seek( f, sizeof(vm_size) );

    while( vm_size > 0 ){

        uint8_t chunk[64];
        uint16_t copy_len = sizeof(chunk);

        if( copy_len > vm_size ){

            copy_len = vm_size;
        }

        int16_t read = fs_i16_read( f, chunk, copy_len );

        if( read < 0 ){

            // this should not happen. famous last words.
            goto error;
        }

        if( wifi_i8_send_msg_blocking( WIFI_DATA_ID_LOAD_VM, chunk, read ) < 0 ){

            // comm error
            goto error;
        }
        
        vm_size -= read;
    }

    // send len 0 to indicate load complete
    if( wifi_i8_send_msg_blocking( WIFI_DATA_ID_LOAD_VM, 0, 0 ) < 0 ){

        // comm error
        goto error;
    }

    // read magic number
    uint32_t meta_magic = 0;
    fs_i16_read( f, (uint8_t *)&meta_magic, sizeof(meta_magic) );

    if( meta_magic == META_MAGIC ){

        char meta_string[KV_NAME_LEN];
        memset( meta_string, 0, sizeof(meta_string) );

        // skip first string, it's the script name
        fs_v_seek( f, fs_i32_tell( f ) + sizeof(meta_string) );

        while( fs_i16_read( f, meta_string, sizeof(meta_string) ) == sizeof(meta_string) ){
            
            // load hashes
            kvdb_i8_add( hash_u32_string( meta_string ), 0, VM_KV_TAG, meta_string );

            memset( meta_string, 0, sizeof(meta_string) );
        }
    }
    else{

        log_v_debug_P( PSTR("Meta read failed") );

        goto error;
    }

    // read through database keys
    mem_handle_t h = mem2_h_alloc( sizeof(uint32_t) * state.read_keys_count );

    if( h > 0 ){

        uint32_t *read_key_hashes = mem2_vp_get_ptr( h );

        fs_v_seek( f, sizeof(vm_size) + state.read_keys_start );

        for( uint16_t i = 0; i < state.read_keys_count; i++ ){

            fs_i16_read( f, (uint8_t *)read_key_hashes, sizeof(uint32_t) );
            
            read_key_hashes++;
        }

        gfx_v_set_subscribed_keys( h );        
    }


    fs_f_close( f );


    vm_info.status = -127;
    vm_info.return_code = -127;
    vm_info.loop_time = 0;
    vm_info.fader_time = 0;

    return 0;

error:
    
    fs_f_close( f );
    return -1;
}

#else

static int8_t load_vm_local( kv_id_t8 prog_id ){

    file_t f = get_program_handle( __KV__vm_prog );

    if( f < 0 ){

        return -1;
    }

    reset_published_data();

    log_v_debug_P( PSTR("Loading VM") );

    // file found, get program size from file header
    int32_t vm_size;
    fs_i16_read( f, (uint8_t *)&vm_size, sizeof(vm_size) );

    // sanity check
    if( vm_size > VM_MAX_IMAGE_SIZE ){

        goto error;
    }
    
    vm_thread = thread_t_create( THREAD_CAST(vm_runner_thread),
                                 PSTR("vm_prog"),
                                 0,
                                 vm_size + sizeof(vm_state_t) );

    if( vm_thread < 0 ){

        goto error;
    }

    // initialize vm data to all 0s
    memset( thread_vp_get_data( vm_thread ), 0, vm_size );

    // load program data into thread state
    vm_state_t *vm_state = (vm_state_t *)thread_vp_get_data( vm_thread );

    vm_state->prog_size = vm_size;

    fs_i16_read( f, &vm_state->byte0, vm_size );

    fs_f_close( f );

    return 0;

error:

    fs_f_close( f );

    return -1;
}
#endif


PT_THREAD( vm_loader( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

#ifdef VM_TARGET_ESP

        gfx_v_reset_subscribed();
        reset_published_data();
        vm_running = FALSE;

        THREAD_WAIT_WHILE( pt, !wifi_b_attached() || !vm_run || ( vm_mode == VM_FINISHED ) );

        TMR_WAIT( pt, 100 );

        if( vm_mode == VM_STARTUP ){

            if( load_vm_wifi( __KV__vm_startup_prog ) < 0 ){

                if( load_vm_wifi( __KV__vm_prog ) < 0 ){

                    goto error;
                }
            }
        }
        else if( vm_mode == VM_RUNNING ){

            if( load_vm_wifi( __KV__vm_prog ) < 0 ){

                goto error;
            }
        }
        else{

            if( load_vm_wifi( __KV__vm_shutdown_prog ) < 0 ){

                goto error;
            }   
        }

        vm_running = TRUE;
        
        // wait for VM to finish loading
        TMR_WAIT( pt, 1000 );

        log_v_debug_P( PSTR("sts: %d ret: %d loop: %ld fade: %ld"),
                       vm_info.status,
                       vm_info.return_code,
                       vm_info.loop_time,
                       vm_info.fader_time );


        if( ( vm_info.status < 0 ) || ( vm_info.return_code < 0 ) ){

            goto error;
        }

        gfx_v_reset_frame_sync();
        vm_reset = FALSE;
        THREAD_WAIT_WHILE( pt, ( vm_reset == FALSE ) &&
                               ( vm_run == TRUE ) &&
                               ( vm_info.status == 0 ) &&
                               ( vm_info.return_code == 0 ) );

        wifi_i8_send_msg_blocking( WIFI_DATA_ID_RESET_VM, 0, 0 );

        if( vm_info.return_code < 0 ){

            goto error;
        }

        if( vm_mode == VM_STARTUP ){

            vm_mode = VM_RUNNING;
        }
        else if( vm_mode == VM_RUNNING ){

            // do nothing here
        }
        else if( vm_mode == VM_SHUTDOWN_REQUESTED ){

            vm_mode = VM_SHUTDOWN;
        }
        else{

            vm_mode = VM_FINISHED;
        }

        THREAD_YIELD( pt );
        THREAD_RESTART( pt );

    error:

        // longish delay after error to prevent swamping CPU trying
        // to reload a bad file.
        TMR_WAIT( pt, 1000 );



#else
        THREAD_WAIT_WHILE( pt, !wifi_b_attached() || !vm_run );

        if( vm_startup ){

            if( load_vm_wifi( __KV__vm_startup_prog ) < 0 ){

                if( load_vm_wifi( __KV__vm_prog ) < 0 ){

                    goto error;
                }
            }
        }
        else{

            if( load_vm_wifi( __KV__vm_prog ) < 0 ){

                goto error;
            }
        }

        vm_reset = FALSE;
        THREAD_WAIT_WHILE( pt, ( vm_reset == FALSE ) &&
                               ( vm_run == TRUE ) &&
                               ( vm_info.status == 0 ) &&
                               ( vm_info.return_code == 0 ) );


        if( vm_thread > 0 ){

            thread_v_kill( vm_thread );
        }

        if( vm_info.return_code < 0 ){

            goto error;
        }

        vm_thread = -1;

        if( vm_reset ){

            vm_startup = TRUE;
        }
        else{

            vm_startup = FALSE;
        }

        THREAD_YIELD( pt );
        THREAD_RESTART( pt );

    error:
        vm_thread = -1;

        // longish delay after error to prevent swamping CPU trying
        // to reload a bad file.
        TMR_WAIT( pt, 1000 );

#endif

    }

PT_END( pt );
}






void vm_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( vm_loader,
                 PSTR("vm_loader"),
                 0,
                 0 );

    #ifndef VM_TARGET_ESP
    vm_thread = -1;
    #endif
}

void vm_v_start( void ){

    vm_run = TRUE;
}

void vm_v_stop( void ){

    vm_run = FALSE;
}

void vm_v_reset( void ){

    vm_reset = TRUE;
}

void vm_v_shutdown( void ){

    vm_mode = VM_SHUTDOWN_REQUESTED;
    vm_v_reset();
}

// NOTE changing the program will not reload the VM.
// You must call vm_v_reset to load the new program.

void vm_v_set_program_P( PGM_P ptr ){

    char progname[VM_MAX_FILENAME_LEN];

    memset( progname, 0, sizeof(progname) );

	strlcpy_P( progname, ptr, sizeof(progname) );

    vm_v_set_program( progname );
}

void vm_v_set_program( char progname[VM_MAX_FILENAME_LEN] ){

    kv_i8_set_by_hash( __KV__vm_prog, progname, VM_MAX_FILENAME_LEN );
}

void vm_v_received_info( vm_info_t *info ){

    vm_info = *info;
}

bool vm_b_running( void ){

    return vm_running;
}
