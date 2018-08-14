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

#include "sapphire.h"

#include "esp8266.h"
#include "vm_wifi_cmd.h"


int8_t vm_sync_i8_request_frame_sync( void ){

	return wifi_i8_send_msg( WIFI_DATA_ID_REQUEST_FRAME_SYNC, 0, 0 );
}

void vm_sync_v_process_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

	if( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ){

        // uint32_t *page = (uint32_t *)data;

        // if( *page == 0 ){

        //     vm_state_t *vm_state = (vm_state_t *)&data[sizeof(uint32_t)];

        //     log_v_debug_P( PSTR("page: %u %lx %u"), *page, vm_state->program_name_hash, vm_state->data_len );
        // }
        // else{

        //     log_v_debug_P( PSTR("page: %u"), *page );
        // }

    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DATA ){

        uint32_t *page = (uint32_t *)data;

        log_v_debug_P( PSTR("sync page: %u"), *page );
    }
}

void vm_sync_v_init( void ){


}