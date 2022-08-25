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

#include "system.h"
#include "threading.h"
#include "logging.h"
#include "timers.h"
#include "keyvalue.h"
#include "fs.h"
#include "random.h"
#include "graphics.h"
#include "hash.h"
#include "kvdb.h"
#include "config.h"
#include "timesync.h"
#include "vm_sync.h"
#include "util.h"

#include "vm.h"
#include "vm_core.h"
#include "vm_cron.h"

#ifdef ENABLE_GFX

static thread_t vm_threads[VM_MAX_VMS];

static bool vm_reset[VM_MAX_VMS];
static bool vm_run[VM_MAX_VMS];

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_run_time[VM_MAX_VMS];
static uint16_t vm_max_cycles[VM_MAX_VMS];
static uint8_t vm_timing_status;

#define VM_FLAG_UPDATE_FRAME_RATE   0x08
static uint8_t vm_run_flags[VM_MAX_VMS];


int8_t vm_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){

    if( op == KV_OP_GET ){

        if( hash == __KV__vm_isa ){

            *(uint8_t *)data = VM_ISA_VERSION;
        }
    }

    return 0;
}


KV_SECTION_META kv_meta_t vm_info_kv[] = {
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                  "vm_reset" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                  "vm_run" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                  "vm_status" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[0],       0,                  "vm_run_time" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                  "vm_peak_cycles" },
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  &vm_timing_status,     0,                  "vm_timing" },

    #if VM_MAX_VMS >= 2
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[1],          0,                  "vm_reset_1" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[1],            0,                  "vm_run_1" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_1" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[1],         0,                  "vm_status_1" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[1],       0,                  "vm_run_time_1" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[1],     0,                  "vm_peak_cycles_1" },
    #endif

    #if VM_MAX_VMS >= 3
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[2],          0,                  "vm_reset_2" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[2],            0,                  "vm_run_2" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_2" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[2],         0,                  "vm_status_2" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[2],       0,                  "vm_run_time_2" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[2],     0,                  "vm_peak_cycles_2" },
    #endif

    #if VM_MAX_VMS >= 4
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[3],          0,                  "vm_reset_3" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[3],            0,                  "vm_run_3" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_3" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[3],         0,                  "vm_status_3" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[3],       0,                  "vm_run_time_3" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[3],     0,                  "vm_peak_cycles_3" },
    #endif

    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  0,                     vm_i8_kv_handler,   "vm_isa" },
};

static const char* vm_names[VM_MAX_VMS] = {
    "vm_0",

    #if VM_MAX_VMS >= 2
    "vm_1",
    #endif

    #if VM_MAX_VMS >= 3
    "vm_2",
    #endif

    #if VM_MAX_VMS >= 4
    "vm_3",
    #endif
};

// keys that we really don't want the VM be to be able to write to.
// generally, these are going to be things that would allow it to 
// brick hardware, mess up the wifi connection, or mess up the pixel 
// array.
static const PROGMEM uint32_t restricted_keys[] = {
    __KV__reboot,
    __KV__wifi_enable_ap,
    __KV__wifi_router,
    __KV__pix_clock,
    __KV__pix_count,
    __KV__pix_mode,    
};

#ifdef ENABLE_CATBUS_LINK
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
#endif

static void reset_published_data( uint8_t vm_id ){

    kvdb_v_clear_tag( 0, 1 << vm_id );

    #ifdef ENABLE_CATBUS_LINK
    link_v_delete_by_tag( get_link_tag( vm_id ) );
    #endif
} 

static int8_t get_program_fname( uint8_t vm_id, char name[FFS_FILENAME_LEN] ){

    catbus_hash_t32 hash;

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

        hash = 0;

        ASSERT( FALSE );
    }    

    return kv_i8_get( hash, name, FFS_FILENAME_LEN );
}


static bool is_vm_running( uint8_t vm_id ){

    return ( vm_status[vm_id] >= VM_STATUS_OK ) && ( vm_status[vm_id] != VM_STATUS_HALT );
}


