/*
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
 */

#ifndef _COMMAND2_H
#define _COMMAND2_H

#include "config.h"
#include "keyvalue.h"
#include "types.h"
#include "ip.h"
#include "sockets.h"

#define CMD2_SERVER_PORT            16385
#define CMD2_UDP_SERVER_PORT        16386

#define CMD2_FILE_READ_LEN          512

#define CMD2_MAX_APP_DATA_LEN       512

#define CMD2_MAX_PACKET_LEN         600

// serial port control characters
#define CMD2_SERIAL_SOF             0xfd
#define CMD2_SERIAL_ACK             0xa1
#define CMD2_SERIAL_NAK             0x1b

typedef struct{
    uint16_t len;
    uint16_t inverted_len;
} cmd2_serial_frame_header_t;

#define CMD2_SERIAL_TIMEOUT_US      1500000

typedef uint16_t cmd2_t16;
#define CMD2_ECHO                   1

#define CMD2_REBOOT                 2
#define CMD2_SAFE_MODE              3
#define CMD2_LOAD_FW                4

#define CMD2_FORMAT_FS              10

#define CMD2_GET_FILE_ID            20
#define CMD2_CREATE_FILE            21
#define CMD2_READ_FILE_DATA         22
#define CMD2_WRITE_FILE_DATA        23
#define CMD2_REMOVE_FILE            24

#define CMD2_RESET_CFG              32

#define CMD2_SET_REG_BITS           40
#define CMD2_CLR_REG_BITS           41
#define CMD2_GET_REG                42

// #define CMD2_SET_KV                 80
// #define CMD2_GET_KV                 81

#define CMD2_SET_SECURITY_KEY       90

#define CMD2_APP_CMD_BASE           32768


typedef struct{
    cmd2_t16 cmd;
} cmd2_header_t;

typedef struct{
    uint32_t id;
    cmd2_header_t cmd_header;
} cmd2_udp_header_t;

typedef struct{
    uint8_t file_id;
    uint32_t pos;
    uint32_t len;
} cmd2_file_request_t;

typedef struct{
    uint16_t reg;
    uint8_t mask;
} cmd2_reg_bits_t;

typedef struct{
    uint8_t key_id;
    uint8_t key[CFG_KEY_SIZE];
} cmd2_set_sec_key_t;

typedef struct{
    ip_addr_t ip;
    uint16_t port;
} cmd2_set_kv_server_t;

#define CMD2_PACKET_LEN(header) ( header->len + sizeof(cmd2_header_t) )

// extern int16_t app_i16_cmd( cmd2_t16 cmd,
//                             const void *data,
//                             uint16_t len,
//                             void *response ) __attribute__((weak));

void cmd2_v_init( void );
mem_handle_t cmd2_h_process_cmd( const cmd2_header_t *cmd,
                                 int16_t len,
                                 sock_addr_t *src_addr );

#endif
