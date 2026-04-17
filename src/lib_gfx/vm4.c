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
#include "vm4.h"

#include "bytecode.h"
#include "pixelarray.h"
#include "sequencer.h"
#include "cron.h"

static bool vm_reset[VM4_MAX_VMS];
static bool vm_run[VM4_MAX_VMS];

static int8_t vm_status[VM4_MAX_VMS];
static uint16_t vm_run_time[VM4_MAX_VMS];
static uint16_t vm_max_cycles[VM4_MAX_VMS];

static thread_t vm_threads[VM4_MAX_VMS];

static bool is_vm_running( uint8_t vm_id ){

    return ( vm_status[vm_id] >= VM4_STATUS_OK ) && ( vm_status[vm_id] != VM4_STATUS_HALT );
}


static int8_t _vm4_prog_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__vm_prog ){

            vm4_v_reset( 0 );
        }
        #if VM4_MAX_VMS >= 2
        else if( hash == __KV__vm_prog_1 ){

            vm4_v_reset( 1 );
        }
        #endif
        #if VM4_MAX_VMS >= 3
        else if( hash == __KV__vm_prog_2 ){

            vm4_v_reset( 2 );
        }
        #endif
        #if VM4_MAX_VMS >= 4
        else if( hash == __KV__vm_prog_3 ){

            vm4_v_reset( 3 );
        }
        #endif
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

KV_SECTION_META kv_meta_t vm4_info_kv[] = {
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                   "vm4_reset" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                   "vm4_run" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                   "vm4_status" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[0],       0,                   "vm4_run_time" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                   "vm4_peak_cycles" },

    #if VM4_MAX_VMS >= 2
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[1],          0,                   "vm4_reset_1" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[1],            0,                   "vm4_run_1" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog_1" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[1],         0,                   "vm4_status_1" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[1],       0,                   "vm4_run_time_1" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[1],     0,                   "vm4_peak_cycles_1" },
    #endif

    #if VM4_MAX_VMS >= 3
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[2],          0,                   "vm4_reset_2" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[2],            0,                   "vm4_run_2" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog_2" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[2],         0,                   "vm4_status_2" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[2],       0,                   "vm4_run_time_2" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[2],     0,                   "vm4_peak_cycles_2" },
    #endif

    #if VM4_MAX_VMS >= 4
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[3],          0,                   "vm4_reset_3" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[3],            0,                   "vm4_run_3" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog_3" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[3],         0,                   "vm4_status_3" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[3],       0,                   "vm4_run_time_3" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[3],     0,                   "vm4_peak_cycles_3" },
    #endif
};

static const char* vm_names[VM4_MAX_VMS] = {
    "vm4_0",

    #if VM4_MAX_VMS >= 2
    "vm4_1",
    #endif
    #if VM4_MAX_VMS >= 3
    "vm4_2",
    #endif
    #if VM4_MAX_VMS >= 4
    "vm4_3",
    #endif
};

typedef struct __attribute__((packed)){
    uint32_t hash;
    uint16_t index;
    uint16_t count;
} published_var_t;

typedef struct __attribute__((packed)){
    vm_t vm;
    uint8_t vm_id;

    published_var_t published_vars[8];

    // mem_handle_t handle;
    // char program_fname[FFS_FILENAME_LEN];
    
    // int8_t vm_return;
    // uint32_t last_run;
    // int32_t delay_adjust;
    // int32_t vm_delay;
    // vm_state_t vm_state;
} vm4_thread_state_t;


PT_THREAD( vm4_thread( pt_t *pt, vm4_thread_state_t *state ) );
PT_THREAD( vm4_loader( pt_t *pt, void *state ) );

