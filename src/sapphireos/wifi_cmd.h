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

#ifndef _WIFI_CMD_H
#define _WIFI_CMD_H

#include "ip.h"
#include "catbus_common.h"

#ifndef ESP8266
typedef ip_addr_t sos_ip_addr_t;
#endif

#define WIFI_UDP_BUF_LEN            548

#define WIFI_MAX_PORTS              16

#define WIFI_SSID_LEN               32
#define WIFI_PASS_LEN               32

#define WIFI_RGB_DATA_N_PIXELS      60
#define WIFI_HSV_DATA_N_PIXELS      40


#define WIFI_STATUS_ERR                 0x80
#define WIFI_STATUS_CONNECTED           0x40
#define WIFI_STATUS_AP_MODE             0x10
#define WIFI_STATUS_IRQ_FLAG            0x08
#define WIFI_STATUS_160MHz              0x02


#define WIFI_COMM_RESET                 0x27
#define WIFI_COMM_DATA                  0x36
#define WIFI_COMM_QUERY_READY           0x68
#define WIFI_COMM_IDLE                  0xff

typedef struct __attribute__((packed)){
    uint8_t data_id;
    uint16_t len;
    uint8_t reserved;
    uint16_t crc;
} wifi_data_header_t;

#define WIFI_BUF_SLACK_SPACE            1
#define WIFI_BUF_LEN                    640
#define WIFI_MAX_DATA_LEN       (WIFI_BUF_LEN - (sizeof(wifi_data_header_t) + 1 + WIFI_BUF_SLACK_SPACE))


#define WIFI_MAIN_BUF_LEN               255
#define WIFI_MAIN_MAX_DATA_LEN  (WIFI_MAIN_BUF_LEN - (sizeof(wifi_data_header_t) + 1 + WIFI_BUF_SLACK_SPACE))


typedef struct __attribute__((packed)){
    uint8_t flags;
} wifi_msg_status_t;
#define WIFI_DATA_ID_STATUS             0x01

typedef struct __attribute__((packed)){
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_SSID_LEN];
} wifi_msg_connect_t;
#define WIFI_DATA_ID_CONNECT            0x02

typedef struct __attribute__((packed)){
    uint16_t version;
    uint8_t mac[6];
    sos_ip_addr_t ip;
    sos_ip_addr_t subnet;
    sos_ip_addr_t gateway;
    sos_ip_addr_t dns;
    int8_t rssi;
    uint16_t rx_udp_overruns;
    uint32_t udp_received;
    uint32_t udp_sent;
    uint16_t comm_errors;
    uint16_t mem_heap_peak;
    uint16_t mem_used;
    uint16_t intf_max_time;
    uint16_t vm_max_time;
    uint16_t wifi_max_time;
    uint16_t mem_max_time;
    uint16_t intf_avg_time;
    uint16_t vm_avg_time;
    uint16_t wifi_avg_time;
    uint16_t mem_avg_time;
} wifi_msg_info_t;
#define WIFI_DATA_ID_INFO               0x03

typedef struct __attribute__((packed)){
    uint16_t ports[WIFI_MAX_PORTS];
} wifi_msg_ports_t;
#define WIFI_DATA_ID_PORTS              0x04

// gfx param data is defined in gfx_lib.h
#define WIFI_DATA_ID_GFX_PARAMS         0x05


typedef struct __attribute__((packed)){
    uint16_t r;
    uint16_t g;
    uint16_t b;
} wifi_msg_rgb_pix0_t;
#define WIFI_DATA_ID_RGB_PIX0           0x06

typedef struct __attribute__((packed)){
    uint16_t index;
    uint8_t count;
    uint8_t rgbd_array[WIFI_RGB_DATA_N_PIXELS * 4];
} wifi_msg_rgb_array_t;
#define WIFI_DATA_ID_RGB_ARRAY          0x07

typedef struct __attribute__((packed)){
    char ssid[WIFI_SSID_LEN];
    char pass[WIFI_SSID_LEN];
} wifi_msg_ap_connect_t;
#define WIFI_DATA_ID_AP_MODE            0x08

typedef struct __attribute__((packed)){
    uint16_t index;
    uint8_t count;
    uint8_t padding;
    uint8_t hsv_array[WIFI_HSV_DATA_N_PIXELS * 6];
} wifi_msg_hsv_array_t;
#define WIFI_DATA_ID_HSV_ARRAY          0x0A


typedef struct __attribute__((packed)){
    sos_ip_addr_t addr;
    uint16_t lport;
    uint16_t rport;
    uint16_t len;
    uint16_t crc;
} wifi_msg_udp_header_t;
#define WIFI_DATA_ID_UDP_BUF_READY     0x0B
#define WIFI_DATA_ID_UDP_HEADER        0x10
#define WIFI_DATA_ID_UDP_DATA          0x11
#define WIFI_DATA_ID_UDP_EXT           0x12

#define WIFI_DATA_ID_WIFI_SCAN         0x13

typedef struct __attribute__((packed)){
    uint32_t ssid_hash;
    int32_t rssi;
} wifi_network_t;

#define WIFI_SCAN_RESULTS_LEN            14
typedef struct __attribute__((packed)){
    uint8_t count;
    uint8_t padding[3];
    wifi_network_t networks[WIFI_SCAN_RESULTS_LEN];
} wifi_msg_scan_results_t;
#define WIFI_DATA_ID_WIFI_SCAN_RESULTS 0x14

#define WIFI_DATA_ID_RUN_FADER          0x27

typedef struct __attribute__((packed)){
    uint8_t tag;
    uint8_t padding[3];
    catbus_meta_t meta;
} wifi_msg_kv_data_t;
#define WIFI_DATA_ID_KV_DATA            0x32

#define WIFI_DATA_ID_DEBUG_PRINT        0x40

typedef struct __attribute__((packed)){
    bool low_power;
    bool led_quiet;
    int8_t midi_channel;
    uint8_t padding[1];
} wifi_msg_set_options_t;
#define WIFI_DATA_ID_SET_OPTIONS        0x60

typedef struct __attribute__((packed)){
    uint32_t vm_id;
    uint16_t func_addr;
} wifi_msg_vm_run_func_t;
#define WIFI_DATA_ID_VM_RUN_FUNC        0x51


#define WIFI_DATA_ID_SHUTDOWN           0x98

#endif




