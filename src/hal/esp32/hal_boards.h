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

// for Elite board:
// BOARD_TYPE_ELITE
#define ELITE_CASE_ADC_IO       IO_PIN_32_A7
#define ELITE_AMBIENT_ADC_IO    IO_PIN_33_A9
#define ELITE_FAN_IO            IO_PIN_19_MISO
#define ELITE_BOOST_IO          IO_PIN_4_A5


#define ELITE_LED_ID_IO         IO_PIN_25_A1 // CS pin on SPI header
#define ELITE_TILT_MOTOR_IO_1   IO_PIN_16_RX
#define ELITE_TILT_MOTOR_IO_0   IO_PIN_17_TX

// ESP32 Mini button controllers:
// BOARD_TYPE_ESP32_MINI_BUTTONS
#define MINI_BTN_BOARD_BTN_0    IO_PIN_32_A7
#define MINI_BTN_BOARD_BTN_1    IO_PIN_33_A9
#define MINI_BTN_BOARD_BTN_2    IO_PIN_25_A1
#define MINI_BTN_BOARD_BTN_3    IO_PIN_26_A0


#endif
