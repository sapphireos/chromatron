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


/*

ESP8266 boot info:

rst cause:
0: undefined
1: power reboot
2: external reset or wake from deep sleep
4: hardware wdt reset


Timing notes:

At UART 4 Mhz, a 128 byte packet transfers in 320 uS.
128 bytes is the size of the ESP8266 hardware FIFO.

theoretical fastest speed for a 576 byte packet is 1.44 ms.

*/

#include "system.h"

#if defined(ENABLE_WIFI) && defined(ENABLE_WIFI_ESP8266_COPROC)

#include "wifi.h"
#include "esp8266.h"

#include "threading.h"
#include "timers.h"
#include "os_irq.h"
#include "keyvalue.h"
#include "hal_usart.h"
#include "wifi_cmd.h"
#include "netmsg.h"
#include "ip.h"
#include "list.h"
#include "config.h"
#include "crc.h"
#include "sockets.h"
#include "fs.h"
#include "watchdog.h"
#include "hal_status_led.h"
#include "hash.h"

#include "hal_esp8266.h"
#include "esp8266_loader.h"

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

#define WIFI_COMM_TRIES         4
#define WIFI_COMM_TIMEOUT       200000

static uint16_t ports[WIFI_MAX_PORTS];

static int8_t wifi_status;
static uint8_t wifi_mac[6];
static uint8_t wifi_status_reg;
static int8_t wifi_rssi;
static int8_t wifi_router;
static int8_t wifi_channel;
static uint32_t wifi_uptime;
static bool default_ap_mode;

static uint16_t wifi_comm_errors_esp;
static uint16_t wifi_comm_errors_mcu;
static uint8_t wifi_connects;

static uint16_t wifi_rx_udp_overruns;
static uint32_t wifi_udp_received;
static uint32_t wifi_udp_sent;
static uint16_t mem_heap_peak;
static uint16_t mem_used;
static uint16_t intf_max_time;
static uint16_t wifi_max_time;
static uint16_t mem_max_time;
static uint16_t intf_avg_time;
static uint16_t wifi_avg_time;
static uint16_t mem_avg_time;

static uint32_t comm_rx_rate;
static uint32_t comm_tx_rate;

static uint8_t wifi_version_major;
static uint8_t wifi_version_minor;
static uint8_t wifi_version_patch;

static uint32_t wifi_flash_id;

static uint8_t watchdog;

static uint8_t tx_power = WIFI_MAX_HW_TX_POWER;

#ifdef ARM
static bool connect_hold;
#endif