static int8_t get_program_fname( uint8_t vm_id, char name[FFS_FILENAME_LEN] ){

    catbus_hash_t32 hash;

    if( vm_id == 0 ){

        hash = __KV__vm4_prog;
    }
    else if( vm_id == 1 ){

        hash = __KV__vm4_prog_1;
    }
    else if( vm_id == 2 ){

        hash = __KV__vm4_prog_2;
    }
    else if( vm_id == 3 ){

        hash = __KV__vm4_prog_3;
    }
    else{

        hash = 0;

        ASSERT( FALSE );
    }    

    return kv_i8_get( hash, name, FFS_FILENAME_LEN );
}


void vm4_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    log_v_info_P( PSTR("FX4 init") );

    for( uint8_t i = 0; i < cnt_of_array(vm_status); i++ ){

        vm_status[i] = VM4_STATUS_NOT_RUNNING;
    }

    cron_v_init();
    pixelarray_init();

    seq_v_init();

    thread_t_create( vm4_loader,
                     PSTR("vm4_loader"),
                     0,
                     0 );
}

void vm4_v_reset( uint8_t vm_id ){

    ASSERT( vm_id < VM4_MAX_VMS );

    if( is_vm_running( vm_id ) ){

        vm_reset[vm_id] = TRUE;    
    }
}

void vm4_v_add_published_var( uint16_t index, catbus_hash_t32 hash, catbus_type_t8 type, uint16_t count, uint8_t flags, uint8_t vm_id ){

    if( ( count == 0 ) || ( count > 256 ) ){

        log_v_error_P( PSTR("invalid array count") );

        return;
    }

    vm4_thread_state_t *thread_state = thread_vp_get_data( vm_threads[vm_id] );

    // search for open slot
    int8_t slot = -1;
    for( uint8_t i = 0; i < cnt_of_array(thread_state->published_vars); i++ ){

        if( thread_state->published_vars[i].hash == 0 ){

            slot = i;

            break;
        }
    }

    if( slot < 0 ){

        log_v_error_P( PSTR("No slots") );

        return;
    }

    thread_state->published_vars[slot].hash    = hash;
    thread_state->published_vars[slot].index   = index;
    thread_state->published_vars[slot].count   = count;

    kvdb_i8_add( hash, type, count, 0, 0 );
    kvdb_v_set_tag( hash, ( 1 << vm_id ) );

    if( flags & KV_FLAGS_PERSIST ){

        kvdb_i8_set_persist( hash, TRUE );
    }
}


