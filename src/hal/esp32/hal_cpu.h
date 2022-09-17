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


#ifndef _HAL_CPU_H	
#define _HAL_CPU_H

#include <stdint.h>

#include "bool.h"
#include "trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

extern uint32_t FW_START_OFFSET;
uint32_t hal_cpu_u32_get_internal_start( void );

// we don't have manual control of interrupts on the ESP32 (at least, not easily)
#define ENABLE_INTERRUPTS 
#define DISABLE_INTERRUPTS

#define ATOMIC unsigned __atomic_state = portSET_INTERRUPT_MASK_FROM_ISR()
#define END_ATOMIC portCLEAR_INTERRUPT_MASK_FROM_ISR(__atomic_state)

#define FLASH_STRING(x) x
#define FLASH_STRING_T const char*

#define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))
#define NON_CACHEABLE __attribute__ ((section (".non_cacheable")))
#define MEMORY_HEAP __attribute__ ((section (".memory_heap"), aligned(4)))

// helper to load from unaligned pointers
// static inline uint32_t load32( uint8_t *ptr ){

//     return (((uint32_t)ptr[3] << 24) | ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[0] << 0));
// }
// #define LOAD32(ptr) load32((uint8_t *)ptr)

// static inline uint16_t load16( uint8_t *ptr ){

//     return (((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[0] << 0));
// }
// #define LOAD16(ptr) load16((uint8_t *)ptr)

// static inline void store32( uint8_t *ptr, uint32_t val ){

//     ptr[0] = ( val >> 0 ) & 0xff;
//     ptr[1] = ( val >> 8 ) & 0xff;
//     ptr[2] = ( val >> 16 ) & 0xff;
//     ptr[3] = ( val >> 24 ) & 0xff;
// }
// #define STORE32(ptr, val) store32((uint8_t *)ptr, val)

// static inline void store16( uint8_t *ptr, uint16_t val ){

//     ptr[0] = ( val >> 0 ) & 0xff;
//     ptr[1] = ( val >> 8 ) & 0xff;
// }
// #define STORE16(ptr, val) store16((uint8_t *)ptr, val)

void hal_cpu_v_delay_us( uint16_t us );
void hal_cpu_v_delay_ms( uint16_t ms );

#endif
