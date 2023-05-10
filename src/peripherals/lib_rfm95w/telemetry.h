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

#define TELEMETRY_BASE_STATION_RELAY_PORT   32123

typedef struct __attribute__((packed)){
    uint8_t flags;
    uint32_t sample;
    int16_t base_rssi;
    int16_t base_snr;
    // uint16_t batt_volts;
    // uint16_t charge_current;
    // uint32_t als;
    // int8_t batt_temp;
    // int8_t case_temp;
    // int8_t ambient_temp;
    // uint8_t batt_fault;
    // uint16_t pixel_power;
    // uint8_t vm_status;
} telemetry_msg_remote_data_0_t;

typedef struct __attribute__((packed)){
    uint64_t src_addr;
    int16_t rssi;
    int16_t snr;
    telemetry_msg_remote_data_0_t msg;
} telemetry_data_entry_t;

#define TELEMETRY_FLAGS_BEACON  0b00000001
#define TELEMETRY_FLAGS_CONFIG  0b00000010
#define TELEMETRY_FLAGS_REMOTE  0b00000100

typedef struct __attribute__((packed)){
    uint8_t flags;
    uint8_t cycle;
} telemetry_msg_beacon_t;


void telemetry_v_init( void );



#endif