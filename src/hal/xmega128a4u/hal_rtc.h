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

#if 0

#ifndef _HAL_RTC_H
#define _HAL_RTC_H

void hal_rtc_v_init( void );
uint16_t hal_rtc_u16_get_period( void );
void hal_rtc_v_set_period( uint16_t period );
uint16_t hal_rtc_u16_get_time( void );

void hal_rtc_v_irq( void ) __attribute__((weak));

#endif

#endif