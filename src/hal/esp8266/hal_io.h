/*
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
 */

#ifndef _HAL_IO_H
#define _HAL_IO_H 

#include "system.h"
#include "io_intf.h"
#include "cpu.h"

#ifdef ENABLE_COPROCESSOR

#define IO_PIN_0_GPIO       0 // PA4
#define IO_PIN_1_XCK        1 // PC5
#define IO_PIN_2_TXD        2 // PC7
#define IO_PIN_3_RXD        3 // PC6
#define IO_PIN_4_ADC0       4 // PA1
#define IO_PIN_5_ADC1       5 // PA0
#define IO_PIN_6_DAC0       6 // PB2
#define IO_PIN_7_DAC1       7 // PB3
#define IO_PIN_PWM_0        8 // PC0
#define IO_PIN_PWM_1        9 // PC1
#define IO_PIN_PWM_2        10 // PC2
#define IO_PIN_PWM_3        11 // PC3

#define IO_PIN_LED_RED      12 // PC4
#define IO_PIN_LED_GREEN    13 // PD5
#define IO_PIN_LED_BLUE     14 // PD4

#define IO_PIN_COUNT        15

#define IO_PIN_DEBUG        IO_PIN_PWM_1

#endif

void io_v_set_esp_led( bool state );

#endif


