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
#include "driver/gpio.h"
#include "driver/rtc_io.h"

// pins 34, 35, 36, and 39 are input only on the ESP32!

// Adafruit ESP32 Feather
#define IO_PIN_13_A12		0
#define IO_PIN_12_A11		1
#define IO_PIN_27_A10		2
#define IO_PIN_33_A9		3
#define IO_PIN_15_A8		4
#define IO_PIN_32_A7		5
#define IO_PIN_14_A6		6
#define IO_PIN_22_SCL		7
#define IO_PIN_23_SDA		8
#define IO_PIN_21			9
#define IO_PIN_17_TX		10
#define IO_PIN_16_RX		11
#define IO_PIN_19_MISO		12 // not actually a MISO pin...
#define IO_PIN_18_MOSI		13
#define IO_PIN_5_SCK 		14
#define IO_PIN_4_A5 		15
#define IO_PIN_36_A4 		16
#define IO_PIN_39_A3 		17
#define IO_PIN_34_A2 		18
#define IO_PIN_25_A1 		19
#define IO_PIN_26_A0 		20

#define IO_PIN_LED0			21
#define IO_PIN_LED1			22
#define IO_PIN_LED2			23

#define IO_PIN_COUNT        24

#define IO_PIN_DEBUG        IO_PIN_26_A0

int32_t hal_io_i32_get_gpio_num( uint8_t pin );

bool hal_io_b_is_board_type_known( void );

#endif


