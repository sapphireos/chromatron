/* 
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
 */
 
#include "cpu.h"
#include "system.h"

#include "boot_serial.h"


static bool timeout_enable;
static bool timeout;


void boot_serial_v_init_serial( bool _timeout_enable ){

    timeout_enable = _timeout_enable;
    timeout = FALSE;
    
}

bool boot_serial_b_timed_out( void ){

    return timeout;
}

uint8_t boot_serial_u8_receive_char( void ){
    
    if( timeout ){
        
        return 0;
    }

    return 0;
}

void boot_serial_v_send_char( uint8_t c ){
    

}

