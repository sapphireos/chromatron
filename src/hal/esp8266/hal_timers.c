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

#include "system.h"

#include "hal_cpu.h"
#include "os_irq.h"
#include "timers.h"
#include "power.h"
#include "hal_timers.h"
#include "hal_watchdog.h"
#include "logging.h"

static uint32_t last_sys_time;
static uint32_t overflows;

void hal_timer_v_init( void ){

}

bool tmr_b_io_timers_running( void ){

    return TRUE;
}


uint64_t tmr_u64_get_system_time_us( void ){

    #ifndef BOOTLOADER
	uint32_t now = system_get_time();

	if( now < last_sys_time ){

        // log_v_debug_P( PSTR("%lu %lu %lu"), now, last_sys_time, overflows );

		overflows++;
	}

	last_sys_time = now;

    return (uint64_t)overflows * 0x100000000 + now;
    #endif
}

