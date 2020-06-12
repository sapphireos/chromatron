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

#include "hal_cpu.h"
#include "cpu.h"
#include "hal_timers.h"
#include "watchdog.h"
#include "flash25.h"
#include "system.h"

#include "esp_system.h"
#include "rom/ets_sys.h"

extern boot_data_t BOOTDATA boot_data;


void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    
}

uint8_t cpu_u8_get_reset_source( void ){
	
	esp_reset_reason_t reason = esp_reset_reason();

	if( reason == ESP_RST_POWERON ){

		return RESET_SOURCE_POWER_ON;
	}
	else if( reason == ESP_RST_EXT ){

		return RESET_SOURCE_EXTERNAL;
	}
	else if( reason == ESP_RST_BROWNOUT ){

		return RESET_SOURCE_BROWNOUT;
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

    return ets_get_cpu_frequency() * 1000000;
}

void cpu_reboot( void ){

    wdg_v_disable();
    esp_restart();
    while(1);
}

void hal_cpu_v_delay_us( uint16_t us ){
	
	ets_delay_us( us );
}

void hal_cpu_v_delay_ms( uint16_t ms ){
	
	for( uint32_t i = 0; i < ms; i++ ){
		
		hal_cpu_v_delay_us( 1000 );
	}
}
