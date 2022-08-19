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

#ifndef _FUEL_GAUGE_H
#define _FUEL_GAUGE_H


#define LION_MAX_VOLTAGE    4200
#define LION_MIN_VOLTAGE    2900

#define FUEL_MAX_DISCHARGE_FILE_SIZE    65536

typedef struct{
    uint8_t batt_volts;
    uint8_t pix_power;
    int8_t batt_temp;
    uint8_t reserved;
} fuel_gauge_data_t;

void fuel_v_init( void );

uint8_t fuel_u8_get_soc( void );

#endif