static int8_t load_vm( uint8_t vm_id, char *program_fname, mem_handle_t *handle ){

    uint32_t start_time = tmr_u32_get_system_time_ms();
    
    *handle = -1;
    

    // open file
    file_t f = fs_f_open( program_fname, FS_MODE_READ_ONLY );

    if( f < 0 ){

        // try again, adding .fxb extension
        char s[FFS_FILENAME_LEN];
        memset( s, 0, sizeof(s) );
        strlcpy( s, program_fname, sizeof(s) );
        strlcat( s, ".fxb", sizeof(s) );

        f = fs_f_open( s, FS_MODE_READ_ONLY );

        if( f < 0 ){

            log_v_debug_P( PSTR("VM file not found") );

            return -1;
        }
    }

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

        uint8_t chunk[512];

        uint16_t copy_len = sizeof(chunk);

        if( copy_len > check_len ){

            copy_len = check_len;
        }

        int16_t read = fs_i16_read( f, chunk, copy_len );

        if( read < 0 ){

            // this should not happen. famous last words.
            goto error;
        }

        // update hash
        computed_file_hash = hash_u32_partial( computed_file_hash, chunk, copy_len );
        
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

    // allocate memory
    *handle = mem2_h_alloc2( vm_size, MEM_TYPE_VM_DATA );

    if( *handle < 0 ){

        goto error;
    }

    // read file
    int16_t read = fs_i16_read( f, mem2_vp_get_ptr( *handle ), vm_size );

    if( read < 0 ){

        // this should not happen. famous last words.
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
    
    // set up additional DB entries
    fs_v_seek( f, sizeof(vm_size) + state.db_start );

    for( uint8_t i = 0; i < state.db_count; i++ ){

        fs_i16_read( f, (uint8_t *)&meta, sizeof(meta) );

        kvdb_i8_add( meta.hash, meta.type, meta.count + 1, 0, 0 );
        kvdb_v_set_tag( meta.hash, 1 << vm_id );      
    }   


    // read through database keys
    uint32_t read_key_hash = 0;

    fs_v_seek( f, sizeof(vm_size) + state.read_keys_start );

    for( uint16_t i = 0; i < state.read_keys_count; i++ ){

        fs_i16_read( f, (uint8_t *)&read_key_hash, sizeof(uint32_t) );
    }
    

    // check published keys and add to DB
    fs_v_seek( f, sizeof(vm_size) + state.publish_start );

    for( uint8_t i = 0; i < state.publish_count; i++ ){

        vm_publish_t publish;

        fs_i16_read( f, (uint8_t *)&publish, sizeof(publish) );

        kvdb_i8_add( publish.hash, publish.type, 1, 0, 0 );
        kvdb_v_set_tag( publish.hash, ( 1 << vm_id ) );
    }   

    // check write keys
    fs_v_seek( f, sizeof(vm_size) + state.write_keys_start );

    for( uint8_t i = 0; i < state.write_keys_count; i++ ){

        uint32_t write_hash = 0;
        fs_i16_read( f, (uint8_t *)&write_hash, sizeof(write_hash) );

        if( write_hash == 0 ){

            continue;
        }

        // check if writing to restricted key
        for( uint8_t j = 0; j < cnt_of_array(restricted_keys); j++ ){

            uint32_t restricted_key = 0;
            memcpy( (uint8_t *)&restricted_key, &restricted_keys[j], sizeof(restricted_key) );

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
    #ifdef ENABLE_CATBUS_LINK
    catbus_hash_t32 link_tag = get_link_tag( vm_id );
    #endif

    for( uint8_t i = 0; i < state.link_count; i++ ){

        link_t link;

        fs_i16_read( f, (uint8_t *)&link, sizeof(link) );

        #ifdef ENABLE_CATBUS_LINK
        link_l_create( 
            link.mode,
            link.source_key,
            link.dest_key,
            &link.query,
            link_tag,
            link.rate,
            link.aggregation,
            LINK_FILTER_OFF );   
        #endif         
    }

    // load cron jobs
    vm_cron_v_load( vm_id, &state, f );

    fs_f_close( f );

    vm_status[vm_id]        = VM_STATUS_READY;
    vm_run_time[vm_id]      = 0;
    vm_max_cycles[vm_id]    = 0;

    log_v_debug_P( PSTR("VM loaded in: %lu ms"), tmr_u32_elapsed_time_ms( start_time ) );

    return 0;

error:
    
    if( *handle > 0 ){

        mem2_v_free( *handle );
    }

    fs_f_close( f );
    return -1;
}


typedef struct{
    uint8_t vm_id;
    char program_fname[FFS_FILENAME_LEN];
    mem_handle_t handle;
    int8_t vm_return;
    uint32_t last_run;
    int32_t delay_adjust;
    int32_t vm_delay;
    vm_state_t vm_state;
} vm_thread_state_t;

#ifdef ENABLE_TIME_SYNC
static uint32_t vm0_sync_ts;
static uint64_t vm0_sync_ticks;
static uint32_t vm0_checkpoint;
static uint32_t vm0_checkpoint_hash;

void vm_v_sync( uint32_t ts, uint64_t ticks ){

    // log_v_debug_P( PSTR("vm sync: %u / %u -> %u"), ts, ts - vm0_sync_ts, (uint32_t)ticks );

    vm0_sync_ts = ts;
    vm0_sync_ticks = ticks;    
}

uint32_t vm_u32_get_sync_time( void ){

    return vm0_sync_ts;
}

uint64_t vm_u64_get_sync_tick( void ){

    return vm0_sync_ticks;
}

uint32_t vm_u32_get_checkpoint( void ){

    return vm0_checkpoint;
}

uint32_t vm_u32_get_checkpoint_hash( void ){

    return vm0_checkpoint_hash;
}

uint64_t vm_u64_get_tick( void ){

    vm_state_t *state = vm_p_get_state();

    if( state == 0 ){

        return 0;
    }

    return state->tick;
}

uint64_t vm_u64_get_frame( void ){

    vm_state_t *state = vm_p_get_state();

    if( state == 0 ){

        return 0;
    }

    return state->frame_number;
}

uint32_t vm_u32_get_data_hash( void ){

    if( vm_threads[0] <= 0 ){

        return 0;
    }

    vm_thread_state_t *state = thread_vp_get_data( vm_threads[0] );        

    uint8_t *data = (uint8_t *)vm_i32p_get_data_ptr( mem2_vp_get_ptr( state->handle ), &state->vm_state );

    return hash_u32_data( data, state->vm_state.data_len );
}

uint16_t vm_u16_get_data_len( void ){

    if( vm_threads[0] <= 0 ){

        return 0;
    }

    vm_thread_state_t *state = thread_vp_get_data( vm_threads[0] );    

    return state->vm_state.data_len;
}

int32_t* vm_i32p_get_data( void ){

    if( vm_threads[0] <= 0 ){

        return 0;
    }

    vm_thread_state_t *state = thread_vp_get_data( vm_threads[0] );

    return vm_i32p_get_data_ptr( mem2_vp_get_ptr( state->handle ), &state->vm_state );   
}


#endif


// return state for VM 0.
// Use with caution!
vm_state_t* vm_p_get_state( void ){

    if( vm_threads[0] <= 0 ){

        return 0;
    }

    vm_thread_state_t *state = thread_vp_get_data( vm_threads[0] );

    return &state->vm_state;
}


static void kill_vm( uint8_t vm_id ){

    vm_reset[vm_id] = FALSE;
    vm_status[vm_id] = VM_STATUS_NOT_RUNNING;

    if( vm_threads[vm_id] <= 0 ){

        return;
    }

    vm_thread_state_t *state = thread_vp_get_data( vm_threads[vm_id] );

    if( state->handle > 0 ){

        mem2_v_free( state->handle );
    }

    // reset VM data
    reset_published_data( state->vm_id );

    // kill thread
    thread_v_kill( vm_threads[state->vm_id] );

    // clear thread handle
    vm_threads[state->vm_id] = -1;
}

PT_THREAD( vm_thread( pt_t *pt, vm_thread_state_t *state ) )
{
PT_BEGIN( pt );
    
    if( state->handle > 0 ){

        mem2_v_free( state->handle );
        state->handle = -1;
    }
    
    vm_run_time[state->vm_id]      = 0;
    vm_max_cycles[state->vm_id]    = 0;

    get_program_fname( state->vm_id, state->program_fname );

    log_v_debug_P( PSTR("Starting VM thread: %s"), state->program_fname );

    // reset VM data
    reset_published_data( state->vm_id );

    if( load_vm( state->vm_id, state->program_fname, &state->handle ) < 0 ){

        // error loading VM
        goto exit;        
    }

    state->vm_return = vm_i8_load_program( 0, mem2_vp_get_ptr( state->handle ), mem2_u16_get_size( state->handle ), &state->vm_state );

    if( state->vm_return ){

        log_v_debug_P( PSTR("VM load fail: %d"), state->vm_return );

        goto exit;
    }

    // init RNG seed to device ID
    uint64_t rng_seed;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &rng_seed );

    // make sure seed is never 0 (otherwise RNG will not work)
    if( rng_seed == 0 ){

        rng_seed = 1;
    }

    state->vm_state.rng_seed = rng_seed;

    // init database
    // vm_v_init_db( mem2_vp_get_ptr( state->handle ), &state->vm_state, 1 << state->vm_id );

    // run VM init
    state->vm_return = vm_i8_run_init( mem2_vp_get_ptr( state->handle ), &state->vm_state );

    if( state->vm_return ){

        log_v_debug_P( PSTR("VM init fail: %d"), state->vm_return );

        goto exit;
    }

    #ifdef ENABLE_TIME_SYNC
    // set initial checkpoint
    vm0_checkpoint = state->vm_state.frame_number;
    vm0_checkpoint_hash = vm_u32_get_data_hash();

    // log_v_debug_P( PSTR("checkpoint: %u -> %x"), (uint32_t)vm0_checkpoint, vm0_checkpoint_hash );
    #endif

    vm_status[state->vm_id] = VM_STATUS_OK;

    state->last_run = tmr_u32_get_system_time_ms();

    // main VM timing loop
    while( vm_status[state->vm_id] == VM_STATUS_OK ){

        state->delay_adjust = 0;
        
        #ifdef ENABLE_TIME_SYNC
        if( state->vm_id == 0 ){

            // check if syncing VM and hold if so
            if( vm_sync_b_in_progress() ){

                THREAD_WAIT_WHILE( pt, vm_sync_b_in_progress() );
                // our synced frame is already behind, so instead of computing a delay,
                // we will run immediately.
            }
        }
        #endif

        uint64_t next_tick = vm_u64_get_next_tick( mem2_vp_get_ptr( state->handle ), &state->vm_state );
        state->vm_delay = (int64_t)next_tick - (int64_t)state->vm_state.tick;

        // if this is the first run, we will start with a short delay
        if( state->vm_state.tick == 0 ){

            state->vm_delay = 10;
        }
        // if delay is 0 (or less, so we're running behind) -
        // yield a couple of times so we don't starve the other threads.
        // we cannot guarantee VM timing with this load.
        else if( state->vm_delay <= 0 ){

            if( state->vm_id == 0 ){

                if( vm_timing_status < 255 ){

                   vm_timing_status++;
                }
            }

            THREAD_YIELD( pt );
            THREAD_YIELD( pt );
            THREAD_YIELD( pt );
            THREAD_YIELD( pt );
        }        
        // VM sync stuff, only for VM 0
        else if( state->vm_id == 0 ){
            #ifdef ENABLE_TIME_SYNC
            // check if vm is a synced follower
            if( vm_sync_b_is_follower() ){

                uint32_t net_time = time_u32_get_network_time();
                int32_t elapsed = (int64_t)net_time - (int64_t)vm0_sync_ts;
                uint64_t current_vm_net_tick = vm0_sync_ticks + elapsed;
                int32_t sync_delta = state->vm_state.tick - current_vm_net_tick;

                state->delay_adjust = 0;


                if( ( sync_delta > 4000 ) || ( sync_delta < -4000 ) ){

                    log_v_debug_P( PSTR("lost sync: %d resetting %u %d %u %u %u"), sync_delta, net_time, elapsed, (uint32_t)current_vm_net_tick, vm0_sync_ts, vm0_sync_ticks );        

                    vm_sync_v_reset();
                }
                else if( sync_delta > 100 ){

                    state->delay_adjust = -100;
                }
                else if( sync_delta > 10 ){

                    state->delay_adjust = -10;
                }
                else if( sync_delta > 1 ){

                    state->delay_adjust = -1;
                }
                else if( sync_delta < -100 ){

                    state->delay_adjust = 100;
                }
                else if( sync_delta < -10 ){

                    state->delay_adjust = 10;
                }
                else if( sync_delta < -1 ){

                    state->delay_adjust = 1;
                }

                if( state->delay_adjust != 0 ){

                    // log_v_debug_P( PSTR("%d -> %d"), sync_delta, state->delay_adjust );        
                }
            }
            #endif
        }

        if( state->vm_id == 0 ){

            if( vm_timing_status > 0 ){

                vm_timing_status--;
            }
        }


        // set alarm

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + state->vm_delay );
      
        // wait, along with a check for an update frame rate
        THREAD_WAIT_WHILE( pt, 
            ( ( vm_run_flags[state->vm_id] & VM_FLAG_UPDATE_FRAME_RATE ) == 0 ) && 
            ( vm_status[state->vm_id] == VM_STATUS_OK ) &&
            thread_b_alarm_set() );

        // check if VM is stopping
        if( vm_status[state->vm_id] != VM_STATUS_OK ){

            goto exit;
        }

        #ifdef ENABLE_TIME_SYNC
        // check if syncing
        if( ( state->vm_id == 0 ) && vm_sync_b_in_progress() ){

            // go back to top of loop so we wait for the sync
            // if we ran now we could corrupt the VM data.

            continue;
        }
        #endif

        if( ( vm_run_flags[state->vm_id] & VM_FLAG_UPDATE_FRAME_RATE ) != 0 ){

            // frame rate update

            // just let the VM loop run.  The timing for this frame will be off, but
            // the next one will be correct.
            // This is will screw up the VM time sync across nodes, they will
            // have to resynchronize.
            state->vm_state.loop_tick = state->vm_state.tick;
        } 
    
        // get elapsed time between last run
        uint32_t delay = tmr_u32_get_system_time_ms() - state->last_run + state->delay_adjust;

        // run VM
        state->vm_return = vm_i8_run_tick( mem2_vp_get_ptr( state->handle ), &state->vm_state, delay );

        #ifdef ENABLE_TIME_SYNC
        if( ( state->vm_id == 0 ) && ( vm_sync_b_is_leader() ) ){

            // record network timestamp and current VM tick
            vm0_sync_ts = time_u32_get_network_time();
            vm0_sync_ticks = state->vm_state.tick;   
        }

        if( ( state->vm_id == 0 ) && ( vm_sync_b_is_synced() ) ){

            // check if it is time for a checkpoint
            if( ( state->vm_state.frame_number % SYNC_CHECKPOINT ) == 0 ){

                vm0_checkpoint = state->vm_state.frame_number;
                vm0_checkpoint_hash = vm_u32_get_data_hash();

                // log_v_debug_P( PSTR("checkpoint: %u -> %x"), (uint32_t)vm0_checkpoint, vm0_checkpoint_hash );
            }
        }

        #endif
        
        // update timestamp
        state->last_run = tmr_u32_get_system_time_ms();

        // update timing
        uint32_t elapsed_us = state->vm_state.last_elapsed_us;

        if( elapsed_us > 65535 ){

            elapsed_us = 65535;
        }

        if( vm_run_time[state->vm_id] == 0 ){

            vm_run_time[state->vm_id] = elapsed_us;
        }
        else{

            vm_run_time[state->vm_id] = util_u16_ewma( elapsed_us, vm_run_time[state->vm_id], 16 );    
        }

        // log_v_debug_P( PSTR("net: %d delay: %d tick: %d next: %d cur: %d loop: %d thread: %d thread2: %d"), 
        //     time_u32_get_network_time(), 
        //     delay,
        //     (int32_t)state->vm_state.tick, 
        //     (int32_t)vm_u64_get_next_tick( mem2_vp_get_ptr( state->handle ), &state->vm_state ), 
        //     vm_i32_get_data( mem2_vp_get_ptr( state->handle ), &state->vm_state, 25 ),
        //     (int32_t)state->vm_state.loop_tick, 
        //     (int32_t)state->vm_state.threads[0].tick, 
        //     (int32_t)state->vm_state.threads[1].tick );

        // if( ( state->vm_id == 0 ) && ( state->vm_return == VM_STATUS_OK ) ){

        //     log_v_debug_P( PSTR("%d l:%d 1:%d 2:%d 3:%d 4:%d r: %x"), 
        //         (int32_t)state->vm_state.tick, 
        //         (int32_t)state->vm_state.loop_tick, 
        //         (int32_t)state->vm_state.threads[1].tick,
        //         (int32_t)state->vm_state.threads[2].tick,
        //         (int32_t)state->vm_state.threads[3].tick,
        //         (int32_t)state->vm_state.threads[4].tick, 
        //         (uint32_t)state->vm_state.rng_seed );
        // }

        // clear all run flags
        vm_run_flags[state->vm_id] = 0;        

        // update cycle count
        vm_max_cycles[state->vm_id] = state->vm_state.max_cycles;

        // check status
        if( state->vm_return == VM_STATUS_HALT ){

            vm_status[state->vm_id] = VM_STATUS_HALT;
            trace_printf( "VM %d halted\r\n", state->vm_id );
        }
        else if( state->vm_return < 0 ){

            vm_status[state->vm_id] = state->vm_return;

            trace_printf( "VM %d error: %d\r\n", state->vm_id, state->vm_return );
            goto exit;
        }
    }
    
exit:
    trace_printf( "Stopping VM thread: %s\r\n", state->program_fname );
    
    kill_vm( state->vm_id );
    
PT_END( pt );
}

static bool vm_loader_wait( void ){

    for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

        if( ( ( !vm_run[i]  && !is_vm_running( i ) )  ||
                ( vm_run[i]   && is_vm_running( i ) ) )    &&
              ( !vm_reset[i] ) ){
        }
        else{

            return FALSE;
        }
    }

    return TRUE;
}

