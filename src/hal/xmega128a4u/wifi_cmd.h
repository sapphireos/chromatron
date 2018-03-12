// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifndef ESP8266
typedef ip_addr_t sos_ip_addr_t;
#endif

#define WIFI_UDP_BUF_LEN            548

#define WIFI_MAX_PORTS              16

#define WIFI_SSID_LEN               32
#define WIFI_PASS_LEN               32

#define WIFI_RGB_DATA_N_PIXELS      60



#define WIFI_STATUS_ERR                 0x80
#define WIFI_STATUS_CONNECTED           0x40
#define WIFI_STATUS_RX_MSG              0x20
#define WIFI_STATUS_AP_MODE             0x10
#define WIFI_STATUS_IRQ_FLAG            0x08


#define WIFI_COMM_RESET                 0x27
#define WIFI_COMM_DATA                  0x36
#define WIFI_COMM_QUERY_READY           0x68
#define WIFI_COMM_IDLE                  0xff

typedef struct __attribute__((packed)){
    uint8_t data_id;
    uint8_t len;
    uint8_t msg_id;
    uint16_t crc;
} wifi_data_header_t;

#define WIFI_BUF_SLACK_SPACE            4
#define WIFI_BUF_LEN                    128
#define WIFI_MAX_DATA_LEN (WIFI_BUF_LEN - (sizeof(wifi_data_header_t) + 1 + WIFI_BUF_SLACK_SPACE))

#define WIFI_MAIN_BUF_LEN               255
#define WIFI_MAIN_MAX_DATA_LEN (WIFI_MAIN_BUF_LEN - (sizeof(wifi_data_header_t) + 1 + WIFI_BUF_SLACK_SPACE))


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
    uint16_t rx_udp_fifo_overruns;
    uint16_t rx_udp_port_overruns;
    uint32_t udp_received;
    uint32_t udp_sent;
    uint16_t comm_errors;
    uint16_t mem_heap_peak;
    uint16_t intf_max_time;
    uint16_t vm_max_time;
    uint16_t wifi_max_time;
    uint16_t mem_max_time;
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
    uint32_t free_heap;
} wifi_msg_debug_t;
#define WIFI_DATA_ID_DEBUG              0x09


typedef struct __attribute__((packed)){
    sos_ip_addr_t addr;
    uint16_t lport;
    uint16_t rport;
    uint16_t len;
    uint16_t crc;
} wifi_msg_udp_header_t;
#define WIFI_DATA_ID_UDP_HEADER        0x10
#define WIFI_DATA_ID_UDP_DATA          0x11

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


#define WIFI_DATA_ID_RESET_VM          0x20
#define WIFI_DATA_ID_LOAD_VM           0x21
#define WIFI_DATA_ID_VM_INFO           0x22


#define WIFI_DATA_FRAME_SYNC_MAX_DATA   16
typedef struct __attribute__((packed)){
    uint64_t rng_seed;
    uint16_t frame_number;
    uint16_t data_index;
    uint16_t data_count;
    uint16_t padding;
    // make sure we're 32 bit aligned here
    int32_t data[WIFI_DATA_FRAME_SYNC_MAX_DATA];
} wifi_msg_vm_frame_sync_t;
#define WIFI_DATA_ID_VM_FRAME_SYNC      0x25

#define WIFI_DATA_ID_RUN_VM             0x26
#define WIFI_DATA_ID_RUN_FADER          0x27
#define WIFI_DATA_ID_REQUEST_FRAME_SYNC 0x28

typedef struct __attribute__((packed)){
    uint16_t frame_number;
    uint8_t status;
} wifi_msg_vm_frame_sync_status_t;
#define WIFI_DATA_ID_FRAME_SYNC_STATUS  0x29


#define WIFI_DATA_ID_KV_BATCH           0x31
#define WIFI_KV_BATCH_LEN               14
typedef struct __attribute__((packed)){
    uint32_t hash;
    int32_t data;
} wifi_kv_batch_entry_t;

typedef struct __attribute__((packed)){
    uint8_t count;
    uint8_t padding[3];
    wifi_kv_batch_entry_t entries[WIFI_KV_BATCH_LEN];
} wifi_msg_kv_batch_t;

#define WIFI_DATA_ID_DEBUG_PRINT        0x40

#endif
