// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#ifndef _HAL_TIMERS_H
#define _HAL_TIMERS_H

/*

Available timers on the Xmega128A4U:

16 bit type 0/1

TCC0 - PWM system
TCC1 - System timer
TCD0 - Graphics timer
TCD1 - Wifi timer
TCE0 - Reserved for application use



*/


#define TIMER_TICKS_TO_MICROSECONDS(a) ( (uint64_t)a * 2 )
#define MICROSECONDS_TO_TIMER_TICKS(a) ( a / 2 )

void hal_timer_v_init( void );

#endif
