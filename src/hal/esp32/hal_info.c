// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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
#include "flash_fs.h"

#include "esp_system.h"

static int8_t hal_info_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

    	if( hash == __KV__hw_type ){
            
            memset( data, 0, len );

            strlcpy_P( data, PSTR("ChromatronESP32"), len );
    	}
        else if( hash == __KV__hw_board_type ){
            
            *(uint8_t *)data = ffs_u8_read_board_type();
        }
    	else if( hash == __KV__cpu_clock ){

            STORE32(data, cpu_u32_get_clock_speed());
    	}
        else if( hash == __KV__cpu_cores ){

            #ifdef CONFIG_FREERTOS_UNICORE
            STORE32(data, 1);
            #else
            STORE32(data, 2);
            #endif
        }
        else if( hash == __KV__esp_heap_free ){

            STORE32(data, esp_get_free_heap_size());
        }
        else if( hash == __KV__esp_heap_min_free ){

            STORE32(data, esp_get_minimum_free_heap_size());
        }
        else if( hash == __KV__vm_max_image_size ){

            STORE32(data, VM_MAX_IMAGE_SIZE);
        }
        else if( hash == __KV__vm_max_cycle_limit ){

            STORE32(data, VM_MAX_CYCLES);
        }
        else if( hash == __KV__vm_max_call_depth ){

            STORE32(data, VM_MAX_CALL_DEPTH);
        }
        else if( hash == __KV__vm_max_threads ){

            STORE32(data, VM_MAX_THREADS);
        }
        else if( hash == __KV__vm_max_vms ){

            STORE32(data, VM_MAX_VMS);
        }

        return 0;
    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__hw_board_type ){

            if( ffs_u8_read_board_type() == FLASH_FS_HW_TYPE_UNSET ){

                ffs_v_write_board_type( *(uint8_t *)data );    

                return 0;
            }

            return -1; // cannot set board rev (already set)
        }
    }

    return -1;
}


KV_SECTION_META kv_meta_t hal_info_kv[] = {
    { CATBUS_TYPE_STRING32,     0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "hw_type" },
    { CATBUS_TYPE_UINT8,        0, 0,                   0, hal_info_kv_handler,  "hw_board_type" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "cpu_clock" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "cpu_cores" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "esp_heap_free" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "esp_heap_min_free" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_image_size" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_cycle_limit" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_call_depth" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_threads" },
    { CATBUS_TYPE_UINT32,       0, KV_FLAGS_READ_ONLY,  0, hal_info_kv_handler,  "vm_max_vms" },
};





