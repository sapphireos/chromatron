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
#include "cron.h"

static bool vm_reset[VM_MAX_VMS];
static bool vm_run[VM_MAX_VMS];

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_run_time[VM_MAX_VMS];
static uint16_t vm_max_cycles[VM_MAX_VMS];
static uint16_t vm_ready_time;

static thread_t vm_threads[VM_MAX_VMS];

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
        #if VM_MAX_VMS >= 2
        else if( hash == __KV__vm_prog_1 ){

            vm4_v_reset( 1 );
        }
        #endif
        #if VM_MAX_VMS >= 3
        else if( hash == __KV__vm_prog_2 ){

            vm4_v_reset( 2 );
        }
        #if VM_MAX_VMS >= 4
        else if( hash == __KV__vm_prog_3 ){

            vm4_v_reset( 3 );
        }
        #endif
        #endif
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

static uint16_t run_ticks;

KV_SECTION_META kv_meta_t vm4_info_kv[] = {
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                  "vm4_reset" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                  "vm4_run" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                  "vm4_status" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[0],       0,                  "vm4_run_time" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                  "vm4_peak_cycles" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_ready_time,     0,                  "vm4_ready_time" },

    { CATBUS_TYPE_UINT16,   0, 0,  &run_ticks,     0,                  "vm4_run_ticks" },
};

static const char* vm_names[VM_MAX_VMS] = {
    "vm4_0",

    #if VM_MAX_VMS >= 2
    "vm4_1",
    #endif

    #if VM_MAX_VMS >= 3
    "vm4_2",
    #endif

    #if VM_MAX_VMS >= 4
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

void vm4_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    log_v_info_P( PSTR("FX4 init") );

    for( uint8_t i = 0; i < cnt_of_array(vm_status); i++ ){

        vm_status[i] = VM_STATUS_NOT_RUNNING;
    }

    cron_v_init();
    pixelarray_init();

    thread_t_create( vm4_loader,
                     PSTR("vm4_loader"),
                     0,
                     0 );
}

void vm4_v_reset( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    // reset_vm( vm_id );
}


void vm4_v_add_published_var( uint16_t index, catbus_hash_t32 hash, catbus_type_t8 type, uint16_t count, uint8_t flags, uint8_t vm_id ){

    // if( vm_id == 0 ){

    //     if( index < cnt_of_array(vm_published_hash) ){

    //         vm_published_hash[index] = hash;
    //     }
    // }

    vm4_thread_state_t *thread_state = thread_vp_get_data( vm_threads[vm_id] );

    thread_state->published_vars[0].hash    = hash;
    thread_state->published_vars[0].index   = index;
    thread_state->published_vars[0].count   = count;

    if( ( count == 0 ) || ( count > 256 ) ){

        log_v_error_P( PSTR("invalid array count") );
    }

    kvdb_i8_add( hash, type, count, 0, 0 );
    kvdb_v_set_tag( hash, ( 1 << vm_id ) );

    if( flags & KV_FLAGS_PERSIST ){

        kvdb_i8_set_persist( hash, TRUE );
    }
}