KV_SECTION_META kv_meta_t wifi_cfg_kv[] = {
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ssid" },
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_password" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid2" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password2" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid3" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password3" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid4" },
    { CATBUS_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password4" },
    { CATBUS_TYPE_BOOL,          0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_enable_ap" },    
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_ssid" },
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_password" },
    { CATBUS_TYPE_KEY128,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_md5" },
    { CATBUS_TYPE_UINT32,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_fw_len" },

    { CATBUS_TYPE_UINT8,         0, KV_FLAGS_PERSIST,           &tx_power,          0,                   "wifi_tx_power" },
};

KV_SECTION_META kv_meta_t wifi_info_kv[] = {
    { CATBUS_TYPE_INT8,          0, 0, &wifi_status,                      0,   "wifi_status" },
    { CATBUS_TYPE_MAC48,         0, 0, &wifi_mac,                         0,   "wifi_mac" },
    { CATBUS_TYPE_INT8,          0, 0, &wifi_rssi,                        0,   "wifi_rssi" },
    { CATBUS_TYPE_INT8,          0, 0, &wifi_channel,                     0,   "wifi_channel" },
    { CATBUS_TYPE_UINT32,        0, 0, &wifi_uptime,                      0,   "wifi_uptime" },
    { CATBUS_TYPE_UINT8,         0, 0, &wifi_connects,                    0,   "wifi_connects" },

    { CATBUS_TYPE_UINT8,         0, 0, &wifi_version_major,               0,   "wifi_version_major" },
    { CATBUS_TYPE_UINT8,         0, 0, &wifi_version_minor,               0,   "wifi_version_minor" },
    { CATBUS_TYPE_UINT8,         0, 0, &wifi_version_patch,               0,   "wifi_version_patch" },

    { CATBUS_TYPE_UINT16,        0, 0, &wifi_comm_errors_esp,             0,   "wifi_comm_errors_esp" },
    { CATBUS_TYPE_UINT16,        0, 0, &wifi_comm_errors_mcu,             0,   "wifi_comm_errors_mcu" },
    { CATBUS_TYPE_UINT16,        0, 0, &wifi_rx_udp_overruns,             0,   "wifi_comm_rx_udp_overruns" },
    { CATBUS_TYPE_UINT32,        0, 0, &wifi_udp_received,                0,   "wifi_udp_received" },
    { CATBUS_TYPE_UINT32,        0, 0, &wifi_udp_sent,                    0,   "wifi_udp_sent" },

    { CATBUS_TYPE_UINT16,        0, 0, &mem_heap_peak,                    0,   "wifi_mem_heap_peak" },
    { CATBUS_TYPE_UINT16,        0, 0, &mem_used,                         0,   "wifi_mem_used" },

    { CATBUS_TYPE_UINT16,        0, 0, &intf_max_time,                    0,   "wifi_proc_intf_max_time" },
    { CATBUS_TYPE_UINT16,        0, 0, &wifi_max_time,                    0,   "wifi_proc_wifi_max_time" },
    { CATBUS_TYPE_UINT16,        0, 0, &mem_max_time,                     0,   "wifi_proc_mem_max_time" },

    { CATBUS_TYPE_UINT16,        0, 0, &intf_avg_time,                    0,   "wifi_proc_intf_avg_time" },
    { CATBUS_TYPE_UINT16,        0, 0, &wifi_avg_time,                    0,   "wifi_proc_wifi_avg_time" },
    { CATBUS_TYPE_UINT16,        0, 0, &mem_avg_time,                     0,   "wifi_proc_mem_avg_time" },

    { CATBUS_TYPE_UINT32,        0, 0, &comm_tx_rate,                     0,   "wifi_comm_rate_tx" },
    { CATBUS_TYPE_UINT32,        0, 0, &comm_rx_rate,                     0,   "wifi_comm_rate_rx" },

    { CATBUS_TYPE_UINT32,        0, 0, &wifi_flash_id,                    0,   "wifi_flash_id" },
};

void debug_strobe( void ){
    // io_v_digital_write( IO_PIN_1_XCK, TRUE );
    // _delay_us( 10 );
    // io_v_digital_write( IO_PIN_1_XCK, FALSE );
}

static int8_t _wifi_i8_send_header( uint8_t data_id, uint16_t data_len ){

    uint8_t tries = WIFI_COMM_TRIES;

    wifi_data_header_t header;
    header.data_id  = data_id;
    header.len      = data_len;
    header.flags    = 0;
    header.reserved = 0;
    
    header.header_crc  = HTONS( crc_u16_block( (uint8_t *)&header, sizeof(header) - sizeof(header.header_crc) ) );

    while( tries > 0 ){

        tries--;

        hal_wifi_v_usart_send_char( WIFI_COMM_DATA );

        // transmit header
        hal_wifi_v_usart_send_data( (uint8_t *)&header, sizeof(header) );

        // if data length is more than the single shot limit,
        // wait for ACK/NAK
        if( data_len > WIFI_MAX_SINGLE_SHOT_LEN ){

            int16_t byte = hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT );

            if( byte == WIFI_COMM_ACK ){

                return 0;
            }
            else if( byte == WIFI_COMM_NAK ){

                debug_strobe();

                log_v_debug_P( PSTR("msg NAK: 0x%02x"), data_id );
            }
            else if( byte < 0 ){

                debug_strobe();

                log_v_debug_P( PSTR("msg response timeout 0x%02x"), data_id );
            }
            else{

                debug_strobe();

                log_v_debug_P( PSTR("msg invalid response: 0x%02x -> 0x%02x"), data_id, byte );
            }
        }
        else{

            // no need to wait for ack, so we're good
            return 0;
        }
    }

    log_v_debug_P( PSTR("header failed") );

    return -1;
}


int8_t wifi_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( !wifi_b_attached() ){

        return -1;
    }

    uint8_t tries = WIFI_COMM_TRIES;

    while( tries > 0 ){

        tries--;

        hal_wifi_v_usart_flush();

        // bail out of header fails, it is already tried multiple times
        if( _wifi_i8_send_header( data_id, len ) < 0 ){

            return -2;
        }

        if( len > 0 ){
        
            uint16_t crc = crc_u16_start();

            while( len > 0 ){

                hal_wifi_v_usart_send_char( *data );
                crc = crc_u16_byte( crc, *data );

                data++;
                len--;
            }

            crc = crc_u16_finish( crc );
            hal_wifi_v_usart_send_data( (uint8_t *)&crc, sizeof(crc) );
        }

        int16_t byte = hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT );

        if( byte == WIFI_COMM_ACK ){

            return 0;
        }
        else if( byte == WIFI_COMM_NAK ){

            debug_strobe();

            log_v_debug_P( PSTR("msg NAK: 0x%02x"), data_id );
        }
        else if( byte < 0 ){

            debug_strobe();

            // log_v_debug_P( PSTR("msg response timeout 0x%02x"), data_id );
        }
        else{

            debug_strobe();

            log_v_debug_P( PSTR("msg invalid response: 0x%02x -> 0x%02x"), data_id, byte );
        }
    }

    debug_strobe();

    log_v_debug_P( PSTR("msg failed: 0x%02x"), data_id );

    return -3;
}

