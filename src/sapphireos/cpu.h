/*
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
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "hal_cpu.h"
#include "bool.h"

#define RESET_SOURCE_POWER_ON   0x01
#define RESET_SOURCE_JTAG       0x02
#define RESET_SOURCE_EXTERNAL   0x04
#define RESET_SOURCE_BROWNOUT   0x08
#define RESET_SOURCE_WATCHDOG   0x10


#define _TOKENPASTE(x, y) x ## y
#define TOKENPASTE(x, y) _TOKENPASTE(x, y)

#if defined(__SIM__)
    
    #define PGM_P const char*
    #define PROGMEM
    // #define PSTR(s) ({static char c[ ] = (s); c;})
    #define PSTR(s) s

    #define strncpy_P strncpy
    #define strncmp_P strncmp
    #define strcmp_P strcmp
    #define strlen_P strlen
    #define memcpy_P memcpy
    #define strnlen(s, n) strlen(s) // NOT SAFE!!!
    #define strstr_P strstr

    #define sprintf_P sprintf
    #define snprintf_P snprintf
    #define vsnprintf_P vsnprintf
    #define printf_P printf

    #define pgm_read_word(x) *(uint16_t *)x
    #define pgm_read_byte_far(x) *(uint8_t *)x
    #define pgm_read_byte(x) *(uint8_t *)x
    // uint16_t pgm_read_word(void *a);
    // uint8_t pgm_read_byte_far(void *a);

    #define wdt_reset()
    #define _delay_us(x)

    #define cli()

    char *itoa(long i, char* s, int dummy_radix);

#elif defined(ARM) || defined(ESP8266) || defined(ESP32)
    #define ALIGN32

    #ifdef BOOTLOADER
        // #define _FILE #__FILE__
        // #define _FNAME TOKENPASTE( unique, _FILE )

        #define ISR(handler) void TOKENPASTE( handler, __COUNTER__ )( void )
    #else
        #define ISR(handler) void handler( void )
    #endif
    
    #define PGM_P const char*

    #ifdef ESP8266
    // #define PROGMEM __attribute__((section(".irom0.text"))) __attribute__((aligned(4)))
    #define PROGMEM ICACHE_RODATA_ATTR
    #define PSTR(s) (__extension__({static const char __c[] __attribute__((aligned(4))) PROGMEM = (s); &__c[0];}))
    #else
    #define PROGMEM
    #define PSTR(s) s
    #endif

    #define strncpy_P strncpy
    #define strncmp_P strncmp
    #define strlcpy_P strlcpy
    #define strlcmp_P strlcmp
    #define strcmp_P strcmp
    #define strlen_P strlen
    #define strcpy_P strcpy
    #define memcpy_P memcpy
    #define memcpy_PF memcpy
    #define strnlen(s, n) strlen(s) // NOT SAFE!!!
    #define strstr_P strstr

    #define sprintf_P sprintf
    #define snprintf_P snprintf
    #define vsnprintf_P vsnprintf
    #define printf_P printf

    // #if !defined(ESP32)
    
    #define pgm_read_word(x) *(uint16_t *)(x)
    #define pgm_read_byte_far(x) *(uint8_t *)(x)
    #define pgm_read_byte(x) *(uint8_t *)(x)

    // #endif
    
    #define wdt_reset()
    #define _delay_us(x) hal_cpu_v_delay_us(x)
    #if defined(ESP8266) || defined(ESP32)
    #define _delay_ms(x) hal_cpu_v_delay_ms(x)
    #else
    #define _delay_ms(x) HAL_Delay(x)
    #endif

    #define cli()

#endif

#ifndef LOAD32
#define LOAD32(ptr) (*(uint32_t*)ptr)
#endif

#ifndef LOAD16
#define LOAD16(ptr) (*(uint16_t*)ptr)
#endif

#ifndef STORE64
#define STORE64(ptr, val) (*(uint64_t*)ptr = val)
#endif

#ifndef STORE32
#define STORE32(ptr, val) (*(uint32_t*)ptr = val)
#endif

#ifndef STORE16
#define STORE16(ptr, val) (*(uint16_t*)ptr = val)
#endif


void cpu_v_init( void );
uint8_t cpu_u8_get_reset_source( void );
void cpu_v_remap_isrs( void );
void cpu_v_sleep( void );
void cpu_v_clear_reset_source( void );
bool cpu_b_osc_fail( void );
void cpu_v_set_clock_speed_low( void );
void cpu_v_set_clock_speed_high( void );
uint32_t cpu_u32_get_clock_speed( void );
void cpu_reboot( void );

uint32_t cpu_u32_get_power( void );

#endif
