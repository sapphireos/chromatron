/*
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
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "fs.h"
#include "threading.h"
#include "target.h"
#include "hal_eeprom.h"
#include "eeprom.h"
#include "ip.h"
#include "types.h"
#include "keyvalue.h"
#include "crypt.h"

#define CFG_VERSION                     14

#define CFG_KEY_SIZE                    ( CRYPT_KEY_SIZE )
#define CFG_STR_LEN                     ( 64 )

// #define ENABLE_CFG_VFILE

// #define CFG_INCLUDE_MANUAL_IP           

// error log section size:
#define CFG_FILE_SIZE_ERROR_LOG         384

// error log section start:
#define CFG_FILE_ERROR_LOG_START        ( EE_ARRAY_SIZE - CFG_FILE_SIZE_ERROR_LOG )

// main config section size
#define CFG_FILE_MAIN_SIZE              ( EE_ARRAY_SIZE - ( CFG_FILE_SIZE_ERROR_LOG + CFG_FILE_SECURE_SIZE ) )

// main config section start
#define CFG_FILE_MAIN_START             ( 16 ) // leave room for "Sapphire" ID string
#define CFG_FILE_MAIN_END               ( CFG_FILE_MAIN_START + CFG_FILE_MAIN_SIZE )


// secure section
#define CFG_FILE_SECURE_SIZE            128
#define CFG_FILE_SECURE_START           CFG_FILE_MAIN_END
#define CFG_FILE_SECURE_END             ( CFG_FILE_SECURE_START + CFG_FILE_SECURE_SIZE );

#define CFG_PARAM_EMPTY_BLOCK           (uint32_t)0xffffffff


#define CFG_PARAM_VERSION 	    				__KV__cfg_version

#define CFG_PARAM_IP_ADDRESS 					__KV__ip
#define CFG_PARAM_IP_SUBNET_MASK				__KV__subnet
#define CFG_PARAM_DNS_SERVER                    __KV__dns_server

#define CFG_PARAM_DEVICE_ID                     __KV__device_id

#define CFG_PARAM_BOARD_TYPE                    __KV__board_type

#define CFG_PARAM_FIRMWARE_LOAD_COUNT           __KV__fw_load_count
#define CFG_PARAM_MAX_LOG_SIZE                  __KV__max_log_size

#define CFG_PARAM_CATBUS_DATA_PORT              __KV__catbus_data_port

#define CFG_PARAM_MANUAL_IP_ADDRESS 		    __KV__manual_ip
#define CFG_PARAM_MANUAL_SUBNET_MASK			__KV__manual_subnet
#define CFG_PARAM_MANUAL_DNS_SERVER             __KV__manual_dns_server
#define CFG_PARAM_MANUAL_INTERNET_GATEWAY       __KV__manual_internet_gateway

#define CFG_PARAM_ENABLE_MANUAL_IP              __KV__enable_manual_ip

#define CFG_PARAM_ENABLE_LED_QUIET_MODE         __KV__enable_led_quiet
#define CFG_PARAM_ENABLE_LOW_POWER_MODE         __KV__enable_low_power

#define CFG_PARAM_SNTP_SERVER                   __KV__sntp_server
#define CFG_PARAM_SNTP_SYNC_INTERVAL            __KV__sntp_sync_interval
#define CFG_PARAM_ENABLE_SNTP                   __KV__enable_sntp

#define CFG_PARAM_WIFI_SSID                     __KV__wifi_ssid
#define CFG_PARAM_WIFI_PASSWORD                 __KV__wifi_password
#define CFG_PARAM_WIFI_MD5                      __KV__wifi_md5
#define CFG_PARAM_WIFI_FW_LEN                   __KV__wifi_fw_len
#define CFG_PARAM_WIFI_AP_SSID                  __KV__wifi_ap_ssid
#define CFG_PARAM_WIFI_AP_PASSWORD              __KV__wifi_ap_password
#define CFG_PARAM_WIFI_ENABLE_AP                __KV__wifi_enable_ap

#define CFG_PARAM_META_TAG_0                    __KV__meta_tag_name
#define CFG_PARAM_META_TAG_1                    __KV__meta_tag_location
#define CFG_PARAM_META_TAG_2                    __KV__meta_tag_0
#define CFG_PARAM_META_TAG_3                    __KV__meta_tag_1
#define CFG_PARAM_META_TAG_4                    __KV__meta_tag_2
#define CFG_PARAM_META_TAG_5                    __KV__meta_tag_3
#define CFG_PARAM_META_TAG_6                    __KV__meta_tag_4
#define CFG_PARAM_META_TAG_7                    __KV__meta_tag_5

#define CFG_PARAM_RECOVERY_MODE_BOOTS          __KV__cfg_recovery_boot_count

// DO NOT ASSIGN IDs >= CFG_TOTAL_BLOCKS!!!!

// Key IDs
#define CFG_MAX_KEYS                            1


typedef struct{
    ip_addr_t ip;
    ip_addr_t subnet;
    ip_addr_t dns_server;
    ip_addr_t internet_gateway;
} cfg_ip_config_t;

typedef struct{
    char log[CFG_FILE_SIZE_ERROR_LOG];
} cfg_error_log_t;


int8_t cfg_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len );

uint16_t cfg_u16_total_blocks( void );
uint16_t cfg_u16_free_blocks( void );

void cfg_v_set( catbus_hash_t32 parameter, void *value );
int8_t cfg_i8_get( catbus_hash_t32 parameter, void *value );

void cfg_v_erase( catbus_hash_t32 parameter );

void cfg_v_set_security_key( uint8_t key_id, const uint8_t key[CFG_KEY_SIZE] );
void cfg_v_get_security_key( uint8_t key_id, uint8_t key[CFG_KEY_SIZE] );

void cfg_v_set_ip_config( cfg_ip_config_t *_ip_config );

bool cfg_b_ip_configured( void );
bool cfg_b_manual_ip( void );
extern bool cfg_b_is_gateway( void ) __attribute__((weak));

bool cfg_b_get_boolean( catbus_hash_t32 parameter );
uint16_t cfg_u16_get_board_type( void );

void cfg_v_reset_on_next_boot( void );
void cfg_v_default_all( void );

void cfg_v_write_config( void );
void cfg_v_init( void );

void cfg_v_write_error_log( cfg_error_log_t *log );
void cfg_v_read_error_log( cfg_error_log_t *log );
void cfg_v_erase_error_log( void );
uint16_t cfg_u16_error_log_size( void );


#endif
