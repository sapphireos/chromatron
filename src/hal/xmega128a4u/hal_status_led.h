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

#ifndef _HAL_STATUS_LED_H
#define _HAL_STATUS_LED_H

#include "status_led.h"

#define LED_BLINK_NORMAL    500

// #define LED_RED_PORT    PORTC
// #define LED_RED_PIN     4
// #define LED_GREEN_PORT  PORTC
// #define LED_GREEN_PIN   5
// #define LED_BLUE_PORT   PORTD
// #define LED_BLUE_PIN    4

// rev 0.2
#define LED_RED_PORT    PORTC
#define LED_RED_PIN     4
#define LED_GREEN_PORT  PORTD
#define LED_GREEN_PIN   5
#define LED_BLUE_PORT   PORTD
#define LED_BLUE_PIN    4


#define STATUS_LED_BLUE         10
#define STATUS_LED_PURPLE       11
#define STATUS_LED_TEAL         12
#define STATUS_LED_WHITE        13


#endif
