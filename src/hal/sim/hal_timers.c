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

#include "os_irq.h"
#include "timers.h"
#include "power.h"
#include "hal_timers.h"

void hal_timer_v_init( void ){


}

// Return TRUE if any timers on the IO clock are running.
// The async timer is a different clock domain, so it does not count.
bool tmr_b_io_timers_running( void ){

    return FALSE;
}

uint64_t tmr_u64_get_ticks( void ){

    return 0;
}