PT_THREAD( vm4_thread( pt_t *pt, vm4_thread_state_t *state ) )
{
PT_BEGIN( pt );
    
    vm_reset[state->vm_id] = FALSE;
    vm_status[state->vm_id] = VM4_STATUS_OK;

    memset( &state->vm, 0, sizeof(state->vm) );
    memset( &state->published_vars, 0, sizeof(state->published_vars) );

    char fname[FFS_FILENAME_LEN] = {0};

    get_program_fname( state->vm_id, fname );

    // load VM
    int status = vm_deserialize( &state->vm, fname );

    if( status < 0 ){

        log_v_error_P( PSTR("VM load failed: %d"), status );

        goto end;
    }
 
    // run top level VM script:    
    status = vm_run_instructions(&state->vm, -1);

    if( status < 0 ){

        log_v_error_P( PSTR("VM init failed: %d"), status );
        goto end;
    }

    log_v_debug_P( PSTR("VM init OK") );

    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while( 1 ){

        thread_v_set_alarm( thread_u32_get_alarm() + FADER_RATE );
        THREAD_WAIT_WHILE( pt, 
            thread_b_alarm_set() && 
            vm_run[state->vm_id] &&
            !vm_reset[state->vm_id] );

        // check if running
        if( !vm_run[state->vm_id] ){

            log_v_info_P( PSTR("VM stop requested") );

            goto end;
        }
        // check if resetting
        else if( vm_reset[state->vm_id] ){

            log_v_info_P( PSTR("VM reset") );

            goto end;
        }

        // load published vars
        for( uint8_t i = 0; i < cnt_of_array(state->published_vars); i++ ){

            if( state->published_vars[i].hash == 0 ){

                continue;
            }

            int32_t *ptr = 0;

            ASSERT( state->published_vars[i].count != 0 );

            if( state->published_vars[i].count == 1 ){

                ptr = vm_get_global( &state->vm, state->published_vars[i].index );
            }
            else{ // > 1

                ptr = vm_get_array( &state->vm, state->published_vars[i].index );
            }

            int8_t kv_status = catbus_i8_array_get( 
                                state->published_vars[i].hash,
                                CATBUS_TYPE_INT32,
                                0,
                                state->published_vars[i].count,
                                ptr );

            if( kv_status < 0 ){

                log_v_error_P( PSTR("KV error: %d"), kv_status );
            }
        }

        uint32_t start_time = tmr_u32_get_system_time_us();

        status = vm_run_tick( &state->vm, thread_u32_get_alarm() );

        uint32_t elapsed_us = tmr_u32_elapsed_time_us( start_time );

        if( status < 0 ){

            log_v_error_P( PSTR("VM error: %d"), status );
            goto end;
        }
        else if( status == VM4_STATUS_NO_COROUTINE ){

            log_v_info_P( PSTR("VM finished") );

            goto end;
        }
        else if( status == VM4_STATUS_NO_READY_COROUTINE ){

        }
        else{

            vm_run_time[state->vm_id] = elapsed_us;
        }

        if( state->vm.cycle_count > vm_max_cycles[state->vm_id] ){

            vm_max_cycles[state->vm_id] = state->vm.cycle_count;
        }

        for( uint8_t i = 0; i < cnt_of_array(state->published_vars); i++ ){

            if( state->published_vars[i].hash == 0 ){

                continue;
            }

            ASSERT( state->published_vars[i].count != 0 );

            int32_t *ptr = 0;

            if( state->published_vars[i].count == 1 ){

                ptr = vm_get_global( &state->vm, state->published_vars[i].index );
            }
            else{ // > 1

                ptr = vm_get_array( &state->vm, state->published_vars[i].index );
            }

            int8_t kv_status = catbus_i8_array_set( 
                                state->published_vars[i].hash,
                                CATBUS_TYPE_INT32,
                                0,
                                state->published_vars[i].count,
                                ptr,
                                sizeof(int32_t) * state->published_vars[i].count );

            if( kv_status < 0 ){

                log_v_error_P( PSTR("KV error: %d"), kv_status );
            }
        }

        THREAD_YIELD( pt );
    }


end:
    vm_deinit( &state->vm );

    kvdb_v_clear_tag( 0, 1 << state->vm_id );

    cron_v_unload( state->vm_id );

    vm_run_time[state->vm_id]   = 0;
    vm_max_cycles[state->vm_id] = 0;

    if( vm_reset[state->vm_id] && vm_run[state->vm_id] ){

        vm_reset[state->vm_id] = FALSE;

        THREAD_RESTART( pt );
    }

    // retain halt status, otherwise reset
    if( vm_status[state->vm_id] != VM4_STATUS_HALT ){

        vm_status[state->vm_id] = VM4_STATUS_NOT_RUNNING;    
    }    

    vm_reset[state->vm_id]      = FALSE;
    vm_run[state->vm_id]        = FALSE;

    vm_threads[state->vm_id]    = -1;

    log_v_info_P( PSTR("VM stop") );
    
PT_END( pt );
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

    vm_threads[vm_id] = thread_t_create( THREAD_CAST(vm4_thread),
                                         vm_names[vm_id],
                                         0,
                                         sizeof(vm4_thread_state_t) );

    if( vm_threads[vm_id] < 0 ){

        vm_run[vm_id] = FALSE;

        log_v_debug_P( PSTR("VM start thread failed: %d"), vm_id );

        return -1;
    }

    // init VM thread data
    vm4_thread_state_t *thread_state = thread_vp_get_data( vm_threads[vm_id] );
    memset( thread_state, 0, sizeof(vm4_thread_state_t) );

    thread_state->vm_id = vm_id;

    vm_status[vm_id] = VM4_STATUS_OK;   

    return 0;
}