int8_t _wifi_i8_internal_receive( wifi_data_header_t *header, uint8_t *data, uint16_t max_len, uint16_t *bytes_read ){

    if( bytes_read != 0 ){
        
        *bytes_read = 0;
    }   

    uint8_t tries = WIFI_COMM_TRIES;
    int8_t status = -1;
    

    while( tries > 0 ){

        tries--;

        // wait for RTS
        if( hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT ) != WIFI_COMM_RTS ){

            debug_strobe();

            log_v_debug_P( PSTR("rts timeout") );
            continue;
        }

        // RTS asserted
        // set CTS
        hal_wifi_v_usart_send_char( WIFI_COMM_CTS );

        // wait for data start
        if( hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT ) != WIFI_COMM_DATA ){
            debug_strobe();
            log_v_debug_P( PSTR("data start timeout") );
            continue;
        }

        if( hal_wifi_i8_usart_receive( (uint8_t *)header, sizeof(wifi_data_header_t), WIFI_COMM_TIMEOUT ) < 0 ){
            debug_strobe();
            log_v_debug_P( PSTR("header timeout") );
            continue;
        }

        if( header->len > max_len ){
            debug_strobe();
            log_v_debug_P( PSTR("invalid len 0x%02x"), header->data_id );
            continue;
        }

        uint16_t crc;
        
        if( header->len > 0 ){

            ASSERT( data != 0 );

            if( hal_wifi_i8_usart_receive( data, header->len, WIFI_COMM_TIMEOUT ) < 0 ){
                debug_strobe();
                log_v_debug_P( PSTR("data timeout 0x%02x %d"), header->data_id, header->len );
                continue;
            }
        
            if( hal_wifi_i8_usart_receive( (uint8_t *)&crc, sizeof(crc), WIFI_COMM_TIMEOUT ) < 0 ){
                debug_strobe();
                log_v_debug_P( PSTR("crc timeout 0x%02x"), header->data_id );
                continue;
            }
        }

        // check CRCs
        header->header_crc = HTONS( header->header_crc );
        if( crc_u16_block( (uint8_t *)header, sizeof(wifi_data_header_t) ) != 0 ){
            debug_strobe();
            log_v_debug_P( PSTR("header crc fail 0x%02x"), header->data_id );
            hal_wifi_v_usart_send_char( WIFI_COMM_NAK );
            continue;
        }

        if( header->len > 0 ){
        
            if( crc_u16_block( data, header->len ) != crc ){

                debug_strobe();
                log_v_debug_P( PSTR("data crc fail 0x%02x"), header->data_id );
                hal_wifi_v_usart_send_char( WIFI_COMM_NAK );
                continue;
            }
        }

        // everything is good!
        hal_wifi_v_usart_send_char( WIFI_COMM_ACK );

        // if bytes read is set, return data length
        if( bytes_read != 0 ){

            *bytes_read = header->len;
        }
        else{

            // bytes read is null, received data is expected to be fixed size
            if( header->len != max_len ){
                debug_strobe();
                log_v_debug_P( PSTR("wrong data len: 0x%02x %d != %d"), header->data_id, header->len, max_len );
                status = -5;
                goto error;
            }
        }

        // message received ok - reset watchdog
        watchdog = WIFI_WATCHDOG_TIMEOUT;

        return 0;
    }


error:
    hal_wifi_v_usart_send_char( WIFI_COMM_NAK );
    log_v_debug_P( PSTR("rx fail") );

    return status;
}

int8_t wifi_i8_receive_msg( uint8_t data_id, uint8_t *data, uint16_t max_len, uint16_t *bytes_read ){

    wifi_data_header_t header;

    int8_t status = _wifi_i8_internal_receive( &header, data, max_len, bytes_read );

    if( status < 0 ){

        log_v_debug_P( PSTR("receive fail: 0x%02x"), data_id );

        return status;
    }

    // header CRC good, check for data ID
    if( ( data_id != 0 ) && ( header.data_id != data_id ) ){

        log_v_debug_P( PSTR("wrong data id") );
        return -4;
    }

    return 0;
}


void send_ports( void ){

    wifi_i8_send_msg( WIFI_DATA_ID_PORTS, (uint8_t *)&ports, sizeof(ports) );
}


