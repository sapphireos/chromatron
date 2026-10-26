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

#ifndef _HAL_BOARDS_H
#define _HAL_BOARDS_H

#include "boards.h"
#include "hal_io.h"

// sim: board pins mapped to the simulated IO pin space

#define ELITE_CASE_ADC_IO       IO_PIN_LED0
#define ELITE_AMBIENT_ADC_IO    IO_PIN_LED1
#define ELITE_FAN_IO            IO_PIN_LED2
#define ELITE_BOOST_IO          IO_PIN_LED2
#define ELITE_LED_ID_IO         IO_PIN_LED2
#define ELITE_MAIN_I2C_SCL      IO_PIN_LED1
#define ELITE_MAIN_I2C_SDA      IO_PIN_LED0
#define ELITE_AUX_I2C_SCL       IO_PIN_LED1
#define ELITE_AUX_I2C_SDA       IO_PIN_LED0

#define MINI_BTN_BOARD_BTN_0    IO_PIN_LED0
#define MINI_BTN_BOARD_BTN_1    IO_PIN_LED1
#define MINI_BTN_BOARD_BTN_2    IO_PIN_LED2
#define MINI_BTN_BOARD_BTN_3    IO_PIN_LED2

#define C2025_BTN_BOARD_BTN_1   IO_PIN_LED0
#define C2025_BTN_BOARD_BTN_2   IO_PIN_LED1
#define C2025_BTN_BOARD_BTN_3   IO_PIN_LED2

#endif
