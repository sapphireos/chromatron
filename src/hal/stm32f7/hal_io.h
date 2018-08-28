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

#ifndef _HAL_IO_H
#define _HAL_IO_H

#include "system.h"
#include "io_intf.h"
#include "cpu.h"

#include "stm32f7xx_hal.h"

#define IO_PIN_0       		0 // PA4
#define IO_PIN_1      		1 // PA5
#define IO_PIN_2       		2 // PA6
#define IO_PIN_3       		3 // PA7
#define IO_PIN_4       		4 // PB10
#define IO_PIN_5       		5 // PB11
#define IO_PIN_COUNT        6

#define IO_PIN0_PORT        GPIOA
#define IO_PIN0_PIN         LL_GPIO_PIN_4
#define IO_PIN1_PORT        GPIOA
#define IO_PIN1_PIN         LL_GPIO_PIN_5
#define IO_PIN2_PORT        GPIOA
#define IO_PIN2_PIN         LL_GPIO_PIN_6
#define IO_PIN3_PORT        GPIOA
#define IO_PIN3_PIN         LL_GPIO_PIN_7
#define IO_PIN4_PORT        GPIOB
#define IO_PIN4_PIN         LL_GPIO_PIN_10
#define IO_PIN5_PORT        GPIOB
#define IO_PIN5_PIN         LL_GPIO_PIN_11


#endif
