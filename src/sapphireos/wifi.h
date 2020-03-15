// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef _WIFI_H
#define _WIFI_H

#include "esp8266.h"

#define WIFI_SSID_LEN               32
#define WIFI_PASS_LEN               32


void wifi_v_init( void );
void wifi_v_shutdown( void );
bool wifi_b_connected( void );
bool wifi_b_attached( void );

int8_t wifi_i8_rssi( void );
void wifi_v_get_ssid( char ssid[WIFI_SSID_LEN] );
bool wifi_b_ap_mode( void );

bool wifi_b_shutdown( void );
int8_t wifi_i8_get_status( void );
uint32_t wifi_u32_get_received( void );

int8_t wifi_i8_send_udp( netmsg_t netmsg );

#endif

