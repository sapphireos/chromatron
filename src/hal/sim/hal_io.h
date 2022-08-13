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


// #define IO_BUTTON_CHECK_INTERVAL	1000
// #define IO_BUTTON_TRIGGER_THRESHOLD	10
// #define IO_BUTTON_RESTART_TIME		5

// #define IO_N_INT_HANDLERS       4

// // IO definitions
// // pin name to logical (not physical) pin number mapping

// #define IO_PIN_COUNT        29

// #define IO_PIN_HW_ID        28

#define IO_PIN_LED2         27
#define IO_PIN_LED1         26
#define IO_PIN_LED0         25

// #define IO_PIN_S3           24
// #define IO_PIN_S2           23
// #define IO_PIN_S1           22
// #define IO_PIN_S0           21

// #define IO_PIN_GPIO7        20
// #define IO_PIN_GPIO6        19
// #define IO_PIN_GPIO5        18
// #define IO_PIN_GPIO4        17
// #define IO_PIN_GPIO3        16
// #define IO_PIN_GPIO2        15
// #define IO_PIN_GPIO1        14
// #define IO_PIN_GPIO0        13

// #define IO_PIN_ADC7         12
// #define IO_PIN_ADC6         11
// #define IO_PIN_ADC5         10
// #define IO_PIN_ADC4         9
// #define IO_PIN_ADC3         8
// #define IO_PIN_ADC2         7
// #define IO_PIN_ADC1         6
// #define IO_PIN_ADC0         5

// #define IO_PIN_PWM2         4
// #define IO_PIN_PWM1         3
// #define IO_PIN_PWM0         2

// #define IO_PIN_TX           1
// #define IO_PIN_RX           0

// // LED color definitions
#define IO_PIN_LED_GREEN    IO_PIN_LED0
#define IO_PIN_LED_YELLOW   IO_PIN_LED1
#define IO_PIN_LED_RED      IO_PIN_LED2


#endif