static bool vm_loader_wait( void ){

    for( uint8_t i = 0; i < VM4_MAX_VMS; i++ ){

        if( is_vm_running( i ) ){

            // VM is already running
            continue;
        }

        // if check VM should start
        if( vm_run[i] ){

            return FALSE; // stop waiting
        }
    }

    return TRUE; // continue waiting
}


PT_THREAD( vm4_loader( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    kvdb_v_set_name_P( PSTR("vm4_0") );
    kvdb_v_set_name_P( PSTR("vm4_1") );
    kvdb_v_set_name_P( PSTR("vm4_2") );
    kvdb_v_set_name_P( PSTR("vm4_3") );

    TMR_WAIT( pt, 100 ); // delay so pixel drivers have a chance to zero out the array

    while(1){

        THREAD_WAIT_WHILE( pt, vm_loader_wait() );

        // check what we're doing, and to what VM    
        for( uint8_t i = 0; i < VM4_MAX_VMS; i++ ){

            // Was there an error and the VM is running
            if( ( vm_run[i] ) &&
                ( vm_status[i] != VM4_STATUS_NOT_RUNNING ) &&
                ( vm_status[i] != 0 ) ){

                vm_run[i] = FALSE;

                if( vm_status[i] == VM4_STATUS_HALT ){

                    // this isn't actually an error, it is the VM
                    // signalling the script has requested a stop.
                    // log_v_debug_P( PSTR("VM %d halted"), i );
                }
                else{

                    log_v_debug_P( PSTR("VM %d error: %d"), i, vm_status[i] );
                }
            }

            // Did VM that was not running just get told to start?
            // This will also occur if we've triggered a reset
            if( vm_run[i] && !is_vm_running( i ) && ( vm_threads[i] <= 0 ) ){

                if( start_vm( i ) < 0 ){

                    log_v_error_P( PSTR("Thread fail") );

                    // this means a thread creation failed.

                    // generally, the system is pretty screwed if that
                    // happens.
                    // rebooting into safe mode is probably the best option:
                    sys_v_reboot_delay( SYS_MODE_SAFE );
                }
            }
        }
    }

PT_END( pt );
}


void vm4_v_run_prog( char name[FFS_FILENAME_LEN], uint8_t vm_id ){

    catbus_hash_t32 hash;

    if( vm_id == 0 ){

        hash = __KV__vm4_prog;
    }
    else if( vm_id == 1 ){

        hash = __KV__vm4_prog_1;
    }
    else if( vm_id == 2 ){

        hash = __KV__vm4_prog_2;
    }
    else if( vm_id == 3 ){

        hash = __KV__vm4_prog_3;
    }
    else{

        hash = 0;

        ASSERT( FALSE );
    }    

    // set full string in KV, with 0 padding
    char prog[FFS_FILENAME_LEN];
    memset( prog, 0, sizeof(prog) );
    strncpy( prog, name, sizeof(prog) );

    // set run
    vm_run[vm_id] = TRUE;
    // set reset, see below
    vm_reset[vm_id] = TRUE;    

    // this should also set reset via the kv handler,
    // but we will explicitly set it above with run
    // just be to clear this is what is happening.
    kv_i8_set( hash, prog, FFS_FILENAME_LEN );
}

bool vm4_b_is_vm_running( uint8_t vm_id ){

    ASSERT( vm_id < VM4_MAX_VMS );

    return is_vm_running( vm_id );
}

int8_t vm4_i8_run_coroutine( uint16_t addr, uint8_t vm_id ){

    ASSERT( vm_id < VM4_MAX_VMS );

    if( vm_threads[vm_id] < 0 ){

        return VM4_STATUS_NOT_RUNNING;
    }

    vm4_thread_state_t *thread_state = thread_vp_get_data( vm_threads[vm_id] );

    vm_run_coroutine( &thread_state->vm, addr );

    return VM4_STATUS_OK;
}