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
#include "user_interface.h"
#include "flash25.h"
#include "system.h"
#include "keyvalue.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

extern boot_data_t BOOTDATA boot_data;
//#define RTC_MEM ((volatile uint32_t*)0x60001200)

static uint8_t esp_reset;

KV_SECTION_META kv_meta_t hal_cpu_kv[] = {
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &esp_reset,                       0,  "esp_reset_reason" }
};


void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    // NOTE
    // the ESP8266 needs the software watchdog to be enabled and kicked, even during init.
    // if we don't kick the SW watchdog, the hardware watchdog will reset the cpu.
    // it is not possible to disable the hardware watchdog.
    // thus, all startup routines need watchdog kicks for long running operations.

    #ifndef BOOTLOADER
    wdg_v_enable( 0, 0 );
    #endif

    #ifdef ENABLE_COPROCESSOR
    boot_data.boot_mode = coproc_i32_call0( OPCODE_GET_BOOT_MODE );
    #endif

    struct rst_info *info = system_get_rst_info();

    esp_reset = info->reason;
}

uint8_t cpu_u8_get_reset_source( void ){

    #ifdef BOOTLOADER
    int reason = rtc_get_reset_reason();

    if( reason == 1 ){ // power on

        return RESET_SOURCE_POWER_ON; 
    }
    else if( reason == 2 ){ // external reset or deep sleep

        return RESET_SOURCE_EXTERNAL;       
    }
    else if( reason == 4 ){ // hardware wdt

        return 0;       
    }

    return 0;

    #elif defined(ENABLE_COPROCESSOR)

    return coproc_i32_call0( OPCODE_GET_RESET_SOURCE );

    #else
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
    #endif
}

void cpu_v_clear_reset_source( void ){

}

void cpu_v_remap_isrs( void ){

}

#define SLEEP_THRESHOLD 2
#define MAX_SLEEP_PERIOD 20

void cpu_v_sleep( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // os_delay_us(100);

        return;
    }

    // This logic is done in user_main.c on the ESP8266.
    // The call stack needs to unwind all the way up for
    // the modem sleep to work.

    // uint32_t delta = thread_u32_get_next_alarm_delta();

    // // if next thread alarm is more than SLEEP_THRESHOLD ms away, we can sleep for at least SLEEP_THRESHOLD ms.
    // if( delta >= SLEEP_THRESHOLD ){

    //     uint32_t sleep_time = delta;

    //     // MAX_SLEEP_PERIOD ms should be set to give us a reasonable polling rate for threads that don't use timers.
    //     if( sleep_time > MAX_SLEEP_PERIOD ){

    //         sleep_time = MAX_SLEEP_PERIOD;
    //     }

    //     os_delay_us(sleep_time * 1000);
    // }   
    // else{

    //     os_delay_us(100);
    // }
}

bool cpu_b_osc_fail( void ){

    return 0;
}

void cpu_v_set_clock_speed_low( void ){

    system_update_cpu_freq( SYS_CPU_80MHZ );
}

void cpu_v_set_clock_speed_high( void ){

    system_update_cpu_freq( SYS_CPU_160MHZ );
}

uint32_t cpu_u32_get_clock_speed( void ){

    return system_get_cpu_freq() * 1000000;
}

void cpu_reboot( void ){

#ifndef BOOTLOADER
    #ifdef ENABLE_COPROCESSOR

    if( boot_data.loader_command == LDR_CMD_LOAD_FW ){

        coproc_i32_call0( OPCODE_LOADFW_1 );
        coproc_i32_call0( OPCODE_LOADFW_2 );
    }
    else if( boot_data.boot_mode == BOOT_MODE_NORMAL ){

        // see set_reboot_mode() in system.c for more details.
        // this is actually a request to boot into safe mode.

        coproc_i32_call0( OPCODE_SAFE_MODE );        
    }

    coproc_v_reboot();

    #else

    wdg_v_disable();
    system_restart();
    while(1);

    #endif
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
