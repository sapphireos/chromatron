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

static bool vm_reset[VM_MAX_VMS];
static bool vm_run[VM_MAX_VMS];

static int8_t vm_status[VM_MAX_VMS];
static uint16_t vm_run_time[VM_MAX_VMS];
static uint16_t vm_max_cycles[VM_MAX_VMS];


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

KV_SECTION_META kv_meta_t vm4_info_kv[] = {
    { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                  "vm4_reset" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                  "vm4_run" },
    { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     _vm4_prog_kv_handler,"vm4_prog" },
    { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                  "vm4_status" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[0],       0,                  "vm4_run_time" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                  "vm4_peak_cycles" },
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
    vm_t vm;
    uint8_t vm_id;

    // mem_handle_t handle;
    // char program_fname[FFS_FILENAME_LEN];
    
    // int8_t vm_return;
    // uint32_t last_run;
    // int32_t delay_adjust;
    // int32_t vm_delay;
    // vm_state_t vm_state;
} vm4_thread_state_t;

PT_THREAD( vm4_thread( pt_t *pt, vm4_thread_state_t *state ) );


void vm4_v_init( void ){

    log_v_info_P( PSTR("FX4 init") );

    log_v_info_P( PSTR("%d"), sizeof(vm_t) );

    if(vm_run[0] == FALSE){

        return;
    }


    uint8_t vm_id = 0;
    thread_t t = thread_t_create( THREAD_CAST(vm4_thread),
                                              vm_names[vm_id],
                                              0,
                                              sizeof(vm4_thread_state_t) );

    if( t < 0 ){

        log_v_error_P( PSTR("failed to create thread") );

        return;
    }

    vm4_thread_state_t *thread_state = thread_vp_get_data( t );

    thread_state->vm_id = vm_id;

    memset( &thread_state->vm, 0, sizeof(thread_state->vm) );

    int status = vm_deserialize(&thread_state->vm, PSTR("vm.f4b") );
    log_v_info_P( PSTR("load status %d"), status );

    if( status < 0 ){

        thread_v_kill( t );
    }
}

void vm4_v_reset( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    // reset_vm( vm_id );
}


PT_THREAD( vm4_thread( pt_t *pt, vm4_thread_state_t *state ) )
{
PT_BEGIN( pt );
        
    // run top level VM script:
    {
        log_v_info_P( PSTR("VM start") );

        int status = vm_run_instructions(&state->vm, -1);

        if( status < 0 ){

            log_v_error_P( PSTR("VM init failed: %d"), status );
            goto end;
        }
    }

    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while( 1 ){

        thread_v_set_alarm( thread_u32_get_alarm() + 20 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        int status = vm_run_tick( &state->vm, tmr_u64_get_system_time_ms() );

        if( status < 0 ){

            log_v_error_P( PSTR("VM error: %d"), status );
            goto end;
        }
        else if( status == VM_STATUS_NO_COROUTINE ){

            log_v_info_P( PSTR("VM finished") );

            goto end;
        }

        THREAD_YIELD( pt );
    }



end:
    vm_deinit( &state->vm );
    
PT_END( pt );
}
