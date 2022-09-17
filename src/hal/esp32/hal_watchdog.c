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
#include "hal_watchdog.h"
#include "watchdog.h"
#include "esp_task_wdt.h"

void esp_task_wdt_isr_user_handler( void ){

    // we can't assert here because it will take too long
    // and the *interrupt* watchdog will trigger.
        

    trace_printf("WATCHDOG!\r\n");

    // ASSERT( 0 );
}

void wdg_v_reset( void ){

    #ifndef BOOTLOADER
    esp_task_wdt_reset();
    #endif
}

void wdg_v_enable( wdg_timeout_t8 timeout, wdg_flags_t8 flags ){

    #ifndef BOOTLOADER
    esp_task_wdt_init( 8, TRUE );
    esp_task_wdt_add( NULL );
    #endif
}

void wdg_v_disable( void ){

    #ifndef BOOTLOADER
    esp_task_wdt_delete( NULL );
    #endif
}
