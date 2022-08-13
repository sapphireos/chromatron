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

#ifndef _DATETIME_H
#define _DATETIME_H

#include <stdint.h>
#include "datetime_struct.h"

// Friday, January 1, 2010, 00:00:00 (midnight)
#define SAPPHIRE_NTP_EPOCH 		3471292800

// minimum string length for iso 8601 date/time string.
// multiple formats are possible, but the format used by
// this implementation is 19 bytes long
#define ISO8601_STRING_MIN_LEN 		20

// long enough for ms precision
#define ISO8601_STRING_MIN_LEN_MS	24

// time interval definitions
#define SECONDS_PER_MINUTE 		60
#define MINUTES_PER_HOUR 		60
#define HOURS_PER_DAY 			24
#define DAYS_PER_YEAR			365 // add 1 for leap years
#define MONTHS_PER_YEAR			12

#define SECONDS_PER_DAY         ( (uint32_t)SECONDS_PER_MINUTE * (uint32_t)MINUTES_PER_HOUR * (uint32_t)HOURS_PER_DAY )

#define DATE_TIME_DELIMITER		11

// don't change this until we switch to something other than the Gregorian calendar
#define FEBRUARY_LEAP_YEAR_DAYS 29

// month names
#define JANUARY		1
#define FEBRUARY	2
#define MARCH		3
#define APRIL		4
#define MAY			5
#define JUNE		6
#define JULY		7
#define AUGUST		8
#define SEPTEMBER	9
#define OCTOBER		10
#define NOVEMBER	11
#define DECEMBER	12

// day names
#define SUNDAY 		0
#define MONDAY 		1
#define TUESDAY 	2
#define WEDNESDAY 	3
#define THURSDAY 	4
#define FRIDAY 		5
#define SATURDAY 	6

void datetime_v_init( void );
    
void datetime_v_update( void );
void datetime_v_parse_iso8601( char *iso8601, uint8_t len, datetime_t *datetime );
void datetime_v_to_iso8601( char *iso8601, uint8_t len, datetime_t *datetime );
void datetime_v_get_epoch( datetime_t *datetime );
void datetime_v_seconds_to_datetime( uint32_t seconds, datetime_t *datetime );
uint32_t datetime_u32_datetime_to_seconds( const datetime_t *datetime );
void datetime_v_increment_seconds( datetime_t *datetime );
bool datetime_b_is_leap_year( const datetime_t *datetime );




#endif

