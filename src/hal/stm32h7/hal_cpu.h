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


#ifndef _HAL_CPU_H	
#define _HAL_CPU_H

#include "bool.h"

#include "stm32h7xx.h"

#include "Trace.h"

#include "../board_types.h"

#define STM32H7_REV_Z		0x1001
#define STM32H7_REV_Y		0x1003
#define STM32H7_REV_X		0x2001
#define STM32H7_REV_V		0x2003

#define hal_cpu_v_invalidate_d_cache 						SCB_InvalidateDCache
#define hal_cpu_v_clean_d_cache 							SCB_CleanDCache
#define hal_cpu_v_clean_and_invalidate_d_cache 				SCB_CleanInvalidateDCache

#define hal_cpu_v_invalidate_d_cache_by_addr 				SCB_InvalidateDCache_by_Addr
#define hal_cpu_v_clean_d_cache_by_addr 					SCB_CleanDCache_by_Addr
#define hal_cpu_v_clean_and_invalidate_d_cache_by_addr 		SCB_CleanInvalidateDCache_by_Addr

void hal_cpu_v_boot_init( void );

#define ENABLE_INTERRUPTS __enable_irq()
#define DISABLE_INTERRUPTS __disable_irq()

#define RESET_SOURCE_POWER_ON   0x01
#define RESET_SOURCE_JTAG       0x02
#define RESET_SOURCE_EXTERNAL   0x04
#define RESET_SOURCE_BROWNOUT   0x08

#define ATOMIC uint32_t __primask = __get_PRIMASK(); DISABLE_INTERRUPTS;
#define END_ATOMIC if( !__primask ){ ENABLE_INTERRUPTS; }

#define FLASH_STRING(x) x
#define FLASH_STRING_T const char*

#define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))
#define NON_CACHEABLE __attribute__ ((section (".non_cacheable")))
#define MEMORY_HEAP __attribute__ ((section (".memory_heap")))

void hal_cpu_v_delay_us( uint16_t us );


#endif
