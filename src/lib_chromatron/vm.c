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
#include "esp8266.h"
#include "hash.h"
#include "kvdb.h"
#include "vm_wifi_cmd.h"

#include "vm.h"
#include "vm_core.h"
#include "vm_config.h"


static bool vm_reset[VM_MAX_VMS];
static bool vm_run[VM_MAX_VMS];

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_loop_time[VM_MAX_VMS];
static uint16_t vm_thread_time[VM_MAX_VMS];
static uint16_t vm_max_cycles[VM_MAX_VMS];

static uint16_t vm_fader_time;
static uint16_t vm_total_size;

int8_t vm_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){

    if( op == KV_OP_SET ){

    }
    else if( op == KV_OP_GET ){

        if( hash == __KV__vm_isa ){

            *(uint8_t *)data = VM_ISA_VERSION;
        }
    }

    return 0;
}


KV_SECTION_META kv_meta_t vm_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                  "vm_reset" },
    { SAPPHIRE_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                  "vm_run" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                  "vm_status" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_loop_time[0],      0,                  "vm_loop_time" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_thread_time[0],    0,                  "vm_thread_time" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                  "vm_max_cycles" },

    { SAPPHIRE_TYPE_BOOL,     0, 0,                   &vm_reset[1],          0,                  "vm_reset_1" },
    { SAPPHIRE_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[1],            0,                  "vm_run_1" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_1" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[1],         0,                  "vm_status_1" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_loop_time[1],      0,                  "vm_loop_time_1" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_thread_time[1],    0,                  "vm_thread_time_1" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[1],     0,                  "vm_max_cycles_1" },

    { SAPPHIRE_TYPE_BOOL,     0, 0,                   &vm_reset[2],          0,                  "vm_reset_2" },
    { SAPPHIRE_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[2],            0,                  "vm_run_2" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_2" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[2],         0,                  "vm_status_2" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_loop_time[2],      0,                  "vm_loop_time_2" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_thread_time[2],    0,                  "vm_thread_time_2" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[2],     0,                  "vm_max_cycles_2" },

    { SAPPHIRE_TYPE_BOOL,     0, 0,                   &vm_reset[3],          0,                  "vm_reset_3" },
    { SAPPHIRE_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[3],            0,                  "vm_run_3" },
    { SAPPHIRE_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_3" },
    { SAPPHIRE_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[3],         0,                  "vm_status_3" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_loop_time[3],      0,                  "vm_loop_time_3" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_thread_time[3],    0,                  "vm_thread_time_3" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[3],     0,                  "vm_max_cycles_3" },

    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_fader_time,        0,                  "vm_fade_time" },
    { SAPPHIRE_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_total_size,        0,                  "vm_total_size" },
    { SAPPHIRE_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  0,                     vm_i8_kv_handler,   "vm_isa" },
};

#ifndef VM_TARGET_ESP
static thread_t vm_thread;
#endif

// keys that we really don't want the VM be to be able to write to.
// generally, these are going to be things that would allow it to 
// brick hardware, mess up the wifi connection, or mess up the pixel 
// array.
static const PROGMEM uint32_t restricted_keys[] = {
    __KV__reboot,
    __KV__wifi_enable_ap,
    __KV__wifi_fw_len,
    __KV__wifi_md5,
    __KV__wifi_router,
    __KV__pix_clock,
    __KV__pix_count,
    __KV__pix_mode,
    __KV__catbus_data_port,    
};


static catbus_hash_t32 get_link_tag( uint8_t vm_id ){

    catbus_hash_t32 link_tag = 0;

    if( vm_id == 0 ){

        link_tag = __KV__vm_0;
    }
    else if( vm_id == 1 ){

        link_tag = __KV__vm_1;
    }
    else if( vm_id == 2 ){

        link_tag = __KV__vm_2;
    }
    else if( vm_id == 3 ){

        link_tag = __KV__vm_3;
    }
    else{

        ASSERT( FALSE );
    }

    return link_tag;
}


static void reset_published_data( uint8_t vm_id ){

    kvdb_v_delete_tag( VM_KV_TAG_START + vm_id );
    catbus_v_purge_links( get_link_tag( vm_id ) );
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

// void published_var_notifier(
//     catbus_hash_t32 hash,
//     catbus_type_t8 type,
//     const void *data )
// {
//     gfx_i8_send_keys( &hash, 1 );
// }

static file_t get_program_handle( catbus_hash_t32 hash ){

    char program_fname[KV_NAME_LEN];

    if( kv_i8_get( hash, program_fname, sizeof(program_fname) ) < 0 ){

        return -1;
    }

    file_t f = fs_f_open( program_fname, FS_MODE_READ_ONLY );

    if( f < 0 ){

        return -1;
    }

    return f;
}


#ifdef VM_TARGET_ESP

static int8_t send_reset_message( uint8_t vm_id ){

    wifi_msg_reset_vm_t reset_msg;
    reset_msg.vm_id = vm_id;

    return wifi_i8_send_msg_blocking( WIFI_DATA_ID_RESET_VM, (uint8_t *)&reset_msg, sizeof(reset_msg) );
}

static int8_t load_vm_wifi( uint8_t vm_id ){

    wifi_msg_load_vm_t vm_load_msg;

    gfx_v_reset_subscribed( VM_KV_TAG_START + vm_id );

    catbus_hash_t32 hash = 0;
    catbus_hash_t32 link_tag = get_link_tag( vm_id );
    
    if( vm_id == 0 ){

        hash = __KV__vm_prog;
    }
    else if( vm_id == 1 ){

        hash = __KV__vm_prog_1;
    }
    else if( vm_id == 2 ){

        hash = __KV__vm_prog_2;
    }
    else if( vm_id == 3 ){

        hash = __KV__vm_prog_3;
    }
    else{

        ASSERT( FALSE );
    }


    file_t f = get_program_handle( hash );

    if( f < 0 ){

        return -1;
    }

    if( send_reset_message( vm_id ) < 0 ){

        goto error;
    }

    reset_published_data( vm_id );

    // sync graphics parameters, because script init function will run as soon as loading is complete.
    gfx_v_sync_params();
    
    log_v_debug_P( PSTR("Loading VM: %d"), vm_id );

    // file found, get program size from file header
    int32_t vm_size;
    fs_i16_read( f, (uint8_t *)&vm_size, sizeof(vm_size) );

    // sanity check
    if( vm_size > VM_MAX_IMAGE_SIZE ){

        goto error;
    }

    fs_v_seek( f, 0 );    
    int32_t check_len = fs_i32_get_size( f ) - sizeof(uint32_t);

    uint32_t computed_file_hash = hash_u32_start();

    // check file hash
    while( check_len > 0 ){

        uint16_t copy_len = sizeof(vm_load_msg.chunk);

        if( copy_len > check_len ){

            copy_len = check_len;
        }

        int16_t read = fs_i16_read( f, vm_load_msg.chunk, copy_len );

        if( read < 0 ){

            // this should not happen. famous last words.
            goto error;
        }

        // update hash
        computed_file_hash = hash_u32_partial( computed_file_hash, vm_load_msg.chunk, copy_len );
        
        check_len -= read;
    }

    // read file hash
    uint32_t file_hash = 0;
    fs_i16_read( f, (uint8_t *)&file_hash, sizeof(file_hash) );

    // check hashes
    if( file_hash != computed_file_hash ){

        log_v_debug_P( PSTR("VM load error: %d"), VM_STATUS_ERR_BAD_FILE_HASH );
        goto error;
    }

    // read header
    fs_v_seek( f, sizeof(vm_size) );    
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

        uint16_t copy_len = sizeof(vm_load_msg.chunk);

        if( copy_len > vm_size ){

            copy_len = vm_size;
        }

        int16_t read = fs_i16_read( f, vm_load_msg.chunk, copy_len );

        if( read < 0 ){

            // this should not happen. famous last words.
            goto error;
        }

        vm_load_msg.vm_id = vm_id;
        if( wifi_i8_send_msg_blocking( WIFI_DATA_ID_LOAD_VM, (uint8_t *)&vm_load_msg, read + sizeof(vm_load_msg.vm_id) ) < 0 ){

            // comm error
            goto error;
        }
        
        vm_size -= read;
    }

    // read magic number
    uint32_t meta_magic = 0;
    fs_i16_read( f, (uint8_t *)&meta_magic, sizeof(meta_magic) );

    if( meta_magic == META_MAGIC ){

        char meta_string[KV_NAME_LEN];
        memset( meta_string, 0, sizeof(meta_string) );

        // skip first string, it's the script name
        fs_v_seek( f, fs_i32_tell( f ) + sizeof(meta_string) );

        // load meta names to database lookup
        while( fs_i16_read( f, meta_string, sizeof(meta_string) ) == sizeof(meta_string) ){
        
            kvdb_v_set_name( meta_string );
            
            memset( meta_string, 0, sizeof(meta_string) );
        }
    }
    else{

        log_v_debug_P( PSTR("Meta read failed") );

        goto error;
    }


    catbus_meta_t meta;
    uint32_t subscribed_key_hashes[64];
    uint8_t subscribe_key_count = 0;

    // set up additional DB entries
    fs_v_seek( f, sizeof(vm_size) + state.db_start );

    for( uint8_t i = 0; i < state.db_count; i++ ){

        fs_i16_read( f, (uint8_t *)&meta, sizeof(meta) );

        kvdb_i8_add( meta.hash, meta.type, meta.count + 1, 0, 0 );
        kvdb_v_set_tag( meta.hash, VM_KV_TAG_START + vm_id );      

        if( subscribe_key_count < cnt_of_array(subscribed_key_hashes) ){

            subscribed_key_hashes[subscribe_key_count] = meta.hash;
            subscribe_key_count++;
        }
    }   


    // read through database keys
    uint32_t read_key_hash = 0;

    fs_v_seek( f, sizeof(vm_size) + state.read_keys_start );

    for( uint16_t i = 0; i < state.read_keys_count; i++ ){

        fs_i16_read( f, (uint8_t *)&read_key_hash, sizeof(uint32_t) );

        if( kv_i8_get_meta( read_key_hash, &meta ) >= 0 ){

            wifi_msg_kv_add_t kv_add_msg;
            kv_add_msg.vm_id    = vm_id;
            kv_add_msg.meta     = meta;

            wifi_i8_send_msg_blocking( WIFI_DATA_ID_KV_ADD, (uint8_t *)&kv_add_msg, sizeof(kv_add_msg) );
        }
        
        if( subscribe_key_count < cnt_of_array(subscribed_key_hashes) ){
        
            subscribed_key_hashes[subscribe_key_count] = meta.hash;
            subscribe_key_count++;
        }
    }
    

    // check published keys and add to DB
    fs_v_seek( f, sizeof(vm_size) + state.publish_start );

    for( uint8_t i = 0; i < state.publish_count; i++ ){

        vm_publish_t publish;

        fs_i16_read( f, (uint8_t *)&publish, sizeof(publish) );

        kvdb_i8_add( publish.hash, CATBUS_TYPE_INT32, 1, 0, 0 );
        kvdb_v_set_tag( publish.hash, VM_KV_TAG_START + vm_id );
    }   

    // subscribe keys to DB sender
    gfx_v_subscribe_keys( subscribed_key_hashes, subscribe_key_count, VM_KV_TAG_START + vm_id );

    // check write keys
    fs_v_seek( f, sizeof(vm_size) + state.write_keys_start );

    for( uint8_t i = 0; i < state.write_keys_count; i++ ){

        uint32_t write_hash = 0;
        fs_i16_read( f, (uint8_t *)&write_hash, sizeof(write_hash) );

        if( write_hash == 0 ){

            continue;
        }

        // make sure ESP's VM hash this database entry
        if( kv_i8_get_meta( write_hash, &meta ) >= 0 ){

            wifi_i8_send_msg_blocking( WIFI_DATA_ID_KV_ADD, (uint8_t *)&meta, sizeof(meta) );
        }

        // check if writing to restricted key
        for( uint8_t j = 0; j < cnt_of_array(restricted_keys); j++ ){

            uint32_t restricted_key = 0;
            memcpy_P( (uint8_t *)&restricted_key, &restricted_keys[j], sizeof(restricted_key) );

            if( restricted_key == 0 ){

                continue;
            }   

            // check for match
            if( restricted_key == write_hash ){

                vm_status[vm_id] = VM_STATUS_RESTRICTED_KEY;

                log_v_debug_P( PSTR("Restricted key: %lu"), write_hash );

                goto error;
            }
        }        
    }

    // set up links
    fs_v_seek( f, sizeof(vm_size) + state.link_start );

    for( uint8_t i = 0; i < state.link_count; i++ ){

        link_t link;

        fs_i16_read( f, (uint8_t *)&link, sizeof(link) );

        if( link.send ){

            catbus_l_send( link.source_hash, link.dest_hash, &link.query, link_tag );
        }
        else{

            catbus_l_recv( link.dest_hash, link.source_hash, &link.query, link_tag );
        }
    }


    // send len 0 to indicate load complete.
    // this will trigger the init function, so we want to do this
    // after the database has been initializd.
    vm_load_msg.vm_id = vm_id;
    if( wifi_i8_send_msg_blocking( WIFI_DATA_ID_LOAD_VM, (uint8_t *)&vm_load_msg, sizeof(vm_load_msg.vm_id) ) < 0 ){

        // comm error
        goto error;
    }


    fs_f_close( f );

    vm_status[vm_id]        = 0;
    vm_loop_time[vm_id]     = 0;
    vm_thread_time[vm_id]   = 0;
    vm_max_cycles[vm_id]    = 0;

    return 0;

error:
    
    fs_f_close( f );
    return -1;
}

#else

// static int8_t load_vm_local( kv_id_t8 prog_id ){

//     file_t f = get_program_handle( __KV__vm_prog );

//     if( f < 0 ){

//         return -1;
//     }

//     reset_published_data();

//     log_v_debug_P( PSTR("Loading VM") );

//     // file found, get program size from file header
//     int32_t vm_size;
//     fs_i16_read( f, (uint8_t *)&vm_size, sizeof(vm_size) );

//     // sanity check
//     if( vm_size > VM_MAX_IMAGE_SIZE ){

//         goto error;
//     }
    
//     vm_thread = thread_t_create( THREAD_CAST(vm_runner_thread),
//                                  PSTR("vm_prog"),
//                                  0,
//                                  vm_size + sizeof(vm_state_t) );

//     if( vm_thread < 0 ){

//         goto error;
//     }

//     // initialize vm data to all 0s
//     memset( thread_vp_get_data( vm_thread ), 0, vm_size );

//     // load program data into thread state
//     vm_state_t *vm_state = (vm_state_t *)thread_vp_get_data( vm_thread );

//     vm_state->prog_size = vm_size;

//     fs_i16_read( f, &vm_state->byte0, vm_size );

//     fs_f_close( f );

//     return 0;

// error:

//     reset_published_data();

//     fs_f_close( f );

//     return -1;
// }
#endif

static bool is_vm_running( uint8_t vm_id ){

    return vm_status[vm_id] == 0;
}


PT_THREAD( vm_loader( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    kvdb_v_set_name_P( PSTR("vm_0") );
    kvdb_v_set_name_P( PSTR("vm_1") );
    kvdb_v_set_name_P( PSTR("vm_2") );
    kvdb_v_set_name_P( PSTR("vm_3") );


    THREAD_WAIT_WHILE( pt, !wifi_b_attached() );


    while(1){

        
#ifdef VM_TARGET_ESP

        THREAD_WAIT_WHILE( pt, 
            ( ( ( !vm_run[0]  && !is_vm_running( 0 ) )  ||
                ( vm_run[0]   && is_vm_running( 0 ) ) )    &&
              ( !vm_reset[0] ) )
            &&
            ( ( ( !vm_run[1]  && !is_vm_running( 1 ) )  ||
                ( vm_run[1]   && is_vm_running( 1 ) ) )    &&
              ( !vm_reset[1] ) )
            &&
            ( ( ( !vm_run[2]  && !is_vm_running( 2 ) )  ||
                ( vm_run[2]   && is_vm_running( 2 ) ) )    &&
              ( !vm_reset[2] ) )
            &&
            ( ( ( !vm_run[3]  && !is_vm_running( 3 ) )  ||
                ( vm_run[3]   && is_vm_running( 3 ) ) )    &&
              ( !vm_reset[3] ) )
        );


        // check what we're doing, and to what VM    
        for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

            // Was there an error and the VM is running
            if( ( vm_run[i] ) &&
                ( vm_status[i] != VM_STATUS_NOT_RUNNING ) &&
                ( vm_status[i] != 0 ) ){

                vm_run[i] = FALSE;
                vm_reset[i] = FALSE;

                if( vm_status[i] == VM_STATUS_HALT ){

                    // this isn't actually an error, it is the VM
                    // signalling the script has requested a stop.
                    log_v_debug_P( PSTR("VM %d halted"), i );
                }
                else{

                    log_v_debug_P( PSTR("VM %d error: %d"), i, vm_status[i] );
                }
            }

            // Are we resetting a VM?
            if( vm_reset[i] ){

                log_v_debug_P( PSTR("Resetting VM: %d"), i );

                send_reset_message( i );
                vm_status[i] = VM_STATUS_NOT_RUNNING;
            }

            // Did VM that was not running just get told to start?
            // This will also occur if we've triggered a reset
            if( vm_run[i] && !is_vm_running( i ) ){

                if( load_vm_wifi( i ) < 0 ){

                    vm_run[i] = FALSE;

                    log_v_debug_P( PSTR("VM load fail: %d"), i );

                    goto error; 
                }

                vm_status[i] = VM_STATUS_OK;
            }
            // Did VM that was running just get told to stop?
            else if( !vm_run[i] && is_vm_running( i ) ){

                log_v_debug_P( PSTR("Stopping VM: %d"), i );
                send_reset_message( i );
                vm_status[i] = VM_STATUS_NOT_RUNNING;

                vm_loop_time[i]     = 0;
                vm_thread_time[i]   = 0;
                vm_max_cycles[i]    = 0;
            }
            
            // always reset the reset
            vm_reset[i] = FALSE;
        }

        TMR_WAIT( pt, 100 );
        continue;


    error:

        // longish delay after error to prevent swamping CPU trying
        // to reload a bad file.
        TMR_WAIT( pt, 1000 );


#else
  
#endif

    }

PT_END( pt );
}






void vm_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    memset( vm_status, VM_STATUS_NOT_RUNNING, sizeof(vm_status) );

    thread_t_create( vm_loader,
                 PSTR("vm_loader"),
                 0,
                 0 );

    #ifndef VM_TARGET_ESP
    vm_thread = -1;
    #endif
}

void vm_v_start( void ){

    vm_run[0] = TRUE;
}

void vm_v_stop( void ){

    vm_run[0] = FALSE;
}

void vm_v_reset( void ){

    vm_reset[0] = TRUE;
}

void vm_v_received_info( wifi_msg_vm_info_t *msg ){

    for( uint8_t i = 0; i < cnt_of_array(msg->vm_info); i++ ){

        vm_status[i]        = msg->vm_info[i].status;
        vm_loop_time[i]     = msg->vm_info[i].loop_time;
        vm_thread_time[i]   = msg->vm_info[i].thread_time;
    }

    vm_fader_time           = msg->fader_time;
    vm_total_size           = msg->vm_total_size;
}

bool vm_b_running( void ){

    for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

        if( is_vm_running( i ) ){

            return TRUE;
        }
    }

    return FALSE;
}
