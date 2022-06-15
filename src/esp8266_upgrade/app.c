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

#include "app.h"

#include "coprocessor.h"

#ifndef ESP8266_UPGRADE
#error "esp8266_upgrade requires ESP8266_UPGRADE defined in target.h!"
#endif


PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
    
    // log_v_debug_P( PSTR("coproc crc: 0x%04x"), coproc_u16_fw_crc() );

    // char firmware_version[FW_VER_LEN];
    // coproc_v_fw_version( firmware_version );
    // log_v_debug_P( PSTR("coproc ver: %s"), firmware_version );

PT_END( pt );	
}


void app_v_init( void ){

    thread_t_create( app_thread,
                     PSTR("app"),
                     0,
                     0 );
}
