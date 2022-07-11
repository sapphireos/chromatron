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


#include "system.h"
#include "hal_cpu.h"
#include "os_irq.h"
#include "hal_watchdog.h"
#include "watchdog.h"

// system timer at 500 khz (see hal_timers.c).

#define WDG_TIMER_STEP 50000 // interrupt at 10 Hz

#define WDG_TIMEOUT 40 // 4 second timeout

static volatile uint8_t wdg_timer;


void wdg_v_reset( void ){

    ATOMIC;
    wdg_timer = WDG_TIMEOUT;
    END_ATOMIC;
}

void wdg_v_enable( wdg_timeout_t8 timeout, wdg_flags_t8 flags ){

    ATOMIC;

    wdt_reset();

    WDTCSR |= ( 1 << WDCE ) | ( 1 << WDE );
    WDTCSR = ( 1 << WDE) | WATCHDOG_TIMEOUT_2048MS; // watchdog timeout at 2048 ms, reset only

    END_ATOMIC;
}

void wdg_v_disable( void ){

    ATOMIC;

    wdt_reset();

    // clear watchdog reset flag
    MCUSR &= ~( 1 << WDRF );

    // enable change
    WDTCSR |= ( 1 << WDCE );

    // disable watchdog
    WDTCSR &= ~( 1 << WDE );


    END_ATOMIC;
}
