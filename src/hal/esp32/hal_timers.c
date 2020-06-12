// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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
#include "timers.h"
#include "hal_timers.h"
#include "driver/timer.h"

#define TICKS_TO_US(a) (a / 40000000)


void hal_timer_v_init( void ){
    // APB clock is 80 MHz
    // divider = 2
    // HAL timer is 40 MHz

    timer_config_t config = {
        FALSE,
        FALSE,
        0,
        TIMER_COUNT_UP,
        TRUE, // auto reload
        2 // divider = 2
    };

    timer_init( HAL_TIMER_GROUP, HAL_TIMER_INDEX, &config );
    timer_set_counter_value(HAL_TIMER_GROUP, HAL_TIMER_INDEX, 0);
    timer_start( HAL_TIMER_GROUP, HAL_TIMER_INDEX );
}

bool tmr_b_io_timers_running( void ){

    return TRUE;
}


uint64_t tmr_u64_get_system_time_us( void ){

    uint64_t ticks = 0;

    timer_get_counter_value( HAL_TIMER_GROUP, HAL_TIMER_INDEX, &ticks );

    return TICKS_TO_US(ticks);
}


int8_t tmr_i8_set_alarm_microseconds( int64_t alarm ){

    return 0;
}

void tmr_v_cancel_alarm( void ){

}

// return true if alarm is armed
bool tmr_b_alarm_armed( void ){

    return 0;
}
