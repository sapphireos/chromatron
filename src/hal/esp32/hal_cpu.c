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


extern boot_data_t BOOTDATA boot_data;


void cpu_v_init( void ){

    DISABLE_INTERRUPTS;

    
}

uint8_t cpu_u8_get_reset_source( void ){
	
	return 0;

 //    #ifdef BOOTLOADER
 //    int reason = rtc_get_reset_reason();

 //    if( reason == 1 ){ // power on

 //        return RESET_SOURCE_POWER_ON; 
 //    }
 //    else if( reason == 2 ){ // external reset or deep sleep

 //        return RESET_SOURCE_EXTERNAL;       
 //    }
 //    else if( reason == 4 ){ // hardware wdt

 //        return 0;       
 //    }

 //    return 0;

 //    #else
	// struct rst_info *info = system_get_rst_info();

	// if( info->reason == REASON_DEFAULT_RST ){

	// 	return RESET_SOURCE_POWER_ON; 
	// }
	// else if( info->reason == REASON_WDT_RST ){

		
	// }
	// else if( info->reason == REASON_EXCEPTION_RST ){

		
	// }
	// else if( info->reason == REASON_SOFT_WDT_RST ){

		
	// }
	// else if( info->reason == REASON_SOFT_RESTART ){

		
	// }
	// else if( info->reason == REASON_DEEP_SLEEP_AWAKE ){

		
	// }
	// else if( info->reason == REASON_EXT_SYS_RST ){

	// 	return RESET_SOURCE_EXTERNAL;		
	// }

    return 0;
 //    #endif
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

    // return system_get_cpu_freq() * 1000000;
    return 0;
}

void cpu_reboot( void ){

// #ifndef BOOTLOADER

//     wdg_v_disable();
//     system_restart();
//     while(1);
// #endif
}

void hal_cpu_v_delay_us( uint16_t us ){
	
	// os_delay_us( us );
}

void hal_cpu_v_delay_ms( uint16_t ms ){
	
	for( uint32_t i = 0; i < ms; i++ ){
		
		// os_delay_us( 1000 );
	}
}

typedef union{
    uint32_t i;
    uint16_t words[2];
} word_word_t;

typedef union{
    uint32_t i;
    uint8_t bytes[4];
} word_bytes_t;

uint16_t hal_cpu_u16_pgm_read_word( uint16_t *x ){

	uint32_t addr = (uint32_t)x;
	uint32_t i_addr = addr & ( ~0x01 );
    uint32_t word_addr = addr & (  0x01 );
    word_word_t temp;
    temp.i = *(uint32_t *)i_addr;

    return temp.words[word_addr];
}

uint8_t hal_cpu_u8_pgm_read_byte( uint8_t *x ){

	uint32_t addr = (uint32_t)x;
	uint32_t i_addr = addr & ( ~0x03 );
    uint32_t byte_addr = addr & (  0x03 );
	word_bytes_t temp;
    temp.i = *(uint32_t *)i_addr;

    return temp.bytes[byte_addr];
}