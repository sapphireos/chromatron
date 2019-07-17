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


#include "cpu.h"

#include "system.h"

#include "fs.h"
#include "memory.h"
#include "sockets.h"
#include "threading.h"
#include "random.h"
#include "timers.h"
#include "target.h"

#include "crc.h"
#include "config.h"
#include "fs.h"
#include "io.h"
#include "catbus_common.h"
#include "keyvalue.h"

//#define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

#include <stddef.h>
#include <string.h>

static uint16_t slowest_time;
static uint32_t slowest_id;

static uint16_t board_type;

KV_SECTION_META kv_meta_t sys_cfg_kv[] = {
    { SAPPHIRE_TYPE_UINT16,      0, KV_FLAGS_READ_ONLY,  0, cfg_i8_kv_handler,  "cfg_version" },
    { SAPPHIRE_TYPE_IPv4,        0, KV_FLAGS_READ_ONLY,  0, cfg_i8_kv_handler,  "ip" },
    { SAPPHIRE_TYPE_IPv4,        0, KV_FLAGS_READ_ONLY,  0, cfg_i8_kv_handler,  "subnet" },
    { SAPPHIRE_TYPE_IPv4,        0, KV_FLAGS_READ_ONLY,  0, cfg_i8_kv_handler,  "dns_server" },
    { SAPPHIRE_TYPE_UINT64,      0, 0,                   0, cfg_i8_kv_handler,  "device_id" },
    { SAPPHIRE_TYPE_UINT16,      0, 0,                   0, cfg_i8_kv_handler,  "fw_load_count" },
    { SAPPHIRE_TYPE_UINT32,      0, 0,                   0, cfg_i8_kv_handler,  "max_log_size" },
    { SAPPHIRE_TYPE_BOOL,        0, 0,                   0, cfg_i8_kv_handler,  "enable_led_quiet" },
    { SAPPHIRE_TYPE_BOOL,        0, 0,                   0, cfg_i8_kv_handler,  "enable_low_power" },
    { SAPPHIRE_TYPE_UINT16,      0, 0,                   0, cfg_i8_kv_handler,  "catbus_data_port" },
    // { SAPPHIRE_TYPE_BOOL,        0,                   0, cfg_i8_kv_handler,  "enable_cpu_sleep" },
    { SAPPHIRE_TYPE_UINT16,      0, 0,                   &slowest_time, 0,      "cfg_slowest_time" },
    { SAPPHIRE_TYPE_UINT32,      0, 0,                   &slowest_id, 0,        "cfg_slowest_id" },
    { SAPPHIRE_TYPE_UINT16,      0, 0,         &board_type, cfg_i8_kv_handler,  "board_type" },
    { SAPPHIRE_TYPE_UINT8,       0, 0,                   0, cfg_i8_kv_handler,  "cfg_recovery_boot_count" },
};

#ifdef CFG_INCLUDE_MANUAL_IP
KV_SECTION_META kv_meta_t sys_cfg_manual_ip_kv[] = {
    { SAPPHIRE_TYPE_IPv4,        0, 0,                   0, cfg_i8_kv_handler,  "manual_ip" },
    { SAPPHIRE_TYPE_IPv4,        0, 0,                   0, cfg_i8_kv_handler,  "manual_subnet" },
    { SAPPHIRE_TYPE_IPv4,        0, 0,                   0, cfg_i8_kv_handler,  "manual_dns_server" },
    { SAPPHIRE_TYPE_IPv4,        0, 0,                   0, cfg_i8_kv_handler,  "manual_internet_gateway" },
    { SAPPHIRE_TYPE_BOOL,        0, 0,                   0, cfg_i8_kv_handler,  "enable_manual_ip" },
}
#endif

// cached values
// these are used often, so we initialize them on startup
// static bool enable_cpu_sleep;

static cfg_ip_config_t ip_config;


