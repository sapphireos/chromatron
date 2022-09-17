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


#ifndef _DATETIME_STRUCT_H
#define _DATETIME_STRUCT_H

#include <stdint.h>

typedef struct{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day; // day represents current day of the current month, not current day of the year (hence 8 bits)
	uint8_t weekday;
	uint8_t month;
	uint16_t year;
} datetime_t;

#endif