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

#include "hal_cpu.h"
#include "cpu.h"
#include "hal_timers.h"
#include "watchdog.h"
#include "flash25.h"
#include "system.h"
#include "keyvalue.h"

#include "graphics.h"

#include "esp_image_format.h"

#ifdef BOOTLOADER
#include "bootloader_flash.h"
#include "bootloader_flash_priv.h"
#else
#include "esp_spi_flash.h"
#endif

#include "esp_system.h"
#include "esp_pm.h"
#include "esp32/clk.h"
#include "esp_sleep.h"
#include "esp32/rom/ets_sys.h"
#include "esp32/rom/rtc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal/brownout_hal.h"

static uint8_t esp_reset;

KV_SECTION_META kv_meta_t hal_cpu_kv[] = {
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &esp_reset,                       0,  "esp_reset_reason" }
};

uint32_t FW_START_OFFSET;

static void spi_read( uint32_t address, uint32_t *ptr, uint32_t size ){

    #ifdef BOOTLOADER
    bootloader_flash_read( address, ptr, size, false );
    #else
    spi_flash_read( address, ptr, size );
    #endif
}

uint32_t hal_cpu_u32_get_internal_start( void ){

    esp_image_header_t header;
    uint32_t addr = FW_SPI_START_OFFSET;
    spi_read( addr, (uint32_t *)&header, sizeof(header) );
    addr += sizeof(header);

    if( header.segment_count == 0xff ){

        trace_printf("INTERNAL Image invalid!\n");        

        return 0xffffffff;
    }

    uint32_t start = 0;

    trace_printf("INTERNAL Image info\nSegments: %d\n", header.segment_count);

    for( uint8_t i = 0; i < header.segment_count; i++ ){

        esp_image_segment_header_t seg_header;
        spi_read( addr, (uint32_t *)&seg_header, sizeof(seg_header) );

        trace_printf("Segment load: 0x%0x len: %u offset: 0x%0x\n", seg_header.load_addr, seg_header.data_len, addr);

        if( seg_header.load_addr == FW_LOAD_ADDR ){

            start = addr + sizeof(seg_header);
        }

        addr += sizeof(seg_header);
        addr += seg_header.data_len;
    }

    return start;
}

void cpu_v_init( void ){

    trace_printf("CPU init...\n");

    FW_START_OFFSET = hal_cpu_u32_get_internal_start();

    trace_printf("FW_START_OFFSET: 0x%0x\n", FW_START_OFFSET);
    
    #ifndef BOOTLOADER
    DISABLE_INTERRUPTS;

    // ensure RTC power domain remains enabled during sleep, so GPIOs will retain their state.
    esp_sleep_pd_config( ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON );

    cpu_v_set_clock_speed_high();

    vTaskDelay(10);

    // this loop will hang forever if light_sleep_enable is TRUE
    // while (esp_clk_cpu_freq() / 1000000 != pm_config.max_freq_mhz) {
    //     vTaskDelay(10);
    // }
    #endif    

    #ifndef BOOTLOADER
    #ifdef CONFIG_FREERTOS_UNICORE
    trace_printf("CPU: 1 core\n");
    #else
    trace_printf("CPU: 2 cores\n");
    #endif    

    trace_printf("Running on core: %d\n", xPortGetCoreID());
    #endif



    /*
    
    Brown out detector config

    This is taken from IDF source and is otherwise undocumented.

    Thresholds:
    0: 2.43V
    1: 2.48V
    2: 2.58V
    3: 2.62V
    4: 2.67V
    5: 2.70V
    6: 2.77V
    7: 2.80V

    */

    /*    
    brownout_hal_config_t cfg = {
        .threshold = 5,
        .enabled = true,
        .reset_enabled = true,
        .flash_power_down = true,
        .rf_power_down = true,
    };

    brownout_hal_config(&cfg);

    */

    #ifndef BOOTLOADER
    vTaskDelay(10);
    #endif
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

    esp_reset = reason;

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

#define SLEEP_THRESHOLD 4
#define MAX_SLEEP_PERIOD 20

void cpu_v_sleep( void ){

    // only yield the RTOS task (so auto light sleep can operate)
    // if we are not in safe mode and pixels are not enabled.

    if( gfx_b_pixels_enabled() ){

        return;
    }

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    uint32_t delta = thread_u32_get_next_alarm_delta();

    // if next thread alarm is more than SLEEP_THRESHOLD ms away, we can sleep for at least SLEEP_THRESHOLD ms.
    if( delta >= SLEEP_THRESHOLD ){

        uint32_t sleep_time = delta;

        // MAX_SLEEP_PERIOD ms should be set to give us a reasonable polling rate for threads that don't use timers.
        if( sleep_time > MAX_SLEEP_PERIOD ){

            sleep_time = MAX_SLEEP_PERIOD;
        }

        vTaskDelay( sleep_time );    
    }   
}

bool cpu_b_osc_fail( void ){

    return 0;
}

/*

NOTE light sleep will break the JTAG connection when debugging!

*/

void cpu_v_set_clock_speed_low( void ){

    esp_pm_config_esp32_t pm_config = { 0 };
    #ifdef ESP32_MAX_CPU_160M
    pm_config.max_freq_mhz = 80;
    pm_config.min_freq_mhz = 80;
    #else
    pm_config.max_freq_mhz = 80;
    pm_config.min_freq_mhz = 80;
    #endif

    pm_config.light_sleep_enable = TRUE;

    esp_pm_configure( &pm_config );    

    trace_printf("Setting frequency to %d MHz...\n", pm_config.max_freq_mhz);
}

void cpu_v_set_clock_speed_high( void ){

    esp_pm_config_esp32_t pm_config = { 0 };
    #ifdef ESP32_MAX_CPU_160M
    pm_config.max_freq_mhz = 160;
    pm_config.min_freq_mhz = 80;
    #else
    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 80;
    #endif

    pm_config.light_sleep_enable = TRUE;

    esp_pm_configure( &pm_config );    

    trace_printf("Setting frequency to %d MHz...\n", pm_config.max_freq_mhz);
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


// replace with actual values!
#define CPU_POWER_HIGH  ( 30000 * 3.3 )
#define CPU_POWER_MED   ( 20000 * 3.3 )
#define CPU_POWER_LOW   ( 16000 * 3.3 )

uint32_t cpu_u32_get_power( void ){

    uint32_t cpu_clock = cpu_u32_get_clock_speed();

    if( cpu_clock <= 80000000 ){

        return CPU_POWER_LOW;
    }
    else if( cpu_clock <= 160000000 ){

        return CPU_POWER_MED;
    }

    return CPU_POWER_HIGH;
}