#define CFG_PARAM_DATA_LEN 8
typedef struct __attribute__((packed)){
    catbus_hash_t32 id;
    sapphire_type_t8 type;
    uint8_t block_number;
    uint16_t reserved;
    uint8_t data[CFG_PARAM_DATA_LEN];
} cfg_block_t;

#define CFG_TOTAL_BLOCKS ( ( CFG_FILE_MAIN_SIZE / sizeof(cfg_block_t) ) - 1 )



static uint16_t error_log_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            ee_v_read_block( CFG_FILE_ERROR_LOG_START + pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = cfg_u16_error_log_size();
            break;

        case FS_VFILE_OP_DELETE:
            cfg_v_erase_error_log();
            break;

        default:
            len = 0;

            break;
    }

    return len;
}

static uint16_t fw_info_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            memcpy_P( ptr, (void *)FW_INFO_ADDRESS + pos + FLASH_START, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = sizeof(fw_info_t);
            break;

        default:
            len = 0;

            break;
    }

    return len;
}


#ifdef ENABLE_CFG_VFILE
static uint16_t eeprom_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            ee_v_read_block( pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = CFG_FILE_MAIN_SIZE;
            break;

        default:
            len = 0;

            break;
    }

    return len;
}
#endif

static uint16_t block_address( uint16_t block_number ){

    ASSERT( block_number < CFG_TOTAL_BLOCKS );

    return ( block_number * sizeof(cfg_block_t) ) + CFG_FILE_MAIN_START;
}

static uint8_t calc_n_blocks( uint8_t len ){

    uint8_t n_blocks = ( len / CFG_PARAM_DATA_LEN );

    // check remainder
    if( ( len % CFG_PARAM_DATA_LEN ) > 0 ){

        n_blocks++;
    }

    return n_blocks;
}

static catbus_hash_t32 read_block_id( uint16_t block_number ){

    catbus_hash_t32 hash;    
    ee_v_read_block( block_address( block_number ), (uint8_t *)&hash, sizeof(hash) );  

    return hash;
}

static uint8_t read_block_number( uint16_t block_number ){

    return ee_u8_read_byte( block_address( block_number ) + offsetof(cfg_block_t, block_number) );
}

static void erase_block( uint16_t block_number ){

    ee_v_erase_block( block_address( block_number ), sizeof(cfg_block_t) );
}

static void read_block( uint16_t block_number, cfg_block_t *block ){

    ee_v_read_block( block_address( block_number ), (uint8_t *)block, sizeof(cfg_block_t) );
}

static int16_t seek_block( catbus_hash_t32 id, uint8_t n ){

    // search array
    for( uint8_t i = 0; i < CFG_TOTAL_BLOCKS; i++ ){

        if( ( read_block_id( i ) == id ) &&
            ( read_block_number( i ) == n ) ){

            return i;
        }
    }

    return -1;
}

static void write_block( uint16_t block_number, const cfg_block_t *block ){

    uint8_t *data = (uint8_t *)block;
    uint16_t addr = block_address( block_number );

    // write everything except the ID
    ee_v_write_block( addr + 1, data + 1, sizeof(cfg_block_t) - 1 );

    // write ID
    ee_v_write_byte_blocking( addr, block->id );
}

static int16_t get_free_block( void ){

    for( uint16_t i = 0; i < CFG_TOTAL_BLOCKS; i++ ){

        if( read_block_id( i ) == CFG_PARAM_EMPTY_BLOCK ){

            return i;
        }
    }

    return -1;
}

