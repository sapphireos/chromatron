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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "trace.h"

// #define trace_printf(...)

#define ENABLE_INTERRUPTS sei()
#define DISABLE_INTERRUPTS cli()


// Critical Section
//
// Notes:  ATOMIC creates a local copy of SREG, but only copies the I bit.
// END_ATOMIC restores the original value of the SREG I bit, but does not modify any other bits.
// This guarantees interrupts will be disabled within the critical section, but does not
// enable interrupts if they were already disabled.  Since it also doesn't modify any of the
// other SREG bits, it will not disturb instructions after the critical section.
// An example of this would be performing a comparison inside the critical section, and then
// performing a branch immediately thereafter.  If we restored SREG in its entirety, we will
// have destroyed the result of the compare and the code will not execute as intended.
#define ATOMIC uint8_t __sreg_i = ( SREG & 0b10000000 ); cli()
#define END_ATOMIC SREG |= __sreg_i

#define FLASH_STRING(x) PSTR(x)
#define FLASH_STRING_T  PGM_P

#define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))

#define CPU_FREQ_PER 16000000

typedef uint8_t sys_clock_t8;

#define CLK_DIV_1   0b00000000
#define CLK_DIV_2   0b00000001
#define CLK_DIV_4   0b00000010
#define CLK_DIV_8   0b00000011
#define CLK_DIV_16  0b00000100
#define CLK_DIV_32  0b00000101
#define CLK_DIV_64  0b00000110
#define CLK_DIV_128 0b00000111
#define CLK_DIV_256 0b00001000

#define SLP_MODE_ACTIVE     0b00000000
#define SLP_MODE_IDLE       0b00000001
#define SLP_MODE_ACDNRM     0b00000011
#define SLP_MODE_PWRDN      0b00000101
#define SLP_MODE_PWRSAVE    0b00000111
#define SLP_MODE_STNDBY     0b00001101
#define SLP_MODE_EXSTNDBY   0b00001111

// #define ENABLE_CONFIG_CHANGE CCP = CCP_IOREG_gc

#endif
