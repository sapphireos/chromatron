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

#include <ESP8266WiFi.h>

extern "C"{
    #include "crc.h"
    #include "wifi_cmd.h"
}

#define MAX_PORT_DEPTH 32
#define RX_BUF_SIZE 32
#define TX_BUF_SIZE 8


uint8_t wifi_u8_get_status( void );
void wifi_v_set_status_bits( uint8_t bits );
void wifi_v_clr_status_bits( uint8_t bits );
void wifi_v_send_status( void );

IPAddress wifi_ip_get_ip( void );
IPAddress wifi_ip_get_subnet( void );

void wifi_v_init( void );
void wifi_v_process( void );

void wifi_v_send_udp( wifi_msg_udp_header_t *header, uint8_t *data );
void wifi_v_set_ap( char *ssid, char *pass );
void wifi_v_set_ap_mode( char *_ssid, char *_pass );
void wifi_v_shutdown( void );

void wifi_v_scan( void );

void wifi_v_set_ports( uint16_t _ports[WIFI_MAX_PORTS] );

bool wifi_b_rx_udp_pending( void );
int8_t wifi_i8_get_rx_udp_header( wifi_msg_udp_header_t *header );
uint8_t* wifi_u8p_get_rx_udp_data( void );
void wifi_v_rx_udp_clear_last( void );

uint32_t wifi_u32_get_rx_udp_overruns( void );
uint32_t wifi_u32_get_udp_received( void );
uint32_t wifi_u32_get_udp_sent( void );

#endif
