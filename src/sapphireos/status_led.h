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

#ifndef _STATUS_LED_H
#define _STATUS_LED_H

#define LED_TIME_SYNC_INTERVAL 512


#define STATUS_LED_GREEN 		0
#define STATUS_LED_YELLOW 		1
#define STATUS_LED_RED 			2
#define STATUS_LED_BLUE         3
#define STATUS_LED_PURPLE       4
#define STATUS_LED_TEAL         5
#define STATUS_LED_WHITE        6



void status_led_v_init( void );
void status_led_v_enable( void );
void status_led_v_disable( void );
void status_led_v_set( uint8_t state, uint8_t led );

void hal_status_led_v_init( void );

#endif
