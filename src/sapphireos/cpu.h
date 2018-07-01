/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "bool.h"


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

#elif defined(AVR)
    #include <avr/io.h>
    #include <avr/interrupt.h>
    #include <avr/pgmspace.h>
    #include <avr/eeprom.h>
    #include <avr/wdt.h>
    #include <avr/sleep.h>
    #include <util/delay.h>
#elif defined(ARM)
    // #include "sam.h"

    #define PGM_P const char*
    #define PROGMEM
    // #define PSTR(s) ({static char c[ ] = (s); c;})
    #define PSTR(s) s

    #define strncpy_P strncpy
    #define strncmp_P strncmp
    #define strlcpy_P strlcpy
    #define strlcmp_P strlcmp
    #define strcmp_P strcmp
    #define strlen_P strlen
    #define memcpy_P memcpy
    #define memcpy_PF memcpy
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

#else
    #error "No target"
#endif

void cpu_v_init( void );
uint8_t cpu_u8_get_reset_source( void );
void cpu_v_remap_isrs( void );
void cpu_v_sleep( void );
void cpu_v_clear_reset_source( void );
bool cpu_b_osc_fail( void );

#endif
