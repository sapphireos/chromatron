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

    // check reset source
    uint8_t reset_source = MCUSR;

    if( reset_source & ( 1 << PORF ) ){

        return RESET_SOURCE_POWER_ON;
    }
    else if( reset_source & ( 1 << JTRF ) ){

        return RESET_SOURCE_JTAG;
    }
    else if( reset_source & ( 1 << BORF ) ){

        return RESET_SOURCE_BROWNOUT;
    }
    else if( reset_source & ( 1 << EXTRF ) ){

        return RESET_SOURCE_EXTERNAL;
    }

    return 0;
}

void cpu_v_clear_reset_source( void ){

    // clear status
    MCUSR = 0;
}

void cpu_v_remap_isrs( void ){

    // move ISRs to app
    MCUCR |= _BV( IVCE );
    
    uint8_t mcucr = MCUCR;          
    
    mcucr &= ~_BV( IVCE );
    mcucr &= ~_BV( IVSEL );
        
    MCUCR = mcucr;
}

void cpu_v_sleep( void ){

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
    wdg_v_enable( WATCHDOG_TIMEOUT_16MS, WATCHDOG_FLAGS_RESET );    

    while(1);
}

#ifndef BOOTLOADER
// bad interrupt
ISR(__vector_default){

    ASSERT( 0 );
}
#endif
