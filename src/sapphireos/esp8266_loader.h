// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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


#ifndef _ESP8266_LOADER_H_
#define _ESP8266_LOADER_H_

#define WIFI_LOADER_MAX_TRIES   8

#define MD5_LEN 16

#define ESP_SYNC_TIMEOUT 		15000
#define ESP_CMD_TIMEOUT 		50000
#define ESP_CESANTA_TIMEOUT 	50000
#define ESP_ERASE_TIMEOUT 		5000000

#define ESP_CESANTA_BAUD                2000000
#define ESP_CESANTA_BAUD_USART_SETTING  BAUD_2000000

#define ESP_FW_INFO_ADDRESS     0x00012000

#define SLIP_END        0xC0
#define SLIP_ESC        0xDB
#define SLIP_ESC_END    0xDC
#define SLIP_ESC_ESC    0xDD

#define ESP_FLASH_BEGIN 0x02
#define ESP_FLASH_DATA  0x03
#define ESP_FLASH_END   0x04
#define ESP_MEM_BEGIN   0x05
#define ESP_MEM_END     0x06
#define ESP_MEM_DATA    0x07
#define ESP_SYNC        0x08
#define ESP_WRITE_REG   0x09
#define ESP_READ_REG    0x0a

typedef struct __attribute__((packed)){
    uint8_t resp;
    uint8_t opcode;
    uint16_t len;
    uint32_t val;
} esp_response_t;


typedef struct __attribute__((packed)){
    uint32_t size;
    uint32_t num_blocks;
    uint32_t block_size;
    uint32_t offset;
} esp_mem_begin_t;

typedef struct __attribute__((packed)){
    uint32_t size;
    uint32_t seq;
    uint32_t reserved0;
    uint32_t reserved1;
} esp_mem_block_t;

typedef struct __attribute__((packed)){
    uint32_t zero;
    uint32_t entrypoint;
} esp_mem_finish_t;

// Cesanta protocol
typedef struct __attribute__((packed)){
    uint32_t addr;
    uint32_t len;
    uint32_t erase;
} esp_write_flash_t;

// Cesanta protocol
typedef struct __attribute__((packed)){
    uint32_t addr;
    uint32_t len;
    uint32_t block_size;
} esp_digest_t;


void esp_v_send_header( uint8_t op, uint16_t len, uint32_t checksum );
void esp_v_command( uint8_t op, uint8_t *data, uint16_t len, uint32_t checksum );
void esp_v_send_sync( void );
// int8_t esp_i8_sync( void );
int8_t esp_i8_wait_response( uint8_t *buf, uint8_t len, uint32_t timeout );
int8_t esp_i8_load_cesanta_stub( void );
int8_t esp_i8_load_flash( file_t file );
int8_t esp_i8_md5( uint32_t len, uint8_t digest[MD5_LEN] );
void esp_v_flash_end( void );


void wifi_v_start_loader( void );
int8_t wifi_i8_loader_status( void );

#define ESP_LOADER_STATUS_BUSY      1
#define ESP_LOADER_STATUS_OK        0 
#define ESP_LOADER_STATUS_FAIL      -1

#endif