static void reset_vm( uint8_t vm_id ){

    vm_status[vm_id] = VM_STATUS_NOT_RUNNING;

    // verify thread exists
    if( vm_threads[vm_id] > 0 ){

        thread_v_restart( vm_threads[vm_id] );
    }   
}

static int8_t start_vm( uint8_t vm_id ){

    if( vm_threads[vm_id] > 0 ){

        // already running
        return 0;
    }

    if( is_vm_running( vm_id ) ){

        return 0;
    }

    // must set vm_run externally!
    if( !vm_run[vm_id] ){

        // not set to run state:
        return -2;
    }

    vm_thread_state_t thread_state;
    memset( &thread_state, 0, sizeof(thread_state) );
    thread_state.vm_id = vm_id;

    vm_threads[vm_id] = thread_t_create( THREAD_CAST(vm_thread),
                                     vm_names[vm_id],
                                     &thread_state,
                                     sizeof(thread_state) );

    if( vm_threads[vm_id] < 0 ){

        vm_run[vm_id] = FALSE;

        log_v_debug_P( PSTR("VM start thread failed: %d"), vm_id );

        return -1;
    }

    vm_status[vm_id] = VM_STATUS_OK;   

    return 0;
}

static void stop_vm( uint8_t vm_id ){

    kill_vm( vm_id );

    vm_run[vm_id] = FALSE;

    vm_run_time[vm_id]      = 0;
    vm_max_cycles[vm_id]    = 0;
}


