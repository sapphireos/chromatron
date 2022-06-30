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
#include "watchdog.h"

void cpu_v_init( void ){

    cli();


}

uint8_t cpu_u8_get_reset_source( void ){

    // uint8_t temp = RST.STATUS;

    // if( temp & RST_PORF_bm ){

    //     return RESET_SOURCE_POWER_ON;
    // }
    // else if( temp & RST_PDIRF_bm ){

    //     return RESET_SOURCE_JTAG;
    // }
    // else if( temp & RST_EXTRF_bm ){

    //     return RESET_SOURCE_EXTERNAL;
    // }
    // else if( temp & RST_BORF_bm ){

    //     return RESET_SOURCE_BROWNOUT;
    // }

    return 0;
}

void cpu_v_clear_reset_source( void ){

    // clear status
    // RST.STATUS = 0xff;
}

void cpu_v_remap_isrs( void ){

}

void cpu_v_sleep( void ){

    // SLEEP.CTRL = SLEEP_MODE_IDLE;
    // DISABLE_INTERRUPTS;
    // sleep_enable();
    // ENABLE_INTERRUPTS;
    // sleep_cpu();
    // sleep_disable();
}

bool cpu_b_osc_fail( void ){

    return FALSE;
}

void cpu_v_set_clock_speed_low( void ){

}

void cpu_v_set_clock_speed_high( void ){

}

uint32_t cpu_u32_get_clock_speed( void ){

    return 16000000;
}

void cpu_reboot( void ){

    // enable watchdog timer:
    // wdg_v_enable( WATCHDOG_TIMEOUT_16MS, WATCHDOG_FLAGS_RESET );    
}

#ifndef BOOTLOADER
// bad interrupt
ISR(__vector_default){

    ASSERT( 0 );
}
#endif
