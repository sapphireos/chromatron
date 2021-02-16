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

#include "hal_cpu.h"
#include "cpu.h"
#include "hal_timers.h"
#include "watchdog.h"
#include "user_interface.h"
#include "flash25.h"
#include "system.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

extern boot_data_t BOOTDATA boot_data;
//#define RTC_MEM ((volatile uint32_t*)0x60001200)

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
}


void hal_cpu_v_load_bootdata( void ){

    uint32_t buf[sizeof(boot_data) + sizeof(uint32_t)];
    uint32_t *ptr = (uint32_t *)&boot_data;     
        
    flash25_v_read( BOOTLOADER_INFO_BLOCK, buf, sizeof(buf) );

    if( ( buf[0] + buf[1] ) != buf[2] ){

        memset( &boot_data, 0, sizeof(boot_data) );

        return;
    }

    *ptr++ = buf[0];
    *ptr++ = buf[1];

    // load bootloader data from RTC
    // uint32_t *ptr = (uint32_t *)&boot_data;
    // volatile uint32_t *rtc = RTC_MEM;
    // uint32_t checksum = 0;

    // for( uint32_t i = 0; i < sizeof(boot_data) / 4; i++ ){

    //     *ptr = *rtc;
    //     checksum += *ptr;
    //     ptr++;
    //     rtc++;
    // }

    // if( *rtc != checksum ){

    //     trace_printf("RTC checksum fail\r\n");

    //     memset( (void *)&boot_data, 0, sizeof(boot_data) );
    // }
}

void hal_cpu_v_store_bootdata( void ){
    
    uint32_t buf[sizeof(boot_data) + sizeof(uint32_t)];
    uint32_t *ptr = (uint32_t *)&boot_data;     
    uint32_t checksum = 0;

    buf[0] = *ptr;
    checksum += *ptr++;
    buf[1] = *ptr;
    checksum += *ptr++;
    buf[2] = checksum;

    flash25_v_erase_4k( BOOTLOADER_INFO_BLOCK );

    flash25_v_write( BOOTLOADER_INFO_BLOCK, buf, sizeof(buf) );

    // load bootloader data from RTC
    // uint32_t *ptr = (uint32_t *)&boot_data;
    // volatile uint32_t *rtc = RTC_MEM;
    // uint32_t checksum = 0;

    // for( uint32_t i = 0; i < sizeof(boot_data) / 4; i++ ){

    //     *rtc = *ptr;
    //     checksum += *ptr;
    //     ptr++;
    //     rtc++;
    // }

    // *rtc = checksum;
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

void cpu_v_sleep( void ){

}

bool cpu_b_osc_fail( void ){

    return 0;
}

uint32_t cpu_u32_get_clock_speed( void ){

    return system_get_cpu_freq() * 1000000;
}

void cpu_reboot( void ){

    hal_cpu_v_store_bootdata();

#ifndef BOOTLOADER
    #ifdef ENABLE_COPROCESSOR

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

