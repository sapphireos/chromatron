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


#ifndef _HAL_CPU_H
#define _HAL_CPU_H

#include "system.h"

void cpu_v_init( void );
uint8_t cpu_u8_get_reset_source( void );
void cpu_v_remap_isrs( void );
void cpu_v_sleep( void );

#define ENABLE_INTERRUPTS
#define DISABLE_INTERRUPTS

#define RESET_SOURCE_POWER_ON   1
#define RESET_SOURCE_JTAG       2
#define RESET_SOURCE_EXTERNAL   4
#define RESET_SOURCE_BROWNOUT   8

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

#define ENABLE_CONFIG_CHANGE

#endif