void open_close_port( uint8_t protocol, uint16_t port, bool open ){

    if( protocol == IP_PROTO_UDP ){

        if( open ){

            // search for duplicate
            for( uint8_t i = 0; i < cnt_of_array(ports); i++ ){

                if( ports[i] == port ){

                    return;
                }
            }

            // search for empty slot
            for( uint8_t i = 0; i < cnt_of_array(ports); i++ ){

                if( ports[i] == 0 ){

                    ports[i] = port;

                    break;
                }
            }
        }
        else{

            // search for port
            for( uint8_t i = 0; i < cnt_of_array(ports); i++ ){

                if( ports[i] == port ){

                    ports[i] = 0;

                    break;
                }
            }
        }
    }

    // send ports message
    send_ports();
}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){

    int8_t status = 0;

    if( !wifi_b_connected() ){

        return NETMSG_TX_ERR_RELEASE;
    }

    netmsg_state_t *netmsg_state = netmsg_vp_get_state( netmsg );

    ASSERT( netmsg_state->type == NETMSG_TYPE_UDP );

    uint16_t data_len = 0;

    uint8_t *data = 0;

    if( netmsg_state->data_handle > 0 ){

        data = mem2_vp_get_ptr( netmsg_state->data_handle );
        data_len = mem2_u16_get_size( netmsg_state->data_handle );
    }

    // setup header
    wifi_msg_udp_header_t udp_header;
    udp_header.addr = netmsg_state->raddr.ipaddr;
    udp_header.lport = netmsg_state->laddr.port;
    udp_header.rport = netmsg_state->raddr.port;
    udp_header.len = data_len;
    
    uint8_t tries = WIFI_COMM_TRIES;

    while( tries > 0 ){

        tries--;

        // bail out of header fails, it is already tried multiple times
        if( _wifi_i8_send_header( WIFI_DATA_ID_SEND_UDP, sizeof(wifi_msg_udp_header_t) + data_len ) < 0 ){

            break;
        }

        uint16_t crc = crc_u16_start();

        uint16_t len = sizeof(wifi_msg_udp_header_t);
        uint8_t *send_data = (uint8_t *)&udp_header;
        while( len > 0 ){

            hal_wifi_v_usart_send_char( *send_data );
            crc = crc_u16_byte( crc, *send_data );

            send_data++;
            len--;
        }

        len = data_len;
        send_data = data;
        while( len > 0 ){

            hal_wifi_v_usart_send_char( *send_data );
            crc = crc_u16_byte( crc, *send_data );

            send_data++;
            len--;
        }

        crc = crc_u16_finish( crc );
        hal_wifi_v_usart_send_data( (uint8_t *)&crc, sizeof(crc) );

        int16_t byte = hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT );

        if( byte == WIFI_COMM_ACK ){

            break;
        }
        else if( byte < 0 ){

            log_v_debug_P( PSTR("msg response timeout") );       
        }
        else{

            log_v_debug_P( PSTR("udp invalid response -> 0x%02x"), byte );
        }
    }

    if( status < 0 ){
     
        log_v_debug_P( PSTR("msg failed") );

        return NETMSG_TX_ERR_RELEASE;   
    }
    
    return NETMSG_TX_OK_RELEASE;
}

static bool _wifi_b_ap_mode_enabled( void ){

    if( default_ap_mode ){

        return TRUE;
    }

    bool wifi_enable_ap = FALSE;
    cfg_i8_get( CFG_PARAM_WIFI_ENABLE_AP, &wifi_enable_ap );
    
    return wifi_enable_ap;    
}


PT_THREAD( wifi_init_thread( pt_t *pt, void *state ) );

static void get_info( void ){

    if( wifi_i8_send_msg( WIFI_DATA_ID_INFO, 0, 0 ) < 0 ){

        return;
    }

    wifi_msg_info_t msg;
    if( wifi_i8_receive_msg( WIFI_DATA_ID_INFO, (uint8_t *)&msg, sizeof(msg), 0 ) < 0 ){

        return;
    }

    // process message data

    wifi_version_major          = msg.version_major;
    wifi_version_minor          = msg.version_minor;    
    wifi_version_patch          = msg.version_patch;
    
    if( wifi_b_connected() ){
        
        wifi_rssi               = msg.rssi;
    }
    else{

        wifi_rssi               =  -127;
    }
    
    memcpy( wifi_mac, msg.mac, sizeof(wifi_mac) );

    uint64_t current_device_id = 0;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &current_device_id );
    uint64_t device_id = 0;
    memcpy( &device_id, wifi_mac, sizeof(wifi_mac) );

    if( current_device_id != device_id ){

        cfg_v_set( CFG_PARAM_DEVICE_ID, &device_id );
    }

    cfg_v_set( CFG_PARAM_IP_ADDRESS, &msg.ip );
    cfg_v_set( CFG_PARAM_IP_SUBNET_MASK, &msg.subnet );
    cfg_v_set( CFG_PARAM_DNS_SERVER, &msg.dns );
    cfg_v_set( CFG_PARAM_INTERNET_GATEWAY, &msg.gateway );

    wifi_rx_udp_overruns        = msg.rx_udp_overruns;
    wifi_udp_received           = msg.udp_received;
    wifi_udp_sent               = msg.udp_sent;
    wifi_comm_errors_esp        = msg.comm_errors;
    mem_heap_peak               = msg.mem_heap_peak;
    mem_used                    = msg.mem_used;

    intf_max_time               = msg.intf_max_time;
    wifi_max_time               = msg.wifi_max_time;
    mem_max_time                = msg.mem_max_time;

    intf_avg_time               = msg.intf_avg_time;
    wifi_avg_time               = msg.wifi_avg_time;
    mem_avg_time                = msg.mem_avg_time;

    wifi_router                 = msg.wifi_router;
    wifi_channel                = msg.wifi_channel;

    wifi_flash_id               = msg.flash_id;
}

PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    THREAD_WAIT_WHILE( pt, wifi_status != WIFI_STATE_ALIVE );
    static uint8_t scan_timeout;
    
    // check if we are connected
    while( !wifi_b_connected() ){

        if( wifi_status != WIFI_STATE_ALIVE ){

            THREAD_RESTART( pt );
        }

        wifi_rssi = -127;
        
        THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

        bool ap_mode = _wifi_b_ap_mode_enabled();

        // get SSIDs from database
        wifi_msg_connect_t msg;
        memset( &msg, 0, sizeof(msg) );

        cfg_i8_get( CFG_PARAM_WIFI_SSID, msg.ssid[0] );
        cfg_i8_get( CFG_PARAM_WIFI_PASSWORD, msg.pass[0] );
        
        kv_i8_get( __KV__wifi_ssid2, msg.ssid[1], sizeof(msg.ssid[1]) );
        kv_i8_get( __KV__wifi_password2, msg.pass[1], sizeof(msg.pass[1]) );  

        kv_i8_get( __KV__wifi_ssid3, msg.ssid[2], sizeof(msg.ssid[2]) );
        kv_i8_get( __KV__wifi_password3, msg.pass[2], sizeof(msg.pass[2]) );  

        kv_i8_get( __KV__wifi_ssid4, msg.ssid[3], sizeof(msg.ssid[3]) );
        kv_i8_get( __KV__wifi_password4, msg.pass[3], sizeof(msg.pass[3]) );  

        // check if no APs are configured
        if( ( msg.ssid[0][0] == 0 ) &&
            ( msg.ssid[1][0] == 0 ) &&
            ( msg.ssid[2][0] == 0 ) &&
            ( msg.ssid[3][0] == 0 ) ){

            // no SSIDs are configured

            // switch to AP mode
            ap_mode = TRUE;
        }
 
        // station mode
        if( !ap_mode ){
station_mode:            
            // scan first
            wifi_router = -1;
            log_v_debug_P( PSTR("Scanning...") );
            wifi_i8_send_msg( WIFI_DATA_ID_SCAN, (uint8_t *)&msg, sizeof(msg) );

            scan_timeout = 200;
            while( ( wifi_router < 0 ) && ( scan_timeout > 0 ) ){

                scan_timeout--;

                TMR_WAIT( pt, 50 );

                get_info();
            }

            if( wifi_router < 0 ){

                goto end;
            }

            log_v_debug_P( PSTR("Connecting...") );
            
            #ifdef ARM
            connect_hold = TRUE;
            #endif

            wifi_i8_send_msg( WIFI_DATA_ID_CONNECT, 0, 0 );

            #ifdef ARM
            // since ARM targets don't use the ESP for anything other than wifi, we don't
            // need to spin lock the entire CPU.
            TMR_WAIT( pt, 1000 );
            #else

            // the wifi module will hang for around 900 ms when doing a connect.
            // since we can't response to messages while that happens, we're going to do a blocking wait
            // for 1 second here.
            #ifdef ENABLE_USB
            usb_v_detach();
            #endif

            for( uint8_t i = 0; i < 10; i++ ){

                _delay_ms( 100 );
                sys_v_wdt_reset();
            }

            #ifdef ENABLE_USB
            usb_v_attach();
            #endif
            #endif

            #ifdef ARM
            connect_hold = FALSE;
            #endif

            get_info();

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                   ( thread_b_alarm_set() ) );
        }
        // AP mode
        else{
            // should have gotten the MAC by now
            ASSERT( wifi_mac[0] != 0 );

            // AP mode
            wifi_msg_ap_connect_t ap_msg;

            cfg_i8_get( CFG_PARAM_WIFI_AP_SSID, ap_msg.ssid );
            cfg_i8_get( CFG_PARAM_WIFI_AP_PASSWORD, ap_msg.pass );

            // check if AP mode SSID is set:
            if( ( ap_msg.ssid[0] == 0 ) || ( default_ap_mode ) ){

                // set up default AP
                memset( ap_msg.ssid, 0, sizeof(ap_msg.ssid) );
                memset( ap_msg.pass, 0, sizeof(ap_msg.pass) );

                strlcpy_P( ap_msg.ssid, PSTR("Chromatron_"), sizeof(ap_msg.ssid) );

                char mac[16];
                memset( mac, 0, sizeof(mac) );
                snprintf_P( &mac[0], 3, PSTR("%02x"), wifi_mac[3] );
                snprintf_P( &mac[2], 3, PSTR("%02x"), wifi_mac[4] ); 
                snprintf_P( &mac[4], 3, PSTR("%02x"), wifi_mac[5] );

                strncat( ap_msg.ssid, mac, sizeof(ap_msg.ssid) );

                strlcpy_P( ap_msg.pass, PSTR("12345678"), sizeof(ap_msg.pass) );

                default_ap_mode = TRUE;
            }
            else if( strnlen( ap_msg.pass, sizeof(ap_msg.pass) ) < WIFI_AP_MIN_PASS_LEN ){

                log_v_warn_P( PSTR("AP mode password must be at least 8 characters.") );

                // disable ap mode
                bool temp = FALSE;
                cfg_v_set( CFG_PARAM_WIFI_ENABLE_AP, &temp );

                goto end;
            }

            log_v_debug_P( PSTR("Starting AP: %s"), ap_msg.ssid );

            // check if wifi settings were present
            if( ap_msg.ssid[0] != 0 ){        

                wifi_i8_send_msg( WIFI_DATA_ID_AP_MODE, (uint8_t *)&ap_msg, sizeof(ap_msg) );

                thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
                THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                       ( thread_b_alarm_set() ) );

                if( !wifi_b_connected() ){

                    log_v_warn_P( PSTR("AP mode failed.") );

                    goto station_mode;
                }
            }
        }