// scan and remove blocks that have a KV type mismatch, or are
// not listed in the KV system
static void clean_blocks( void ){

    for( uint16_t i = 0; i < CFG_TOTAL_BLOCKS; i++ ){

        cfg_block_t block;

        read_block( i, &block );

        // check ID
        if( block.id == CFG_PARAM_EMPTY_BLOCK ){

            // empty block
            continue;
        }

        // search for duplicate blocks
        for( uint16_t j = 0; j < CFG_TOTAL_BLOCKS; j++ ){

            // check current block, we don't want to mark a duplicate of itself
            if( j == i ){

                continue;
            }

            cfg_block_t block2;

            read_block( j, &block2 );

            // duplicate block found
            if( ( block2.id == block.id ) &&
                ( block2.block_number == block.block_number ) ){

                // erase duplicate
                erase_block( j );

                log_v_debug_P( PSTR("Cfg check found duplicate block ID:%lu at %u and %u"), 
                                 block.id, block.block_number, i, j );
            }
        }

        sapphire_type_t8 type = kv_i8_type( block.id );

        // check if parameter was not found in the KV system
        // (possibly a parameter was removed)
        // OR
        // the type listed in the parameter data does not match the
        // type listed in the KV system (parameter type may have been
        // changed)
        // OR 
        // block number exceeds number of blocks for this item
        if( ( type < 0 ) ||
            ( type != block.type ) ||
            ( block.block_number >= calc_n_blocks( type_u16_size( type ) ) ) ){

            log_v_debug_P( PSTR("Cfg check found bad block ID:%lu type:%d/%d #%d addr:%d"), 
                           block.id, type, block.type, block.block_number, i );

            erase_block( i );
        }
    }
}

uint16_t cfg_u16_total_blocks( void ){

    return CFG_TOTAL_BLOCKS;
}

uint16_t cfg_u16_free_blocks( void ){

    uint16_t blocks = 0;

    for( uint16_t i = 0; i < CFG_TOTAL_BLOCKS; i++ ){

        if( read_block_id( i ) == CFG_PARAM_EMPTY_BLOCK ){

            blocks++;
        }
    }

    return blocks;
}


static void erase_param( catbus_hash_t32 parameter ){

    for( uint16_t i = 0; i < CFG_TOTAL_BLOCKS; i++ ){

        if( read_block_id( i ) == parameter ){

            erase_block( i );
        }
    }
}

static void write_param( catbus_hash_t32 parameter, void *value ){

    // search for sys config
    sapphire_type_t8 type = kv_i8_type( parameter );

    // parameter not found
    if( type < 0 ){

        // bail out
        return;
    }

    uint16_t len = type_u16_size( type );

    // get number of blocks for this parameter
    uint8_t n_blocks = calc_n_blocks( len );

    // iterate through blocks
    for( uint8_t i = 0; i < n_blocks; i++ ){

        // set up new parameter block
        cfg_block_t param;
        param.id            = parameter;
        param.type          = type;
        param.block_number  = i;
        param.reserved      = 0xffff;

        memset( param.data, 0, sizeof(param.data) );

        uint16_t copy_len = len;

        if( copy_len > CFG_PARAM_DATA_LEN ){

            copy_len = CFG_PARAM_DATA_LEN;
        }

        memcpy( param.data, value, copy_len );

        // seek to old parameter
        int16_t old_block = seek_block( parameter, i );

        if( old_block >= 0 ){

            // read old block and compare
            cfg_block_t old_param;
            read_block( old_block, &old_param );

            if( memcmp( &old_param, &param, sizeof(old_param) ) == 0 ){

                // new data matches old data, we don't need to write anything for this
                // block.

                goto end;
            }
        }

        // get a free block
        int16_t free_block = get_free_block();

        // no blocks available, that's sad
        if( free_block < 0 ){

            sys_v_set_warnings( SYS_WARN_CONFIG_FULL );

            return;
        }

        // write new block
        write_block( free_block, &param );

        // read back block
        cfg_block_t check_param;
        read_block( free_block, &check_param );

        // compare
        if( memcmp( &check_param, &param, sizeof(check_param) ) != 0 ){

            // block write failed
            erase_block( free_block );

            sys_v_set_warnings( SYS_WARN_CONFIG_WRITE_FAIL );

            return;
        }

        // erase old block (if it exists)
        if( old_block >= 0 ){

            erase_block( old_block );
        }

end:
        value += copy_len;
        len -= copy_len;
    }
}

