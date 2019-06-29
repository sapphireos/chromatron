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

#include "hal_info.h"
#include "cpu.h"

static int8_t hal_info_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

    	if( hash == __KV__hw_type ){
            
            memset( data, 0, len );

            #ifdef BOARD_CHROMATRONX
    		strlcpy_P( data, PSTR("ChromatronX"), len );
            #else
            strlcpy_P( data, PSTR("ChromatronNG"), len );
            #endif
    	}
    	else if( hash == __KV__cpu_clock ){

    		*(uint32_t *)data = cpu_u32_get_clock_speed();
    	}
        else if( hash == __KV__vm_max_image_size ){

            *(uint32_t *)data = VM_MAX_IMAGE_SIZE;
        }
        else if( hash == __KV__vm_max_cycle_limit ){

            *(uint32_t *)data = VM_MAX_CYCLES;
        }
        else if( hash == __KV__vm_max_call_depth ){

            *(uint32_t *)data = VM_MAX_CALL_DEPTH;
        }
        else if( hash == __KV__vm_max_threads ){

            *(uint32_t *)data = VM_MAX_THREADS;
        }
        else if( hash == __KV__vm_max_vms ){

            *(uint32_t *)data = VM_MAX_VMS;
        }


        // TODO this is for debug only, and can go away at some point.
        else if( hash == __KV__flash_intf_addr ){

            *(uint32_t *)data = (uint32_t)__builtin_return_address(0);
        }

        return 0;
    }

    return -1;
}


KV_SECTION_META kv_meta_t hal_info_kv[] = {
    { SAPPHIRE_TYPE_STRING32,     0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "hw_type" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "cpu_clock" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_image_size" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_cycle_limit" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_call_depth" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_threads" },
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_vms" },

    // TODO debug only
    { SAPPHIRE_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "flash_intf_addr" },
};





