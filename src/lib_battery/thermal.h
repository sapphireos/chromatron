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

#ifndef _THERMAL_H
#define _THERMAL_H


#define FAN_THRESH_BATT_MIN  38
#define FAN_THRESH_BATT_MAX  40
#define FAN_THRESH_CASE_MIN  52
#define FAN_THRESH_CASE_MAX  55

#define FAN_MIN_ON_TIME      600 // tenths of a second

void thermal_v_init( void );

int8_t thermal_i8_get_case_temp( void );
int8_t thermal_i8_get_ambient_temp( void );


#endif