static int8_t read_param( catbus_hash_t32 parameter, void *value ){

    // seek to parameter
    int16_t block = seek_block( parameter, 0 );

    // get data len from kv system
    int16_t len = kv_i16_len( parameter );

    // parameter not found
    if( len < 0 ){

        return -1;
    }

    if( block < 0 ){

        // block not found, we're going to return all 0s for the data

        // set zeroes
        if( len > 0 ){

            memset( value, 0, len );
        }

        // parameter not found
        return -1;
    }

    // get number of blocks for this parameter
    uint8_t n_blocks = calc_n_blocks( len );

    // iterate through blocks
    for( uint8_t i = 0; i < n_blocks; i++ ){

        // skip seeking first block, we already have it
        if( i != 0 ){

            // seek next block
            block = seek_block( parameter, i );
        }

        cfg_block_t param;

        // check if we're missing a block
        if( block < 0 ){

            // load 0s for missing block
            memset( param.data, 0, CFG_PARAM_DATA_LEN );
        }
        else{

            read_block( block, &param );
        }

        uint16_t copy_len = len;

        if( copy_len > CFG_PARAM_DATA_LEN ){

            copy_len = CFG_PARAM_DATA_LEN;
        }

        // copy data to parameter
        memcpy( value, param.data, copy_len );

        value += copy_len;
        len -= copy_len;
    }

    return 0;
}