end:
        if( !wifi_b_connected() ){

            TMR_WAIT( pt, 500 );
        }
    }

    if( wifi_b_connected() ){

        if( !_wifi_b_ap_mode_enabled() ){

            wifi_connects++;

            char ssid[WIFI_SSID_LEN];
            wifi_v_get_ssid( ssid );
            log_v_debug_P( PSTR("Wifi connected to: %s"), ssid );
        }
        else{

            log_v_debug_P( PSTR("Wifi soft AP up") );    
        }
    }

    THREAD_WAIT_WHILE( pt, wifi_b_connected() );
    
    log_v_debug_P( PSTR("Wifi disconnected") );

    THREAD_RESTART( pt );

PT_END( pt );
}


//static 
int8_t process_rx_data( wifi_data_header_t *header, uint8_t *data ){

    int8_t status = 0;

    if( header->data_id == WIFI_DATA_ID_STATUS ){

        if( header->len != sizeof(wifi_msg_status_t) ){

            goto len_error;
        }

        wifi_msg_status_t *msg = (wifi_msg_status_t *)data;

        wifi_status_reg = msg->flags;
    }  
//     else if( header->data_id == WIFI_DATA_ID_WIFI_SCAN_RESULTS ){
    
//         if( wifi_networks_handle < 0 ){        

//             wifi_networks_handle = mem2_h_alloc( sizeof(wifi_msg_scan_results_t) );

//             if( wifi_networks_handle > 0 ){

//                 memcpy( mem2_vp_get_ptr( wifi_networks_handle ), data, sizeof(wifi_msg_scan_results_t) );
//             }
//         }

//         // wifi_msg_scan_results_t *msg = (wifi_msg_scan_results_t *)data;

//         // for( uint8_t i = 0; i < msg->count; i++ ){

//         //     log_v_debug_P(PSTR("%ld %lu"), msg->networks[i].rssi, msg->networks[i].ssid_hash );
//         // }
//     }
    else if( header->data_id == WIFI_DATA_ID_DEBUG_PRINT ){

        // ensure null termination
        data[header->len - 1] = 0;

        log_v_debug_P( PSTR("ESP: %s"), data );
    }
    // check if msg handler is installed
    else if( wifi_i8_msg_handler ){

        wifi_i8_msg_handler( header->data_id, data, header->len );
    }

    goto end;

len_error:

    wifi_comm_errors_mcu++;

    log_v_debug_P( PSTR("Wifi len error: %d"), header->data_id );
    status = -3;

end:
    return status;
}


static void send_options_msg( void ){

    // send options message
    wifi_msg_set_options_t options_msg;
    memset( &options_msg, 0, sizeof(options_msg) );
    options_msg.high_speed = TRUE;
    options_msg.led_quiet = cfg_b_get_boolean( CFG_PARAM_ENABLE_LED_QUIET_MODE );
    options_msg.low_power = cfg_b_get_boolean( CFG_PARAM_ENABLE_LOW_POWER_MODE );

    if( tx_power > WIFI_MAX_HW_TX_POWER ){

        tx_power = WIFI_MAX_HW_TX_POWER;
    }

    #ifdef WIFI_MAX_SW_TX_POWER
    if( tx_power > WIFI_MAX_SW_TX_POWER ){

        tx_power = WIFI_MAX_SW_TX_POWER;
    }
    #endif

    /*
    Power:
    dBm mW
    10  10
    11  12.6
    12  15.8
    13  20
    14  25.1
    15  31.6  
    16  39.8
    17  50.1
    18  63.1
    19  79.4
    20  100
    * note, we can't set to 20.5 with this API, but the ESP8266 can technically allow it.
    20.5 112.2
    */


    options_msg.tx_power = tx_power; // in dbm

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // disable mdns in safe mode
        options_msg.mdns_enable = FALSE;    
    }
    else{

        options_msg.mdns_enable = TRUE;       
    }

    wifi_i8_send_msg( WIFI_DATA_ID_SET_OPTIONS, (uint8_t *)&options_msg, sizeof(options_msg) );
}




