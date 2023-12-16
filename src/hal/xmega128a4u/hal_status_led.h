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

#ifndef _HAL_STATUS_LED_H
#define _HAL_STATUS_LED_H

#include "status_led.h"

// rev 0.2
#define LED_RED_PORT    PORTC
#define LED_RED_PIN     4
#define LED_GREEN_PORT  PORTD
#define LED_GREEN_PIN   5
#define LED_BLUE_PORT   PORTD
#define LED_BLUE_PIN    4

#define LED_TIME_SYNC_INTERVAL 512

#endif