int8_t cfg_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    int16_t item_len = kv_i16_len( hash );

    // check for overflow
    if( item_len > (int16_t)len ){
        
        memset( data, 0, len );

        return 0;
    }

    if( op == KV_OP_GET ){

        // ignoring return value, the config system will return 0s as
        // default data in a parameter is not present
        cfg_i8_get( hash, data );
    }
    else if( op == KV_OP_SET ){

        cfg_v_set( hash, data );
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

void cfg_v_set( catbus_hash_t32 parameter, void *value ){

    // ASSERT( !osirq_b_is_irq() );

    ATOMIC;

    // IP config params are in-memory only
    if( parameter == CFG_PARAM_IP_ADDRESS ){

        memcpy( &ip_config.ip, value, sizeof(ip_config.ip) );
    }
    else if( parameter == CFG_PARAM_IP_SUBNET_MASK ){

        memcpy( &ip_config.subnet, value, sizeof(ip_config.subnet) );
    }
    else if( parameter == CFG_PARAM_DNS_SERVER ){

        memcpy( &ip_config.dns_server, value, sizeof(ip_config.dns_server) );
    }
    // else if( parameter == CFG_PARAM_INTERNET_GATEWAY ){

    //     memcpy( &ip_config.internet_gateway, value, sizeof(ip_config.internet_gateway) );
    // }
    else{

        write_param( parameter, value );
    }

    END_ATOMIC;
}

int8_t cfg_i8_get( catbus_hash_t32 parameter, void *value ){

    int8_t status = 0;

    ATOMIC;

    uint32_t start = tmr_u32_get_system_time_us();

    if( ( parameter == CFG_PARAM_IP_ADDRESS ) && ( value != 0 ) ){

        // read IP config from in memory state
        memcpy( value, &ip_config.ip, sizeof(ip_config.ip) );
    }
    else if( ( parameter == CFG_PARAM_IP_SUBNET_MASK ) && ( value != 0 ) ){

        memcpy( value, &ip_config.subnet, sizeof(ip_config.subnet) );
    }
    else if( ( parameter == CFG_PARAM_DNS_SERVER ) && ( value != 0 ) ){

        memcpy( value, &ip_config.dns_server, sizeof(ip_config.dns_server) );
    }
    else if( ( parameter == CFG_PARAM_BOARD_TYPE ) && ( value != 0 ) ){

        memcpy( value, &board_type, sizeof(board_type) );   
    }
    // else if( ( parameter == CFG_PARAM_INTERNET_GATEWAY ) && ( value != 0 ) ){

    //     memcpy( value, &ip_config.internet_gateway, sizeof(ip_config.internet_gateway) );
    // }
    else{

        status = read_param( parameter, value );

        // if( status < 0 ){

        //     cfg_v_set( parameter, value );
        // }
    }

    uint32_t elapsed = tmr_u32_elapsed_time_us( start );

    if( elapsed > slowest_time ){

        slowest_time = elapsed;
        slowest_id = parameter;
    }


    END_ATOMIC;

    return status;
}

void cfg_v_erase( catbus_hash_t32 parameter ){

    ATOMIC;

    erase_param( parameter );

    END_ATOMIC;
}

void cfg_v_set_security_key( uint8_t key_id, const uint8_t key[CFG_KEY_SIZE] ){

    // bounds check
    if( key_id >= CFG_MAX_KEYS ){

        return;
    }

    ATOMIC;

    ee_v_write_block( CFG_FILE_SECURE_START + ( key_id * CFG_KEY_SIZE ),
                      key,
                      CFG_KEY_SIZE );

    END_ATOMIC;
}

void cfg_v_get_security_key( uint8_t key_id, uint8_t key[CFG_KEY_SIZE] ){

    // bounds check
    if( key_id >= CFG_MAX_KEYS ){

        return;
    }

    ATOMIC;

    ee_v_read_block( CFG_FILE_SECURE_START + ( key_id * CFG_KEY_SIZE ),
                     key,
                     CFG_KEY_SIZE );

    END_ATOMIC;
}

void cfg_v_set_ip_config( cfg_ip_config_t *_ip_config ){

    ATOMIC;
    ip_config = *_ip_config;
    END_ATOMIC;
}

bool cfg_b_ip_configured( void ){

    bool configured = FALSE;

    ATOMIC;

    // check if IP is configured
    if( !ip_b_is_zeroes( ip_config.ip ) ){

        configured = TRUE;
    }

    END_ATOMIC;

    return configured;
}

bool cfg_b_manual_ip( void ){

    bool manual_ip = FALSE;

    ATOMIC;

    // read manual IP config
    cfg_ip_config_t manual_ip_cfg;

    cfg_i8_get( CFG_PARAM_MANUAL_IP_ADDRESS, &manual_ip_cfg.ip );
    cfg_i8_get( CFG_PARAM_MANUAL_SUBNET_MASK, &manual_ip_cfg.subnet );

    // check that we have IP and subnet set at the very least
    if( ( !ip_b_is_zeroes( manual_ip_cfg.ip ) ) &&
        ( !ip_b_addr_compare( manual_ip_cfg.ip, ip_a_addr(255,255,255,255) ) ) &&
        ( !ip_b_is_zeroes( manual_ip_cfg.subnet ) ) &&
        ( !ip_b_addr_compare( manual_ip_cfg.subnet, ip_a_addr(255,255,255,255) ) ) &&
        ( cfg_b_get_boolean( CFG_PARAM_ENABLE_MANUAL_IP ) ) ){

        manual_ip = TRUE;
    }

    END_ATOMIC;

    return manual_ip;
}

bool cfg_b_is_gateway( void ){

    return FALSE;
}

// this is a quick read function to get boolean parameters.
// it is just a convenience for code that doesn't want to create a local
// boolean variable
// NOTE:
// this function doesn't actually check that the parameter requested is actually a boolean.
// it will return whatever the boolean value of the first byte would be.
bool cfg_b_get_boolean( catbus_hash_t32 parameter ){

    // switch( parameter ){

    //     case CFG_PARAM_ENABLE_CPU_SLEEP:
    //         return enable_cpu_sleep;
    //         break;

    //     default:
    //         break;
    // }


    // get type length
    sapphire_type_t8 type = kv_i8_type( parameter );

    if( type != SAPPHIRE_TYPE_BOOL ){

        return FALSE;
    }

    bool value;

    // get parameter
    cfg_i8_get( parameter, &value );

    return value;
}

uint16_t cfg_u16_get_board_type( void ){

    return board_type;
}

void cfg_v_set_boolean( catbus_hash_t32 parameter, bool value ){

    cfg_v_set( parameter, &value );
}

void cfg_v_set_u16( catbus_hash_t32 parameter, uint16_t value ){

    cfg_v_set( parameter, &value );
}

void cfg_v_set_string( catbus_hash_t32 parameter, char *value ){

    cfg_v_set( parameter, value );
}

void cfg_v_set_string_P( catbus_hash_t32 parameter, PGM_P value ){

    char s[CFG_STR_LEN];
    memset( s, 0, sizeof(s) );
    strlcpy_P( s, value, sizeof(s) );

    cfg_v_set( parameter, s );
}

void cfg_v_set_ipv4( catbus_hash_t32 parameter, ip_addr_t value ){

    cfg_v_set( parameter, &value );
}

void cfg_v_set_mac64( catbus_hash_t32 parameter, uint8_t *value ){

    cfg_v_set( parameter, value );
}

void cfg_v_set_mac48( catbus_hash_t32 parameter, uint8_t *value ){

    cfg_v_set( parameter, value );
}

void cfg_v_set_key128( catbus_hash_t32 parameter, uint8_t *value ){

    cfg_v_set( parameter, value );
}

void cfg_v_reset_on_next_boot( void ){

    cfg_v_erase( CFG_PARAM_VERSION );
}

// write default values to all config items
void cfg_v_default_all( void ){

    // erase all the things!
    ee_v_erase_block( 0, EE_ARRAY_SIZE );

    uint8_t zeroes[CFG_STR_LEN];
    memset( zeroes, 0, sizeof(zeroes) );

    cfg_v_set_ipv4( CFG_PARAM_MANUAL_IP_ADDRESS, ip_a_addr(0,0,0,0) );
    cfg_v_set_ipv4( CFG_PARAM_MANUAL_SUBNET_MASK, ip_a_addr(0,0,0,0) );
    cfg_v_set_ipv4( CFG_PARAM_MANUAL_DNS_SERVER, ip_a_addr(0,0,0,0) );
    cfg_v_set_ipv4( CFG_PARAM_MANUAL_INTERNET_GATEWAY, ip_a_addr(0,0,0,0) );

    cfg_v_set_mac64( CFG_PARAM_DEVICE_ID, zeroes );

    cfg_v_set_u16( CFG_PARAM_CATBUS_DATA_PORT, CATBUS_DISCOVERY_PORT );
    cfg_v_set_u16( CFG_PARAM_MAX_LOG_SIZE, 65535 );

    cfg_v_set_u16( CFG_PARAM_VERSION, CFG_VERSION );
}

// init config module
void cfg_v_init( void ){

    COMPILER_ASSERT( CFG_TOTAL_BLOCKS < 256 );

    // run block clean algorithm
    clean_blocks();

    // create virtual files
    fs_f_create_virtual( PSTR("error_log.txt"), error_log_vfile_handler );
    fs_f_create_virtual( PSTR("fwinfo"), fw_info_vfile_handler );

    #ifdef ENABLE_CFG_VFILE
    fs_f_create_virtual( PSTR("cfg_eeprom"), eeprom_vfile_handler );
    #endif

    // check config version
    uint16_t version;
    if( cfg_i8_get( CFG_PARAM_VERSION, &version ) < 0 ){

        // parameter not present
        cfg_v_default_all();
    }
    // version mismatch
    else if( ( version != CFG_VERSION ) && ( version != ( CFG_VERSION - 1 ) ) ){

        cfg_v_default_all();
    }

    #ifdef CFG_INCLUDE_MANUAL_IP
    // check if manual IP config is set
    if( cfg_b_get_boolean( CFG_PARAM_ENABLE_MANUAL_IP ) ){

        // read manual IP config
        cfg_ip_config_t manual_ip_cfg;
        memset( &manual_ip_cfg, 0, sizeof(manual_ip_cfg) );

        cfg_i8_get( CFG_PARAM_MANUAL_IP_ADDRESS, &manual_ip_cfg.ip );
        cfg_i8_get( CFG_PARAM_MANUAL_SUBNET_MASK, &manual_ip_cfg.subnet );
        cfg_i8_get( CFG_PARAM_MANUAL_DNS_SERVER, &manual_ip_cfg.dns_server );
        cfg_i8_get( CFG_PARAM_MANUAL_INTERNET_GATEWAY, &manual_ip_cfg.internet_gateway );

        // check that we have IP and subnet set
        if( ( !ip_b_is_zeroes( manual_ip_cfg.ip ) ) &&
            ( !ip_b_addr_compare( manual_ip_cfg.ip, ip_a_addr(255,255,255,255) ) ) &&
            ( !ip_b_is_zeroes( manual_ip_cfg.subnet ) ) &&
            ( !ip_b_addr_compare( manual_ip_cfg.subnet, ip_a_addr(255,255,255,255) ) ) ){

            // manual config is set, apply to IP config
            ip_config = manual_ip_cfg;
        }
    }
    #endif

    // check loader status
    if( sys_u8_get_loader_status() == LDR_STATUS_NEW_FW ){

        // increment firmware load count

        uint16_t load_count;
        cfg_i8_get( CFG_PARAM_FIRMWARE_LOAD_COUNT, &load_count );

        load_count++;
        cfg_v_set( CFG_PARAM_FIRMWARE_LOAD_COUNT, &load_count );
    }

    // read mode string
    char mode_string[9];
    ee_v_read_block( 0, (uint8_t *)mode_string, sizeof(mode_string) - 1 );
    mode_string[8] = 0;

    // check mode
    if( strncmp_P( mode_string, PSTR("Sapphire"), sizeof(mode_string) ) != 0 ){

        // set mode string for bootloader
        strlcpy_P( mode_string, PSTR("Sapphire"), sizeof(mode_string) );
        ee_v_write_block( 0, (uint8_t *)mode_string, sizeof(mode_string) );
    }

    // cache oft used values
    // cfg_i8_get( CFG_PARAM_ENABLE_CPU_SLEEP, &enable_cpu_sleep );
    cfg_i8_get( CFG_PARAM_BOARD_TYPE, &board_type );

    // increment recovery counter
    #ifndef DEBUG // skip in debug mode
    uint8_t count;
    cfg_i8_get( CFG_PARAM_RECOVERY_MODE_BOOTS, &count );
    count++;
    cfg_v_set( CFG_PARAM_RECOVERY_MODE_BOOTS, &count );
    #endif

    log_v_debug_P( PSTR("Cfg size:%d free:%d eeprom:%d"), cfg_u16_total_blocks(), cfg_u16_free_blocks(), CFG_FILE_MAIN_SIZE );
}

void cfg_v_write_error_log( cfg_error_log_t *log ){

    ee_v_write_block( CFG_FILE_ERROR_LOG_START, (uint8_t *)log, sizeof(cfg_error_log_t) );
}

void cfg_v_read_error_log( cfg_error_log_t *log ){

    ee_v_read_block( CFG_FILE_ERROR_LOG_START, (uint8_t *)log, sizeof(cfg_error_log_t) );
}

void cfg_v_erase_error_log( void ){

    ee_v_write_byte_blocking( CFG_FILE_ERROR_LOG_START, 0xff );
}

uint16_t cfg_u16_error_log_size( void ){

    uint16_t count = 0;

    for( uint16_t i = CFG_FILE_ERROR_LOG_START;
         i < ( CFG_FILE_ERROR_LOG_START + CFG_FILE_SIZE_ERROR_LOG );
         i++ ){

        uint8_t byte = ee_u8_read_byte( i );

        if( ( byte == 0xff ) || ( byte == 0 ) ){

            break;
        }

        count++;
    }

    return count;
}
