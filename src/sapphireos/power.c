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
#include "hal_cpu.h"

#include "power.h"
#include "threading.h"
#include "keyvalue.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"


#ifdef ENABLE_POWER

void pwr_v_init( void ){

}


void pwr_v_sleep( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }    

    // set sleep mode
    cpu_v_sleep();
}

void pwr_v_wake( void ){

}


#endif