PT_THREAD( wifi_comm_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
        
    hal_wifi_v_enter_normal_mode();

    // delay while wifi boots up
    TMR_WAIT( pt, 300 );

    wifi_status_reg = 0;

    watchdog = WIFI_WATCHDOG_TIMEOUT;

    while( watchdog > 0 ){

        watchdog--;

        hal_wifi_v_usart_flush();

        // poll for connection
        hal_wifi_v_usart_send_char( WIFI_COMM_QUERY_READY );

        // wait for connection from wifi module
        if( hal_wifi_i16_usart_get_char_timeout( 100000 ) == WIFI_COMM_READY ){

            break;
        }
    }

    if( watchdog == 0 ){

        log_v_debug_P( PSTR("wifi failed to boot") );

        THREAD_EXIT( pt );
    }

    watchdog = WIFI_WATCHDOG_TIMEOUT;
    wifi_status = WIFI_STATE_ALIVE;

    get_info();
    send_options_msg();
    send_ports();

    if( tx_power < WIFI_MAX_HW_TX_POWER ){

        log_v_debug_P( PSTR("TX power set to: %d dbm!"), tx_power );
    }

    while(1){

        THREAD_YIELD( pt );
        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_read_irq() );

        hal_wifi_v_usart_flush();

        // retrieve message
        hal_wifi_v_usart_send_char( WIFI_COMM_GET_MSG );

        int16_t byte = hal_wifi_i16_usart_get_char_timeout( WIFI_COMM_TIMEOUT );
        if( byte != WIFI_COMM_ACK ){

            debug_strobe();

            if( byte < 0 ){

                log_v_debug_P( PSTR("WIFI_COMM_GET_MSG timeout") );                
            }
            else{

                log_v_debug_P( PSTR("Invalid control byte: %x"), byte);
            }
            
            continue;
        }

        uint8_t buf[WIFI_MAX_MCU_BUF];
        wifi_data_header_t header;
        uint16_t bytes_read;

        if( _wifi_i8_internal_receive( &header, buf, sizeof(buf), &bytes_read ) < 0 ){

            // message failed
            continue;
        }

        // process message
        process_rx_data( &header, buf );
    
        if( wifi_status_reg & WIFI_STATUS_NET_RX ){

            THREAD_WAIT_WHILE( pt, sock_b_rx_pending() );

            if( wifi_i8_send_msg( WIFI_DATA_ID_PEEK_UDP, 0, 0 ) < 0 ){

                log_v_debug_P( PSTR("peek fail") );     
                continue;
            }   

            wifi_msg_udp_header_t udp_header;

            if( wifi_i8_receive_msg( WIFI_DATA_ID_PEEK_UDP, (uint8_t *)&udp_header, sizeof(udp_header), 0 ) < 0 ){

                log_v_debug_P( PSTR("peek fail 2") );
                continue;
            }

            // allocate netmsg
            netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

            if( rx_netmsg < 0 ){

                log_v_debug_P( PSTR("rx udp alloc fail") );     

                continue;
            }

            netmsg_state_t *state = netmsg_vp_get_state( rx_netmsg );

            // set up addressing info
            state->laddr.port   = udp_header.lport;
            state->raddr.port   = udp_header.rport;
            state->raddr.ipaddr = udp_header.addr;

            // allocate data buffer
            state->data_handle = mem2_h_alloc2( udp_header.len, MEM_TYPE_SOCKET_BUFFER );

            if( state->data_handle < 0 ){

                log_v_debug_P( PSTR("rx udp no handle") );     

                netmsg_v_release( rx_netmsg );

                continue;
            }      

            if( wifi_i8_send_msg( WIFI_DATA_ID_GET_UDP, 0, 0 ) < 0 ){

                netmsg_v_release( rx_netmsg );

                log_v_debug_P( PSTR("get udp fail") );     

                continue;
            }

            // we can get a fast ptr because we've already verified the handle
            uint8_t *data = mem2_vp_get_ptr_fast( state->data_handle );

            if( wifi_i8_receive_msg( WIFI_DATA_ID_GET_UDP, data, udp_header.len, 0 ) < 0 ){

                netmsg_v_release( rx_netmsg );

                continue;
            }

            // receive message
            netmsg_v_receive( rx_netmsg );

            wifi_status_reg &= ~WIFI_STATUS_NET_RX;
        }
    }

PT_END( pt );
}


PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

        #ifdef ARM
        THREAD_WAIT_WHILE( pt, connect_hold );
        #endif

        if( watchdog > 0 ){

            watchdog--;
        }

        if( watchdog == 0 ){

            if( sys_u8_get_mode() != SYS_MODE_SAFE ){

                log_v_debug_P( PSTR("Wifi watchdog triggered") );

                // reboot to safe mode
                sys_v_reboot_delay( SYS_MODE_SAFE );
                
                // delay on this thread until reboot
                TMR_WAIT( pt, 100000 );

                ASSERT( FALSE );
            }
        }

        if( wifi_b_connected() ){

            wifi_uptime += 1;
        }
        else{

            wifi_uptime = 0;
        }

        get_info();


        // update comm traffic counters
        comm_rx_rate = hal_wifi_u32_get_rx_bytes();
        comm_tx_rate = hal_wifi_u32_get_tx_bytes();

        send_options_msg();
    }

PT_END( pt );
}



PT_THREAD( wifi_init_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    wifi_status_reg = 0;
    wifi_status = WIFI_STATE_BOOT;

    wifi_v_start_loader();

    THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

    if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

        THREAD_EXIT( pt );
    }


    trace_printf( "Starting wifi!\r\n" );
    // log_v_debug_P( PSTR("Starting wifi!") );

    thread_t_create_critical( 
                     wifi_comm_thread,
                     PSTR("wifi_comm"),
                     0,
                     0 );

    thread_t_create_critical( 
                 wifi_connection_manager_thread,
                 PSTR("wifi_connection_manager"),
                 0,
                 0 );


PT_END( pt );
}

PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static socket_t sock;

    sock = sock_s_create( SOS_SOCK_DGRAM );
    sock_v_bind( sock, 7 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        EVENT( EVENT_ID_DEBUG_0, 2 );

        if( sock_i16_sendto( sock, sock_vp_get_data( sock ), sock_i16_get_bytes_read( sock ), 0 ) >= 0 ){
            
        }
    }

PT_END( pt );
}


int8_t get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    // check if interface is up
    if( !wifi_b_connected() ){

        return NETMSG_ROUTE_NOT_AVAILABLE;
    }

    cfg_i8_get( CFG_PARAM_IP_ADDRESS, subnet );
    cfg_i8_get( CFG_PARAM_IP_SUBNET_MASK, subnet_mask );

    // set this route as default
    return NETMSG_ROUTE_DEFAULT;
}

ROUTING_TABLE routing_table_entry_t route_wifi = {
    get_route,
    wifi_i8_send_udp,
    open_close_port,
    ROUTE_FLAGS_ALLOW_GLOBAL_BROADCAST
};


void wifi_v_init( void ){
    // return;
// io_v_set_mode( IO_PIN_1_XCK, IO_MODE_OUTPUT );
    hal_wifi_v_init();

    wifi_status = WIFI_STATE_BOOT;


    thread_t_create_critical( THREAD_CAST(wifi_init_thread),
                              PSTR("wifi_init"),
                              0,
                              0 );

    thread_t_create_critical( wifi_status_thread,
                              PSTR("wifi_status"),
                              0,
                              0 );

    thread_t_create_critical( wifi_echo_thread,
                              PSTR("wifi_echo"),
                              0, 
                              0 );
}

void wifi_v_shutdown( void ){

    wifi_i8_send_msg( WIFI_DATA_ID_SHUTDOWN, 0, 0 );
    wifi_status = WIFI_STATE_SHUTDOWN;
}

bool wifi_b_connected( void ){

    return ( wifi_status_reg & WIFI_STATUS_CONNECTED ) != 0;
}

int8_t wifi_i8_rssi( void ){

    return wifi_rssi;
}

void wifi_v_get_ssid( char ssid[WIFI_SSID_LEN] ){

    if( wifi_router == 0 ){

        cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );
    }
    else if( wifi_router == 1 ){

        kv_i8_get( __KV__wifi_ssid2, ssid, WIFI_SSID_LEN );
    }
    else if( wifi_router == 2 ){

        kv_i8_get( __KV__wifi_ssid3, ssid, WIFI_SSID_LEN ); 
    }
    else if( wifi_router == 3 ){

        kv_i8_get( __KV__wifi_ssid4, ssid, WIFI_SSID_LEN );
    }
    else{

        memset( ssid, 0, WIFI_SSID_LEN );
    }
}

bool wifi_b_ap_mode( void ){

    return ( wifi_status_reg & WIFI_STATUS_AP_MODE ) != 0;
}

bool wifi_b_attached( void ){

    return wifi_status >= WIFI_STATE_ALIVE;
}

bool wifi_b_shutdown( void ){

    return wifi_status == WIFI_STATE_SHUTDOWN;
}

int8_t wifi_i8_get_status( void ){

    return wifi_status;
}

#elif !defined(ESP8266) && !defined(ESP32)
bool wifi_b_connected( void ){

    return FALSE;
}

bool wifi_b_ap_mode( void ){

    return FALSE;
}

#endif

