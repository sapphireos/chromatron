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

#ifndef _WIFI_CMD_H
#define _WIFI_CMD_H

#include "ip.h"

#define WIFI_UDP_BUF_LEN            548

#define WIFI_MAX_PORTS              16
#define WIFI_MAX_APS                4

#define WIFI_SSID_LEN               32
#define WIFI_PASS_LEN               32

#define WIFI_RGB_DATA_N_PIXELS      60
#define WIFI_HSV_DATA_N_PIXELS      40


#define WIFI_STATUS_ERR                 0x80
#define WIFI_STATUS_CONNECTED           0x40
#define WIFI_STATUS_CONNECTING          0x20
#define WIFI_STATUS_AP_MODE             0x10
#define WIFI_STATUS_NET_RX              0x08
#define WIFI_STATUS_160MHz              0x02

#define WIFI_COMM_DATA                  0xD1
#define WIFI_COMM_ACK                   0xA2
#define WIFI_COMM_NAK                   0xF3
#define WIFI_COMM_RTS                   0xB4
#define WIFI_COMM_CTS                   0xC5
#define WIFI_COMM_READY                 0xD6
#define WIFI_COMM_QUERY_READY           0xE0
#define WIFI_COMM_GET_MSG               0xE5


typedef struct __attribute__((packed)){
    uint8_t data_id;
    uint16_t len;
    uint8_t flags;
    uint16_t reserved;
    uint16_t header_crc;
} wifi_data_header_t;

#define WIFI_ESP_BUF_SIZE               128        
#define WIFI_MAX_SINGLE_SHOT_LEN        ( WIFI_ESP_BUF_SIZE - sizeof(wifi_data_header_t) )

#define WIFI_MAX_MCU_BUF                128





#define WIFI_DATA_ID_SCAN               0x10

typedef struct __attribute__((packed)){
    uint8_t flags;
} wifi_msg_status_t;
#define WIFI_DATA_ID_STATUS             0x11

typedef struct __attribute__((packed)){
    char ssid[WIFI_MAX_APS][WIFI_SSID_LEN];
    char pass[WIFI_MAX_APS][WIFI_SSID_LEN];
} wifi_msg_connect_t;
#define WIFI_DATA_ID_CONNECT            0x12

typedef struct __attribute__((packed)){
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_patch;
    uint8_t mac[6];
    ip_addr4_t ip;
    ip_addr4_t subnet;
    ip_addr4_t gateway;
    ip_addr4_t dns;
    int8_t rssi;
    uint16_t rx_udp_overruns;
    uint32_t udp_received;
    uint32_t udp_sent;
    uint16_t comm_errors;
    uint16_t mem_heap_peak;
    uint16_t mem_used;
    uint16_t intf_max_time;
    uint16_t wifi_max_time;
    uint16_t mem_max_time;
    uint16_t intf_avg_time;
    uint16_t wifi_avg_time;
    uint16_t mem_avg_time;
    int8_t wifi_router;
    int8_t wifi_channel;
    uint32_t flash_id;
} wifi_msg_info_t;
#define WIFI_DATA_ID_INFO               0x13

#define WIFI_DATA_ID_DEBUG_PRINT        0x14

typedef struct __attribute__((packed)){
    bool low_power;
    bool led_quiet;
    bool high_speed;
    uint8_t tx_power;
    bool mdns_enable;
} wifi_msg_set_options_t;
#define WIFI_DATA_ID_SET_OPTIONS        0x15

#define WIFI_DATA_ID_SHUTDOWN           0x1F

typedef struct __attribute__((packed)){
    uint16_t ports[WIFI_MAX_PORTS];
} wifi_msg_ports_t;
#define WIFI_DATA_ID_PORTS              0x21

typedef struct __attribute__((packed)){
    ip_addr4_t addr;
    uint16_t lport;
    uint16_t rport;
    uint16_t len;
} wifi_msg_udp_header_t;
#define WIFI_DATA_ID_PEEK_UDP          0x22
#define WIFI_DATA_ID_GET_UDP           0x23
#define WIFI_DATA_ID_SEND_UDP          0x24

typedef struct __attribute__((packed)){
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_SSID_LEN];
} wifi_msg_ap_connect_t;
#define WIFI_DATA_ID_AP_MODE            0x25

// #define WIFI_DATA_ID_WIFI_SCAN         0x26

// typedef struct __attribute__((packed)){
//     uint32_t ssid_hash;
//     int32_t rssi;
// } wifi_network_t;

// #define WIFI_SCAN_RESULTS_LEN            14
// typedef struct __attribute__((packed)){
//     uint8_t count;
//     uint8_t padding[3];
//     wifi_network_t networks[WIFI_SCAN_RESULTS_LEN];
// } wifi_msg_scan_results_t;
// #define WIFI_DATA_ID_WIFI_SCAN_RESULTS 0x27

#endif




