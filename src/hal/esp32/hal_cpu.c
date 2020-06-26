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
#include "esp_pm.h"
#include "esp_clk.h"
#include "rom/ets_sys.h"
#include "rom/rtc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern boot_data_t BOOTDATA boot_data;


void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    esp_pm_config_esp32_t pm_config = { 0 };
    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 240;
    pm_config.light_sleep_enable = FALSE;
    
    esp_pm_configure( &pm_config );
    trace_printf("Waiting for frequency to be set to %d MHz...\n", 240);
    while (esp_clk_cpu_freq() / 1000000 != 240) {
        vTaskDelay(10);
    }
    trace_printf("Frequency is set to %d MHz\n", 240);
}

uint8_t cpu_u8_get_reset_source( void ){
    #ifdef BOOTLOADER
    
    RESET_REASON reason = rtc_get_reset_reason( 0 );

    if( reason == POWERON_RESET ){

        return RESET_SOURCE_POWER_ON;
    }
    else if( reason == RTCWDT_RTC_RESET ){

        return RESET_SOURCE_EXTERNAL;        
    }
    else if( reason == RTCWDT_BROWN_OUT_RESET ){

        return RESET_SOURCE_BROWNOUT;        
    }

    return 0;

    #else
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
    #endif
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

    return esp_clk_cpu_freq();
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
