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

#define FUEL_GAUGE_VOLTS_FILTER_DEPTH   16

// #define FUEL_MAX_DISCHARGE_FILE_SIZE        65536

// #define BATT_RECORDER_RATE                  300

// typedef struct __attribute__((packed)){
//     uint8_t flags;
//     uint8_t batt_volts;
//     uint8_t pix_power;
//     int8_t batt_temp;
// } fuel_gauge_data_t;
// #define FUEL_RECORD_TYPE_BLANK              0b00000000
// #define FUEL_RECORD_TYPE_IDLE               0b00100000
// #define FUEL_RECORD_TYPE_DISCHARGE          0b10000000
// #define FUEL_RECORD_TYPE_CHARGE             0b01000000

// typedef struct __attribute__((packed)){
//     uint8_t flags;
//     uint16_t record_id;
//     uint8_t rate;
// } fuel_gauge_record_start_t;
// #define FUEL_RECORD_TYPE_RECORD_START       0b11000000


void fuel_v_init( void );

uint8_t fuel_u8_get_soc( void );

void fuel_v_do_soc( void );

#endif

