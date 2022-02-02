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


#ifndef _HAL_CPU_H	
#define _HAL_CPU_H

#include "user_interface.h"
#include "osapi.h"
#include "bool.h"
#include "trace.h"

// borrowed from Arduino.h
#ifndef __STRINGIFY
#define __STRINGIFY(a) #a
#endif
#define xt_rsil(level) (__extension__({uint32_t state; __asm__ __volatile__("rsil %0," __STRINGIFY(level) : "=a" (state)); state;}))
#define xt_wsr_ps(state)  __asm__ __volatile__("wsr %0,ps; isync" :: "a" (state) : "memory")

#define ENABLE_INTERRUPTS xt_rsil(0)
#define DISABLE_INTERRUPTS xt_rsil(15)

#define ATOMIC int __atomic_state = xt_rsil(15)
#define END_ATOMIC xt_wsr_ps(__atomic_state)

#define FLASH_STRING(x) x
#define FLASH_STRING_T const char*

#define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))
#define NON_CACHEABLE __attribute__ ((section (".non_cacheable")))
#define MEMORY_HEAP __attribute__ ((section (".memory_heap"), aligned(4)))

// helper to load from unaligned pointers
static inline uint32_t load32( uint8_t *ptr ){

    return (((uint32_t)ptr[3] << 24) | ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[0] << 0));
}
#define LOAD32(ptr) load32((uint8_t *)ptr)

static inline uint16_t load16( uint8_t *ptr ){

    return (((uint32_t)ptr[1] << 8) | ((uint32_t)ptr[0] << 0));
}
#define LOAD16(ptr) load16((uint8_t *)ptr)

static inline void store64( uint8_t *ptr, uint64_t val ){

    ptr[0] = ( val >> 0 ) & 0xff;
    ptr[1] = ( val >> 8 ) & 0xff;
    ptr[2] = ( val >> 16 ) & 0xff;
    ptr[3] = ( val >> 24 ) & 0xff;

    ptr[4] = ( val >> 32 ) & 0xff;
    ptr[5] = ( val >> 40 ) & 0xff;
    ptr[6] = ( val >> 48 ) & 0xff;
    ptr[7] = ( val >> 46 ) & 0xff;
}
#define STORE64(ptr, val) store64((uint8_t *)ptr, val)

static inline void store32( uint8_t *ptr, uint32_t val ){

    ptr[0] = ( val >> 0 ) & 0xff;
    ptr[1] = ( val >> 8 ) & 0xff;
    ptr[2] = ( val >> 16 ) & 0xff;
    ptr[3] = ( val >> 24 ) & 0xff;
}
#define STORE32(ptr, val) store32((uint8_t *)ptr, val)

static inline void store16( uint8_t *ptr, uint16_t val ){

    ptr[0] = ( val >> 0 ) & 0xff;
    ptr[1] = ( val >> 8 ) & 0xff;
}
#define STORE16(ptr, val) store16((uint8_t *)ptr, val)

void hal_cpu_v_delay_us( uint16_t us );
void hal_cpu_v_delay_ms( uint16_t ms );

#endif
