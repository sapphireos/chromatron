// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#include "hal_cpu.h"
#include "cpu.h"
#include "hal_timers.h"
#include "watchdog.h"

#include "system.h"

void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    // NOTE
    // the ESP8266 needs the software watchdog to be enabled and kicked, even during init.
    // if we don't kick the SW watchdog, the hardware watchdog will reset the cpu.
    // it is not possible to disable the hardware watchdog.
    // thus, all startup routines need watchdog kicks for long running operations.

    wdg_v_enable( 0, 0 );
}

uint8_t cpu_u8_get_reset_source( void ){

	struct rst_info *info = system_get_rst_info();

	if( info->reason == REASON_DEFAULT_RST ){

		return RESET_SOURCE_POWER_ON; 
	}
	else if( info->reason == REASON_WDT_RST ){

		
	}
	else if( info->reason == REASON_EXCEPTION_RST ){

		
	}
	else if( info->reason == REASON_SOFT_WDT_RST ){

		
	}
	else if( info->reason == REASON_SOFT_RESTART ){

		
	}
	else if( info->reason == REASON_DEEP_SLEEP_AWAKE ){

		
	}
	else if( info->reason == REASON_EXT_SYS_RST ){

		return RESET_SOURCE_EXTERNAL;		
	}

    return 0;
}

void cpu_v_clear_reset_source( void ){

}

void cpu_v_remap_isrs( void ){

}

void cpu_v_sleep( void ){

}

bool cpu_b_osc_fail( void ){

    return 0;
}

uint32_t cpu_u32_get_clock_speed( void ){

    return system_get_cpu_freq() * 1000000;
}

void cpu_reboot( void ){

    #ifndef BOOTLOADER
    system_restart();
    #endif
}

void hal_cpu_v_delay_us( uint16_t us ){
	
	os_delay_us( us );
}

void hal_cpu_v_delay_ms( uint16_t ms ){
	
	for( uint32_t i = 0; i < ms; i++ ){
		
		os_delay_us( 1000 );
	}
}