PT_THREAD( vm4_thread( pt_t *pt, vm4_thread_state_t *state ) )
{
PT_BEGIN( pt );

    // load VM
    int status = vm_deserialize( &state->vm, PSTR("vm.f4b") );

    if( status < 0 ){

        goto end;
    }

restart:    
    // run top level VM script:    
    log_v_info_P( PSTR("VM start") );

    status = vm_run_instructions(&state->vm, -1);

    if( status < 0 ){

        log_v_error_P( PSTR("VM init failed: %d"), status );
        goto end;
    }


    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while( 1 ){

        thread_v_set_alarm( thread_u32_get_alarm() + 20 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // check if running
        if( !vm_run[state->vm_id] ){

            break;
        }

        // check if resetting
        if( vm_reset[state->vm_id] ){

            log_v_info_P( PSTR("VM reset") );

            goto restart;
        }

        uint32_t start_time = tmr_u32_get_system_time_us();

        status = vm_run_tick( &state->vm, thread_u32_get_alarm() );

        uint32_t elapsed_us = tmr_u32_elapsed_time_us( start_time );

        if( status < 0 ){

            log_v_error_P( PSTR("VM error: %d"), status );
            goto end;
        }
        else if( status == VM_STATUS_NO_COROUTINE ){

            log_v_info_P( PSTR("VM finished") );

            goto end;
        }
        else if( status == VM_STATUS_NO_READY_COROUTINE ){

            vm_ready_time = elapsed_us;
        }
        else{

            vm_run_time[0] = elapsed_us;
        }

        if( state->vm.cycle_count > vm_max_cycles[0] ){

            vm_max_cycles[0] = state->vm.cycle_count;
        }

        if( state->published_vars[0].hash != 0 ){

            int32_t *ptr = 0;

            if( state->published_vars[0].count == 1 ){

                ptr = vm_get_global( &state->vm, state->published_vars[0].index );
            }
            else if( state->published_vars[0].count > 1 ){

                ptr = vm_get_array( &state->vm, state->published_vars[0].index );
            }

            int8_t kv_status = catbus_i8_array_set( 
                                state->published_vars[0].hash,
                                CATBUS_TYPE_INT32,
                                0,
                                state->published_vars[0].count,
                                ptr,
                                sizeof(int32_t) * state->published_vars[0].count );

            if( kv_status < 0 ){

                log_v_error_P( PSTR("KV error: %d"), kv_status );
            }

        }

        THREAD_YIELD( pt );
    }


end:
    vm_deinit( &state->vm );

    kvdb_v_clear_tag( 0, 1 << state->vm_id );

    // retain halt status, otherwise reset
    if( vm_status[state->vm_id] != VM_STATUS_HALT ){

        vm_status[state->vm_id] = VM_STATUS_NOT_RUNNING;    
    }

    vm_run[state->vm_id]        = FALSE;
    vm_reset[state->vm_id]      = FALSE;

    vm_run_time[state->vm_id]   = 0;
    vm_max_cycles[state->vm_id] = 0;

    vm_threads[state->vm_id]    = -1;

    log_v_info_P( PSTR("VM stop") );
    
PT_END( pt );
}

static bool is_vm_running( uint8_t vm_id ){

    return ( vm_status[vm_id] >= VM_STATUS_OK ) && ( vm_status[vm_id] != VM_STATUS_HALT );
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
    thread_state->vm_id = vm_id;

    memset( &thread_state->vm, 0, sizeof(thread_state->vm) );


    vm_status[vm_id] = VM_STATUS_OK;   

    return 0;
}

// static void stop_vm( uint8_t vm_id ){

//     vm_run[vm_id] = FALSE;
//     vm_reset[vm_id] = FALSE;

//     vm_run_time[vm_id]      = 0;
//     vm_max_cycles[vm_id]    = 0;
// }

// static void reset_vm( uint8_t vm_id ){

//     vm_status[vm_id] = VM_STATUS_NOT_RUNNING;

//     // verify thread exists
//     if( vm_threads[vm_id] > 0 ){

//         // thread_v_restart( vm_threads[vm_id] );

//         stop_vm( vm_id );

//         vm_run[vm_id] = TRUE;
//         start_vm( vm_id );
//     }   
// }


// static bool vm_loader_wait( void ){

//     for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

//         if( ( ( !vm_run[i]  && !is_vm_running( i ) )  ||
//                 ( vm_run[i]   && is_vm_running( i ) ) )    &&
//               ( !vm_reset[i] ) ){
//         }
//         else{

//             return FALSE;
//         }
//     }

//     return TRUE;
// }


PT_THREAD( vm4_loader( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    kvdb_v_set_name_P( PSTR("vm4_0") );
    kvdb_v_set_name_P( PSTR("vm4_1") );
    kvdb_v_set_name_P( PSTR("vm4_2") );
    kvdb_v_set_name_P( PSTR("vm4_3") );

    TMR_WAIT( pt, 100 ); // delay so pixel drivers have a chance to zero out the array

    while(1){

        // THREAD_WAIT_WHILE( pt, vm_loader_wait() );
        TMR_WAIT( pt, 100 );

        // check what we're doing, and to what VM    
        for( uint8_t i = 0; i < VM_MAX_VMS; i++ ){

            // Was there an error and the VM is running
            if( ( vm_run[i] ) &&
                ( vm_status[i] != VM_STATUS_NOT_RUNNING ) &&
                ( vm_status[i] != 0 ) ){

                vm_run[i] = FALSE;

                if( vm_status[i] == VM_STATUS_HALT ){

                    // this isn't actually an error, it is the VM
                    // signalling the script has requested a stop.
                    // log_v_debug_P( PSTR("VM %d halted"), i );
                }
                else{

                    log_v_debug_P( PSTR("VM %d error: %d"), i, vm_status[i] );
                }
            }

            // Are we resetting a VM?
            // if( vm_reset[i] ){

            //     trace_printf( PSTR("Resetting VM: %d\r\n"), i );

            //     reset_vm( i );
            // }

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
            // // Did VM that was running just get told to stop?
            // else if( !vm_run[i] && is_vm_running( i ) ){

            //     trace_printf( PSTR("Stopping VM: %d\r\n"), i );
                
            //     stop_vm( i );
            // }
            
            // always reset the reset
            vm_reset[i] = FALSE;
        }

        // TMR_WAIT( pt, 100 );
        THREAD_YIELD( pt );
    }

PT_END( pt );
}