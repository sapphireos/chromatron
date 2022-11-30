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

#ifndef _ONEWIRE_H
#define _ONEWIRE_H


// 1-wire delays, from Maxim app note AN126:
// these are all microseconds
#define ONEWIRE_DELAY_A 6
#define ONEWIRE_DELAY_B 64
#define ONEWIRE_DELAY_C 60
#define ONEWIRE_DELAY_D 10
#define ONEWIRE_DELAY_E 9
#define ONEWIRE_DELAY_F 55
#define ONEWIRE_DELAY_G 0
#define ONEWIRE_DELAY_H 480
#define ONEWIRE_DELAY_I 70
#define ONEWIRE_DELAY_J 410


void onewire_v_init( void );


#endif