PT_THREAD( vm_loader( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    kvdb_v_set_name_P( PSTR("vm_0") );
    kvdb_v_set_name_P( PSTR("vm_1") );
    kvdb_v_set_name_P( PSTR("vm_2") );
    kvdb_v_set_name_P( PSTR("vm_3") );

    // alignment check
    vm_state_t temp;
    if( ( (uint32_t)&temp.rng_seed % 4 ) != 0 ){

        log_v_critical_P( PSTR("VM state alignment failure!") );

        THREAD_EXIT( pt );
    }

    TMR_WAIT( pt, 100 ); // delay so pixel drivers have a chance to zero out the array

    while(1){

        THREAD_WAIT_WHILE( pt, vm_loader_wait() );

        // check what we're doing, and to what VM    
        for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

            // Was there an error and the VM is running
            if( ( vm_run[i] ) &&
                ( vm_status[i] != VM_STATUS_NOT_RUNNING ) &&
                ( vm_status[i] != VM_STATUS_READY ) &&
                ( vm_status[i] != 0 ) ){

                vm_run[i] = FALSE;

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

                reset_vm( i );
            }

            // Did VM that was not running just get told to start?
            // This will also occur if we've triggered a reset
            if( vm_run[i] && !is_vm_running( i ) && ( vm_threads[i] <= 0 ) ){

                if( start_vm( i ) < 0 ){

                    // this means a thread creation failed.

                    // generally, the system is pretty screwed if that
                    // happens.
                    // rebooting into safe mode is probably the best option:
                    sys_v_reboot_delay( SYS_MODE_SAFE );
                }
            }
            // Did VM that was running just get told to stop?
            else if( !vm_run[i] && is_vm_running( i ) ){

                log_v_debug_P( PSTR("Stopping VM: %d"), i );
                
                stop_vm( i );
            }
            
            // always reset the reset
            vm_reset[i] = FALSE;
        }

        // TMR_WAIT( pt, 100 );
        THREAD_YIELD( pt );
    }

PT_END( pt );
}


