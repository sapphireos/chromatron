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

#ifndef _TELEMETRY_H_
#define _TELEMETRY_H_

#define TELEMTRY_MAGIC_0 0x12
#define TELEMTRY_MAGIC_1 0x34
#define TELEMTRY_MAGIC_2 0x56

#define TELEMETRY_BASE_STATION_RELAY_PORT   32123

typedef struct __attribute__((packed)){
    uint8_t magic[3];
    uint8_t flags;

    uint64_t src_addr;
} telemtry_msg_header_0_t; // 12 bytes

typedef struct __attribute__((packed)){
    uint16_t batt_flags;
    uint16_t batt_volts;
    uint16_t charge_current;
    uint32_t als;
    uint8_t batt_temp;
    uint8_t case_temp;
    uint8_t ambient_temp;
    uint8_t batt_fault;
} telemtry_msg_0_t; // 14 bytes

void telemetry_v_init( void );



#endif