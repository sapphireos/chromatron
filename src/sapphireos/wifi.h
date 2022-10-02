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

#ifndef _WIFI_H
#define _WIFI_H

#include "ip.h"
#include "netmsg.h"


// maximum power setting the hardware will support.
// technically this is 20.5 dbm.
// however, 20.5 dbm is only spec'd for 802.11b.
// if we use 802.11n, the radio is only spec'd for 17.5 dbm.
// since we are using an integer, we will set to 17 for our max.
#define WIFI_MAX_HW_TX_POWER    17 // this is for the ESP8266.  The ESP32 may be able to run a bit higher.

#define WIFI_CONNECT_TIMEOUT    10000

#define WIFI_STATE_ERROR        -2
#define WIFI_STATE_BOOT         -1
#define WIFI_STATE_UNKNOWN      0
#define WIFI_STATE_ALIVE        1
#define WIFI_STATE_SHUTDOWN     2

#define WIFI_AP_MIN_PASS_LEN    8

#define WIFI_SSID_LEN               32
#define WIFI_PASS_LEN               32




void wifi_v_init( void );
void wifi_v_shutdown( void );
void wifi_v_powerup( void );

bool wifi_b_connected( void );
bool wifi_b_attached( void );

int8_t wifi_i8_rssi( void );
void wifi_v_get_ssid( char ssid[WIFI_SSID_LEN] );
void wifi_v_switch_to_ap( void );
bool wifi_b_ap_mode( void );

int8_t wifi_i8_get_status( void );
int8_t wifi_i8_get_channel( void );

int8_t wifi_i8_send_udp( netmsg_t netmsg );

int8_t wifi_i8_igmp_join( ip_addr4_t mcast_ip );
int8_t wifi_i8_igmp_leave( ip_addr4_t mcast_ip );

void hal_wifi_v_init( void );
int8_t hal_wifi_i8_igmp_join( ip_addr4_t mcast_ip );
int8_t hal_wifi_i8_igmp_leave( ip_addr4_t mcast_ip );

void wifi_v_reset_scan_timeout( void );

#endif