void vm_v_start( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    vm_run[vm_id] = TRUE;
}

void vm_v_stop( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    stop_vm( vm_id );
}

void vm_v_reset( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    reset_vm( vm_id );
}

void vm_v_run_prog( char name[FFS_FILENAME_LEN], uint8_t vm_id ){

    catbus_hash_t32 hash;

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

        hash = 0;

        ASSERT( FALSE );
    }    

    // set full string in KV, with 0 padding
    char prog[FFS_FILENAME_LEN];
    memset( prog, 0, sizeof(prog) );
    strncpy( prog, name, sizeof(prog) );

    kv_i8_set( hash, prog, FFS_FILENAME_LEN );

    log_v_info_P( PSTR("Starting %s on VM: %d"), prog, vm_id );

    vm_run[vm_id] = TRUE;
    vm_reset[vm_id] = TRUE;
}

bool vm_b_running( void ){

    for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

        if( is_vm_running( i ) ){

            return TRUE;
        }
    }

    return FALSE;
}

bool vm_b_is_vm_running( uint8_t i ){

    ASSERT( i < VM_MAX_VMS );

    return is_vm_running( i );   
}

void gfx_vm_v_update_frame_rate( uint16_t new_frame_rate ){

    for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

        vm_run_flags[i] |= VM_FLAG_UPDATE_FRAME_RATE;
    }
}

int8_t vm_cron_i8_run_func( uint8_t i, uint16_t func_addr ){

    if( vm_threads[i] <= 0 ){
        
        return VM_STATUS_ERROR;        
    }

    vm_thread_state_t *thread_state = thread_vp_get_data( vm_threads[i] );

    return vm_i8_run( mem2_vp_get_ptr( thread_state->handle ), func_addr, 0, &thread_state->vm_state );
}

#endif

void vm_v_init( void ){

    #ifdef ENABLE_GFX

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    COMPILER_ASSERT( ( sizeof(vm_state_t) % 4 ) == 0 );

    memset( vm_status, VM_STATUS_NOT_RUNNING, sizeof(vm_status) );

    thread_t_create( vm_loader,
                     PSTR("vm_loader"),
                     0,
                     0 );

    vm_cron_v_init();
    #endif
}

