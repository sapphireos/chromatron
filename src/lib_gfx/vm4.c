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

void vm4_v_init( void ){


}

void vm4_v_reset( uint8_t vm_id ){

    ASSERT( vm_id < VM_MAX_VMS );

    // reset_vm( vm_id );
}
