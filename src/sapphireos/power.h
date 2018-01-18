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

#ifndef _POWER_H
#define _POWER_H

typedef int8_t pwr_wake_source_t8;
#define PWR_WAKE_SOURCE_ANY         0
#define PWR_WAKE_SOURCE_TIMER       1
#define PWR_WAKE_SOURCE_WCOM        2
#define PWR_WAKE_SOURCE_UART        3
#define PWR_WAKE_SOURCE_SIGNAL      4

#define PWR_WAKE_SOURCE_APP0        100
#define PWR_WAKE_SOURCE_APP1        101
#define PWR_WAKE_SOURCE_APP2        102
#define PWR_WAKE_SOURCE_APP3        103
#define PWR_WAKE_SOURCE_APP4        104
#define PWR_WAKE_SOURCE_APP5        105
#define PWR_WAKE_SOURCE_APP6        106
#define PWR_WAKE_SOURCE_APP7        107



void pwr_v_init( void );

void pwr_v_wake_up( pwr_wake_source_t8 source );
bool pwr_b_is_wake_event_set( void );
pwr_wake_source_t8 pwr_i8_get_wake_source( void );

void pwr_v_sleep( void );
int8_t pwr_i8_enter_sleep_mode( void );

void pwr_v_shutdown( void );
void pwr_v_shutdown_leds( void );
void pwr_v_shutdown_io( void );


#endif

