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

#ifdef ENABLE_WIFI

#include "threading.h"
#include "timers.h"
#include "os_irq.h"
#include "keyvalue.h"
#include "hal_usart.h"
#include "esp8266.h"
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


#define WIFI_COMM_TIMEOUT 20000
#define WIFI_CONNECT_TIMEOUT 10000

static uint16_t ports[WIFI_MAX_PORTS];
static bool run_manager;

static int8_t wifi_status;
static uint8_t wifi_mac[6];
static uint8_t wifi_status_reg;
static bool connected;
static int8_t wifi_rssi;
static uint32_t wifi_uptime;
static bool default_ap_mode;

static uint16_t wifi_comm_errors;
static uint16_t wifi_comm_errors2;
static uint8_t wifi_connects;

static uint16_t wifi_rx_udp_overruns;
static uint32_t wifi_udp_received;
static uint32_t wifi_udp_sent;
static uint16_t mem_heap_peak;
static uint16_t mem_used;
static uint16_t intf_max_time;
static uint16_t vm_max_time;
static uint16_t wifi_max_time;
static uint16_t mem_max_time;
static uint16_t intf_avg_time;
static uint16_t vm_avg_time;
static uint16_t wifi_avg_time;
static uint16_t mem_avg_time;

static uint32_t comm_rx_rate;
static uint32_t comm_tx_rate;

static uint16_t wifi_version;

static netmsg_t rx_netmsg;
static uint16_t rx_netmsg_index;
static uint16_t rx_netmsg_crc;

static uint8_t router;

static uint16_t max_ready_wait;

static uint8_t comm_stalls;

static list_t netmsg_list;
static bool udp_busy;

static uint8_t watchdog;

static mem_handle_t wifi_networks_handle = -1;


KV_SECTION_META kv_meta_t wifi_cfg_kv[] = {
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ssid" },
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_password" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid2" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password2" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid3" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password3" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid4" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password4" },
    { SAPPHIRE_TYPE_UINT8,         0, KV_FLAGS_PERSIST,           &router,            0,                   "wifi_router" },
    { SAPPHIRE_TYPE_BOOL,          0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_enable_ap" },    
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_ssid" },
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_password" },
    { SAPPHIRE_TYPE_KEY128,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_md5" },
    { SAPPHIRE_TYPE_UINT32,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_fw_len" },
};

KV_SECTION_META kv_meta_t wifi_info_kv[] = {
    { SAPPHIRE_TYPE_INT8,          0, 0, &wifi_status,                      0,   "wifi_status" },
    { SAPPHIRE_TYPE_MAC48,         0, 0, &wifi_mac,                         0,   "wifi_mac" },
    { SAPPHIRE_TYPE_INT8,          0, 0, &wifi_rssi,                        0,   "wifi_rssi" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_uptime,                      0,   "wifi_uptime" },
    { SAPPHIRE_TYPE_UINT8,         0, 0, &wifi_connects,                    0,   "wifi_connects" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_version,                     0,   "wifi_version" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_comm_errors,                 0,   "wifi_comm_errors" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_comm_errors2,                0,   "wifi_comm_errors2" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_rx_udp_overruns,             0,   "wifi_comm_rx_udp_overruns" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_udp_received,                0,   "wifi_udp_received" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_udp_sent,                    0,   "wifi_udp_sent" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &mem_heap_peak,                    0,   "wifi_mem_heap_peak" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &mem_used,                         0,   "wifi_mem_used" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &intf_max_time,                    0,   "wifi_proc_intf_max_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &vm_max_time,                      0,   "wifi_proc_vm_max_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_max_time,                    0,   "wifi_proc_wifi_max_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &mem_max_time,                     0,   "wifi_proc_mem_max_time" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &intf_avg_time,                    0,   "wifi_proc_intf_avg_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &vm_avg_time,                      0,   "wifi_proc_vm_avg_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &wifi_avg_time,                    0,   "wifi_proc_wifi_avg_time" },
    { SAPPHIRE_TYPE_UINT16,        0, 0, &mem_avg_time,                     0,   "wifi_proc_mem_avg_time" },

    { SAPPHIRE_TYPE_UINT16,        0, 0, &max_ready_wait,                   0,   "wifi_max_ready_wait" },

    { SAPPHIRE_TYPE_UINT32,        0, 0, &comm_tx_rate,                     0,   "wifi_comm_rate_tx" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &comm_rx_rate,                     0,   "wifi_comm_rate_rx" },

    { SAPPHIRE_TYPE_UINT8,         0, 0, &comm_stalls,                      0,   "wifi_comm_stalls" },
};


static bool is_udp_rx_released( void ){

    if( udp_busy ){

        if( !sock_b_rx_pending() ){

            return TRUE;
        }
    }

    return FALSE;
}


bool wifi_b_comm_ready( void ){

    return hal_wifi_b_comm_ready();
}

// waits up to WIFI_COMM_TIMEOUT microseconds for comm to be ready
bool wifi_b_wait_comm_ready( void ){

    if( watchdog == 0 ){

        return FALSE;
    }

    
    BUSY_WAIT_TIMEOUT( !hal_wifi_b_comm_ready(), WIFI_COMM_TIMEOUT );

    return hal_wifi_b_comm_ready();
}


int8_t wifi_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    ASSERT( len <= WIFI_MAX_DATA_LEN );
    
    if( !hal_wifi_b_comm_ready() ){

        log_v_debug_P( PSTR("rx not ready! %x"), data_id ); 

        return -1;  
    }

    uint16_t crc = crc_u16_start();

    wifi_data_header_t header;
    header.len      = len;
    header.reserved = 0;
    header.data_id  = data_id;
    header.crc      = 0;
    crc = crc_u16_partial_block( crc, (uint8_t *)&header, sizeof(header) );    

    crc = crc_u16_partial_block( crc, data, len );

    header.crc = crc_u16_finish( crc );


    hal_wifi_v_clear_rx_ready();

    hal_wifi_v_usart_send_char( WIFI_COMM_DATA );
    hal_wifi_v_usart_send_data( (uint8_t *)&header, sizeof(header) );
    hal_wifi_v_usart_send_data( data, len );

    return 0;
}

int8_t wifi_i8_send_msg_blocking( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( !wifi_b_wait_comm_ready() ){

        return -1;
    }

    if( wifi_i8_send_msg( data_id, data, len ) < 0 ){

        return -2;
    }

    return 0;
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

                    run_manager = TRUE;

                    break;
                }
            }
        }
        else{

            // search for port
            for( uint8_t i = 0; i < cnt_of_array(ports); i++ ){

                if( ports[i] == port ){

                    ports[i] = 0;

                    run_manager = TRUE;

                    break;
                }
            }
        }
    }
}

static void transmit_udp( netmsg_t tx_netmsg ){

    netmsg_state_t *netmsg_state = netmsg_vp_get_state( tx_netmsg );

    ASSERT( netmsg_state->type == NETMSG_TYPE_UDP );

    uint16_t data_len = 0;

    uint16_t crc = crc_u16_start();

    uint8_t *data = 0;
    uint8_t *h2 = 0;
    uint16_t h2_len = 0;

    if( netmsg_state->data_handle > 0 ){

        data = mem2_vp_get_ptr( netmsg_state->data_handle );
        data_len = mem2_u16_get_size( netmsg_state->data_handle );
    }

    // header 2, if present
    if( netmsg_state->header_2_handle > 0 ){

        h2 = mem2_vp_get_ptr( netmsg_state->header_2_handle );
        h2_len = mem2_u16_get_size( netmsg_state->header_2_handle );
    }

    // setup header
    wifi_msg_udp_header_t udp_header;
    udp_header.addr = netmsg_state->raddr.ipaddr;
    udp_header.lport = netmsg_state->laddr.port;
    udp_header.rport = netmsg_state->raddr.port;
    udp_header.len = data_len + h2_len;
    udp_header.crc = 0;
    

    uint16_t len = data_len + h2_len + sizeof(udp_header);

    wifi_data_header_t header;
    header.len      = len;
    header.reserved = 0;
    header.data_id  = WIFI_DATA_ID_UDP_EXT;
    header.crc      = 0;                

    crc = crc_u16_partial_block( crc, (uint8_t *)&header, sizeof(header) );
    crc = crc_u16_partial_block( crc, (uint8_t *)&udp_header, sizeof(udp_header) );
    crc = crc_u16_partial_block( crc, h2, h2_len );
    crc = crc_u16_partial_block( crc, data, data_len );
    header.crc = crc_u16_finish( crc );

    hal_wifi_v_clear_rx_ready();

    hal_wifi_v_usart_send_char( WIFI_COMM_DATA );
    hal_wifi_v_usart_send_data( (uint8_t *)&header, sizeof(header) );
    hal_wifi_v_usart_send_data( (uint8_t *)&udp_header, sizeof(udp_header) );
    hal_wifi_v_usart_send_data( h2, h2_len );
    hal_wifi_v_usart_send_data( data, data_len );   


    // release netmsg
    netmsg_v_release( tx_netmsg );
}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){

    if( !wifi_b_connected() ){

        return NETMSG_TX_ERR_RELEASE;
    }

    // check if wifi comm is ready.
    // if so, we can just send now instead of queuing.
    if( hal_wifi_b_comm_ready() ){

        transmit_udp( netmsg );

        return NETMSG_TX_OK_NORELEASE;
    }

    if( list_u8_count( &netmsg_list ) >= WIFI_MAX_NETMSGS ){

        log_v_debug_P( PSTR("tx udp overflow") );

        return NETMSG_TX_ERR_RELEASE;   
    }

    // add to list
    list_v_insert_head( &netmsg_list, netmsg );

    // signal comm thread
    thread_v_signal( WIFI_SIGNAL );

    return NETMSG_TX_OK_NORELEASE;
}


typedef struct{
    uint8_t timeout;
    file_t fw_file;
    uint8_t tries;
} loader_thread_state_t;

PT_THREAD( wifi_loader_thread( pt_t *pt, loader_thread_state_t *state ) );



PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // check if we are connected
    while( !wifi_b_connected() ){

        wifi_rssi = -127;
        
        THREAD_WAIT_WHILE( pt, !hal_wifi_b_comm_ready() );

        bool ap_mode = wifi_b_ap_mode_enabled();


        char ssid[WIFI_SSID_LEN];
        cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );
        
        // check if wifi settings were present
        if( ssid[0] == 0 ){        

            // switch to AP mode
            ap_mode = TRUE;
        }

        // station mode
        if( !ap_mode ){
    
            // log_v_debug_P( PSTR("Start scan") );

            // // run a scan
            // wifi_i8_send_msg( WIFI_DATA_ID_WIFI_SCAN, 0, 0 );

            // thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            // THREAD_WAIT_WHILE( pt, ( wifi_networks_handle < 0 ) &&
            //                        ( thread_b_alarm_set() ) );


            // uint32_t network_hashes[4];
            // memset( network_hashes, 0, sizeof(network_hashes) );
            // int8_t selected_network = -1;

            // if( wifi_networks_handle > 0 ){

            //     // gather available networks
            //     char ssid[WIFI_SSID_LEN];

            //     memset( ssid, 0, sizeof(ssid) );
            //     cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );
            //     network_hashes[0] = hash_u32_string( ssid );

            //     memset( ssid, 0, sizeof(ssid) );
            //     kv_i8_get( __KV__wifi_ssid2, ssid, sizeof(ssid) );
            //     network_hashes[1] = hash_u32_string( ssid );

            //     memset( ssid, 0, sizeof(ssid) );
            //     kv_i8_get( __KV__wifi_ssid3, ssid, sizeof(ssid) );
            //     network_hashes[2] = hash_u32_string( ssid );

            //     memset( ssid, 0, sizeof(ssid) );
            //     kv_i8_get( __KV__wifi_ssid4, ssid, sizeof(ssid) );
            //     network_hashes[3] = hash_u32_string( ssid );


            //     wifi_msg_scan_results_t *msg = (wifi_msg_scan_results_t *)mem2_vp_get_ptr( wifi_networks_handle );

            //     // search for matching networks and track best signal
            //     // int8_t best_rssi = -120;
            //     int8_t best_index = -1;

            //     for( uint8_t i = 0; i < msg->count; i++ ){

            //         // log_v_debug_P(PSTR("%ld %lu"), msg->networks[i].rssi, msg->networks[i].ssid_hash );

            //         // is this RSSI any good?

            //         // if( msg->networks[i].rssi <= best_rssi ){

            //         //     continue;
            //         // }

            //         // do we have this SSID?
            //         for( uint8_t j = 0; j < cnt_of_array(network_hashes); j++ ){

            //             if( network_hashes[j] == msg->networks[i].ssid_hash ){

            //                 // match!

            //                 // record RSSI and index
            //                 best_index = j;
            //                 // best_rssi = msg->networks[i].rssi;

            //                 break;
            //             }
            //         }
            //     }

            //     if( best_index >= 0 ){

            //         // log_v_debug_P( PSTR("Best: %d %lu"), best_rssi, network_hashes[best_index] );

            //         selected_network = best_index;
            //     }

            //     mem2_v_free( wifi_networks_handle );
            //     wifi_networks_handle = -1;     
            // }
            
                
            wifi_msg_connect_t msg;
            memset( &msg, 0, sizeof(msg) );

            if( router == 1 ){

                kv_i8_get( __KV__wifi_ssid2, msg.ssid, sizeof(msg.ssid) );
                kv_i8_get( __KV__wifi_password2, msg.pass, sizeof(msg.pass) );  
            }
            else if( router == 2 ){

                kv_i8_get( __KV__wifi_ssid3, msg.ssid, sizeof(msg.ssid) );
                kv_i8_get( __KV__wifi_password3, msg.pass, sizeof(msg.pass) );  
            }
            else if( router == 3 ){

                kv_i8_get( __KV__wifi_ssid4, msg.ssid, sizeof(msg.ssid) );
                kv_i8_get( __KV__wifi_password4, msg.pass, sizeof(msg.pass) );  
            }
            else{

                cfg_i8_get( CFG_PARAM_WIFI_SSID, msg.ssid );
                cfg_i8_get( CFG_PARAM_WIFI_PASSWORD, msg.pass );
            }

            // check if a router was configured
            if( msg.ssid[0] == 0 ){

                router++;

                if( router >= 4 ){

                    router = 0;
                }

                THREAD_YIELD( pt );

                continue;
            }

            log_v_debug_P( PSTR("Connecting to: %s"), msg.ssid );
            wifi_i8_send_msg( WIFI_DATA_ID_CONNECT, (uint8_t *)&msg, sizeof(msg) );

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                   ( thread_b_alarm_set() ) );

            // check if connected
            if( wifi_b_connected() ){

                kv_i8_persist( __KV__wifi_router );
            }
            else{
                // not connected, try next router

                router++;

                if( router >= 4 ){

                    router = 0;

                    // if we are in recovery mode and we've cycled through all
                    // of the routers, start up the AP
                    if( sys_b_is_recovery_mode() ){

                        default_ap_mode = TRUE;
                    }
                }
            }
        }
        // AP mode
        else{

            // wait until we have a MAC address
            THREAD_WAIT_WHILE( pt, wifi_mac[0] == 0 );

            log_v_debug_P( PSTR("starting AP") );

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

            log_v_debug_P( PSTR("ssid: %s"), ap_msg.ssid );

            // check if wifi settings were present
            if( ap_msg.ssid[0] != 0 ){        

                wifi_i8_send_msg( WIFI_DATA_ID_AP_MODE, (uint8_t *)&ap_msg, sizeof(ap_msg) );

                thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
                THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                       ( thread_b_alarm_set() ) );
            }
        }

end:
        if( !wifi_b_connected() ){

            TMR_WAIT( pt, 500 );
        }
    }     

    while( wifi_b_connected() ){

        if( !connected ){

            if( !wifi_b_ap_mode_enabled() ){

                wifi_connects++;
                connected = TRUE;
                log_v_debug_P( PSTR("Wifi connected") );
            }
        }

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 2000 );    
        THREAD_WAIT_WHILE( pt, ( run_manager == FALSE ) &&
                               ( thread_b_alarm_set() ) );

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_comm_ready() );
        wifi_i8_send_msg( WIFI_DATA_ID_PORTS, (uint8_t *)&ports, sizeof(ports) );

        run_manager = FALSE;
    }

    connected = FALSE;
    log_v_debug_P( PSTR("Wifi disconnected") );

    THREAD_RESTART( pt );

PT_END( pt );
}


static int8_t process_rx_data( void ){

    int8_t status = 0;

    int16_t rx_bytes = hal_wifi_i16_rx_data_received();

    if( rx_bytes < 0 ){

        return -1;
    }

    // reset buffer control byte
    hal_wifi_v_reset_control_byte();

    uint8_t buf[WIFI_UART_RX_BUF_SIZE];

    uint8_t *rx_buf = hal_wifi_u8p_get_rx_buf_ptr();

    wifi_data_header_t *header = (wifi_data_header_t *)&rx_buf[1];

    uint16_t msg_len = sizeof(wifi_data_header_t) + header->len ;

    // validate msg size
    if( msg_len > WIFI_UART_RX_BUF_SIZE ){

        log_v_debug_P( PSTR("Wifi msg len error. ID:0x%02x len:%d"), header->data_id, header->len );
        goto len_error;
    }   

    memcpy( buf, &rx_buf[1], msg_len );

    hal_wifi_v_release_rx_buffer();

    header = (wifi_data_header_t *)buf;
    uint8_t *data = (uint8_t *)( header + 1 );

    uint16_t msg_crc = header->crc;
    header->crc = 0;

    uint16_t computed_crc = crc_u16_block( (uint8_t *)header, msg_len );
    if( computed_crc != msg_crc ){

        log_v_debug_P( PSTR("Wifi crc error. ID:0x%02x len:%d crc: 0x%04x != 0x%04x"), header->data_id, header->len, computed_crc, msg_crc );
        status = -2;
        goto end;
    }

    if( header->data_id == WIFI_DATA_ID_STATUS ){

        if( header->len != sizeof(wifi_msg_status_t) ){

            goto len_error;
        }

        wifi_msg_status_t *msg = (wifi_msg_status_t *)data;

        wifi_status_reg = msg->flags;
    }  
    else if( header->data_id == WIFI_DATA_ID_INFO ){

        if( header->len != sizeof(wifi_msg_info_t) ){

            goto len_error;
        }

        wifi_msg_info_t *msg = (wifi_msg_info_t *)data;

        wifi_version            = msg->version;
        
        if( wifi_b_connected() ){
            
            wifi_rssi               = msg->rssi;
        }
        
        memcpy( wifi_mac, msg->mac, sizeof(wifi_mac) );

        uint64_t current_device_id = 0;
        cfg_i8_get( CFG_PARAM_DEVICE_ID, &current_device_id );
        uint64_t device_id = 0;
        memcpy( &device_id, wifi_mac, sizeof(wifi_mac) );

        if( current_device_id != device_id ){

            cfg_v_set( CFG_PARAM_DEVICE_ID, &device_id );
        }

        cfg_v_set( CFG_PARAM_IP_ADDRESS, &msg->ip );
        cfg_v_set( CFG_PARAM_IP_SUBNET_MASK, &msg->subnet );
        cfg_v_set( CFG_PARAM_DNS_SERVER, &msg->dns );

        wifi_rx_udp_overruns        = msg->rx_udp_overruns;
        wifi_udp_received           = msg->udp_received;
        wifi_udp_sent               = msg->udp_sent;
        wifi_comm_errors            = msg->comm_errors;
        mem_heap_peak               = msg->mem_heap_peak;
        mem_used                    = msg->mem_used;

        intf_max_time               = msg->intf_max_time;
        vm_max_time                 = msg->vm_max_time;
        wifi_max_time               = msg->wifi_max_time;
        mem_max_time                = msg->mem_max_time;

        intf_avg_time               = msg->intf_avg_time;
        vm_avg_time                 = msg->vm_avg_time;
        wifi_avg_time               = msg->wifi_avg_time;
        mem_avg_time                = msg->mem_avg_time;
    }
    else if( header->data_id == WIFI_DATA_ID_UDP_HEADER ){

        if( header->len < sizeof(wifi_msg_udp_header_t) ){

            goto len_error;
        }

        wifi_msg_udp_header_t *msg = (wifi_msg_udp_header_t *)data;

        // check if sockets module is busy
        if( sock_b_rx_pending() ){

            log_v_debug_P( PSTR("sock rx pending: %u"), msg->rport );
            goto error;
        }
        
        #ifndef SOCK_SINGLE_BUF
        // check if port is busy
        if( sock_b_port_busy( msg->rport ) ){

            log_v_debug_P( PSTR("port busy: %u"), msg->rport );
            goto error;
        }
        #endif

        // check if we have a netmsg that didn't get freed for some reason
        if( rx_netmsg > 0 ){

            log_v_debug_P( PSTR("freeing loose netmsg") );     

            netmsg_v_release( rx_netmsg );
            rx_netmsg = -1;
        }

        // allocate netmsg
        rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

        if( rx_netmsg < 0 ){

            log_v_debug_P( PSTR("rx udp alloc fail") );     

            goto error;
        }

        netmsg_state_t *state = netmsg_vp_get_state( rx_netmsg );

        // allocate data buffer
        state->data_handle = mem2_h_alloc2( msg->len, MEM_TYPE_SOCKET_BUFFER );

        if( state->data_handle < 0 ){

            log_v_debug_P( PSTR("rx udp no handle") );     

            netmsg_v_release( rx_netmsg );
            rx_netmsg = 0;

            goto error;
        }      


        udp_busy = TRUE;

        // set up address info
        state->laddr.port   = msg->lport;
        state->raddr.port   = msg->rport;
        state->raddr.ipaddr = msg->addr;

        rx_netmsg_crc       = msg->crc;

        // copy data
        data += sizeof(wifi_msg_udp_header_t);

        uint16_t data_len = header->len - sizeof(wifi_msg_udp_header_t);

        // we can get a fast ptr because we've already verified the handle
        memcpy( mem2_vp_get_ptr_fast( state->data_handle ), data, data_len );

        rx_netmsg_index = data_len;
    }
    else if( header->data_id == WIFI_DATA_ID_UDP_DATA ){

        if( rx_netmsg <= 0 ){

            // log_v_debug_P( PSTR("rx udp no netmsg") );     

            goto error;
        }

        netmsg_state_t *state = netmsg_vp_get_state( rx_netmsg );
        uint8_t *ptr = mem2_vp_get_ptr( state->data_handle );        
        uint16_t total_len = mem2_u16_get_size( state->data_handle );

        // bounds check
        if( ( header->len + rx_netmsg_index ) > total_len ){

            log_v_debug_P( PSTR("rx udp len error") );     

            // bad length, throwaway
            netmsg_v_release( rx_netmsg );
            rx_netmsg = 0;

            goto error;
        }

        memcpy( &ptr[rx_netmsg_index], data, header->len );

        rx_netmsg_index += header->len;

        // message is complete
        if( rx_netmsg_index == total_len ){

            // check crc
            if( crc_u16_block( ptr, total_len ) != rx_netmsg_crc ){

                netmsg_v_release( rx_netmsg );
                rx_netmsg = 0;

                log_v_debug_P( PSTR("rx udp crc error") );     

                goto error;
            }

            netmsg_v_receive( rx_netmsg );
            rx_netmsg = 0;

            // signals receiver that a UDP msg was received
            status = 1;
        }   
    }
    else if( header->data_id == WIFI_DATA_ID_WIFI_SCAN_RESULTS ){
    
        if( wifi_networks_handle < 0 ){        

            wifi_networks_handle = mem2_h_alloc( sizeof(wifi_msg_scan_results_t) );

            if( wifi_networks_handle > 0 ){

                memcpy( mem2_vp_get_ptr( wifi_networks_handle ), data, sizeof(wifi_msg_scan_results_t) );
            }
        }

        // wifi_msg_scan_results_t *msg = (wifi_msg_scan_results_t *)data;

        // for( uint8_t i = 0; i < msg->count; i++ ){

        //     log_v_debug_P(PSTR("%ld %lu"), msg->networks[i].rssi, msg->networks[i].ssid_hash );
        // }
    }
    else if( header->data_id == WIFI_DATA_ID_DEBUG_PRINT ){

        log_v_debug_P( PSTR("ESP: %s"), data );
    }
    // check if msg handler is installed
    else if( wifi_i8_msg_handler ){

        wifi_i8_msg_handler( header->data_id, data, header->len );
    }

    watchdog = WIFI_WATCHDOG_TIMEOUT;

    goto end;

len_error:

    wifi_comm_errors2++;

    log_v_debug_P( PSTR("Wifi len error: %d"), header->data_id );
    status = -3;    
    goto end;

error:
    wifi_comm_errors2++;
    status = -4;
    goto end;    

end:
    return status;
}


PT_THREAD( wifi_comm_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
restart:
    
    watchdog = WIFI_WATCHDOG_TIMEOUT;

    hal_wifi_v_enter_normal_mode();
    wifi_status_reg = 0;

    hal_wifi_v_clear_rx_ready();

    // delay while wifi boots up
    TMR_WAIT( pt, 300 );

    hal_wifi_v_enable_irq();
    
    hal_wifi_v_reset_control_byte();
    hal_wifi_v_reset_comm();

    TMR_WAIT( pt, 100 );

    if( !hal_wifi_b_comm_ready() ){

        goto restart;
    }

    wifi_status = WIFI_STATE_ALIVE;
    
    // set ready and wait for message
    hal_wifi_v_set_rx_ready();

    while(1){

        thread_v_set_signal_flag();
        THREAD_WAIT_WHILE( pt, ( list_u8_count( &netmsg_list ) == 0 ) && 
                               ( !thread_b_signalled( WIFI_SIGNAL ) ) && 
                               ( hal_wifi_u8_get_control_byte() == WIFI_COMM_IDLE ) &&
                               ( !is_udp_rx_released() ) );
        thread_v_clear_signal( WIFI_SIGNAL );
        thread_v_clear_signal_flag();

        if( watchdog == 0 ){

            log_v_debug_P( PSTR("Wifi watchdog triggered") );

            // reboot to safe mode
            sys_v_reboot_delay( SYS_MODE_SAFE );
        
            goto restart;
        }

        // check if UDP buffer is clear (and transmit interface is available)
        if( is_udp_rx_released() && hal_wifi_b_comm_ready() ){
            
            wifi_i8_send_msg( WIFI_DATA_ID_UDP_BUF_READY, 0, 0 );

            udp_busy = FALSE;        
        }

        // check control byte for a ready query
        if( hal_wifi_u8_get_control_byte() == WIFI_COMM_QUERY_READY ){

            log_v_debug_P( PSTR("query ready") );
            hal_wifi_v_reset_control_byte();

            // send ready signal
            // this will also reset the DMA engine.
            hal_wifi_v_set_rx_ready();

            if( comm_stalls < 255 ){

                comm_stalls++;
            }

            continue;
        }

        uint8_t msgs_received = 0;

        // check for udp transmission
        if( list_u8_count( &netmsg_list ) > 0 ){

            // check if transmission is available
            if( !hal_wifi_b_comm_ready() ){

                // comm is not ready.

                // re-signal thread, then bail out to receive handler
                thread_v_signal( WIFI_SIGNAL );

                goto receive;
            }

            
            netmsg_t tx_netmsg = list_ln_remove_tail( &netmsg_list );

            transmit_udp( tx_netmsg );
        }

receive:
        while( ( process_rx_data() == 0 ) &&
               ( msgs_received < 8 ) ){

            msgs_received++;
        }

        max_ready_wait = hal_wifi_u32_get_max_ready_wait();

        THREAD_YIELD( pt );
        THREAD_YIELD( pt );
    }

PT_END( pt );
}


PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        if( watchdog > 0 ){

            watchdog--;
        }

        if( wifi_b_connected() ){

            wifi_uptime += 1;
        }
        else{

            wifi_uptime = 0;
        }

        // update comm traffic counters
        comm_rx_rate = hal_wifi_u32_get_rx_bytes();
        comm_tx_rate = hal_wifi_u32_get_tx_bytes();

        
        if( hal_wifi_b_comm_ready() ){

            // send options message

            wifi_msg_set_options_t options_msg;
            memset( options_msg.padding, 0, sizeof(options_msg.padding) );
            options_msg.led_quiet = cfg_b_get_boolean( CFG_PARAM_ENABLE_LED_QUIET_MODE );
            options_msg.low_power = cfg_b_get_boolean( CFG_PARAM_ENABLE_LOW_POWER_MODE );

            if( kv_i8_get( __KV__midi_channel, &options_msg.midi_channel, sizeof(options_msg.midi_channel) ) < 0 ){

                options_msg.midi_channel = -1;                
            }

            wifi_i8_send_msg( WIFI_DATA_ID_SET_OPTIONS, (uint8_t *)&options_msg, sizeof(options_msg) );
        }
    }

PT_END( pt );
}



PT_THREAD( wifi_loader_thread( pt_t *pt, loader_thread_state_t *state ) )
{
PT_BEGIN( pt );

    state->fw_file = 0;
    state->tries = WIFI_LOADER_MAX_TRIES;

    // log_v_debug_P( PSTR("wifi loader starting") );
    trace_printf( "Starting Wifi loader\r\n" );
    
restart:

    // check if we've exceeded our retries
    if( state->tries == 0 ){

        // wifi is hosed, give up.
        goto error;
    }

    state->tries--;

    hal_wifi_v_enter_boot_mode();
    wifi_status_reg = 0;

    wifi_status = WIFI_STATE_BOOT;

    if( state->fw_file == 0 ){

        state->fw_file = fs_f_open_P( PSTR("wifi_firmware.bin"), FS_MODE_READ_ONLY );
    }

    if( state->fw_file <= 0 ){

        goto run_wifi;
    }

    // delay while wifi boots up
    TMR_WAIT( pt, 250 );

    state->timeout = 10;
    while( state->timeout > 0 ){

        hal_wifi_v_usart_flush();

        state->timeout--;

        if( state->timeout == 0 ){

            log_v_debug_P( PSTR("wifi loader timeout") );

            goto restart;
        }

        uint8_t buf[32];
        memset( buf, 0xff, sizeof(buf) );


        hal_wifi_v_clear_rx_buffer();
        hal_wifi_v_enable_rx_dma( FALSE );

        // ESP seems to miss the first sync for some reason,
        // so we'll just send twice.
        // it's not really a big deal from a timing standpoint since
        // we'd try again in a few milliseconds, but if the wait response
        // function is doing error logging, it saves us a pointless error
        // message on every start up.
        esp_v_send_sync();
        esp_v_send_sync();

        // blocking wait!
        int8_t status = esp_i8_wait_response( buf, sizeof(buf), ESP_SYNC_TIMEOUT );

        

        if( status == 0 ){

            esp_response_t *resp = (esp_response_t *)buf;

            if( resp->opcode == ESP_SYNC ){

                break;
            }
        }

        TMR_WAIT( pt, 5 );
    }

    // delay, as Sync will output several responses
    TMR_WAIT( pt, 50 );

    int8_t status = esp_i8_load_cesanta_stub();

    if( status < 0 ){

        log_v_debug_P( PSTR("error %d"), status );

        TMR_WAIT( pt, 500 );
        goto restart;
    }

    // change baud rate
    hal_wifi_v_usart_set_baud( ESP_CESANTA_BAUD_USART_SETTING );
    hal_wifi_v_usart_flush();    

    // clear buffer
    hal_wifi_v_clear_rx_buffer();
    // re-enable rx buffer
    hal_wifi_v_enable_rx_dma( FALSE );

    // cesanta stub has a delay, so make sure we wait plenty long enough
    _delay_ms( 50 );

    uint8_t *rx_dma_buf = hal_wifi_u8p_get_rx_dma_buf_ptr();

    bool ok = FALSE;
    // now check buffer, Cesanta will send us a hello message
    for( uint8_t i = 0; i < 4; i++ ){

        if( ( ( rx_dma_buf[i + 0] == SLIP_END ) &&
              ( rx_dma_buf[i + 1] == 'O' ) &&
              ( rx_dma_buf[i + 2] == 'H' ) &&
              ( rx_dma_buf[i + 3] == 'A' ) &&
              ( rx_dma_buf[i + 4] == 'I' ) &&
              ( rx_dma_buf[i + 5] == SLIP_END ) ) ){

            ok = TRUE;
            break;
        }
    }

    if( !ok ){

        log_v_debug_P( PSTR("error: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x"),
                rx_dma_buf[0],
                rx_dma_buf[1],
                rx_dma_buf[2],
                rx_dma_buf[3],
                rx_dma_buf[4],
                rx_dma_buf[5] );

        hal_wifi_v_disable_rx_dma();

        TMR_WAIT( pt, 500 );
        goto restart;
    }

    status_led_v_set( 1, STATUS_LED_BLUE );

    trace_printf( "Cesanta flasher ready!\r\n" );
    // log_v_debug_P( PSTR("Cesanta flasher ready!") );

    uint32_t file_len;
    cfg_i8_get( CFG_PARAM_WIFI_FW_LEN, &file_len );

    uint8_t wifi_digest[MD5_LEN];

    int8_t md5_status = esp_i8_md5( file_len, wifi_digest );
    if( md5_status < 0 ){

        log_v_debug_P( PSTR("error %d"), md5_status );

        TMR_WAIT( pt, 1000 );
        goto restart;
    }

    fs_v_seek( state->fw_file, file_len );

    uint8_t file_digest[MD5_LEN];
    memset( file_digest, 0, MD5_LEN );
    fs_i16_read( state->fw_file, file_digest, MD5_LEN );

    uint8_t cfg_digest[MD5_LEN];
    cfg_i8_get( CFG_PARAM_WIFI_MD5, cfg_digest );

    if( memcmp( file_digest, cfg_digest, MD5_LEN ) == 0 ){

        // file and cfg match, so our file is valid


        if( memcmp( file_digest, wifi_digest, MD5_LEN ) == 0 ){

            // all 3 match, run wifi
            // log_v_debug_P( PSTR("Wifi firmware image valid") );

            goto run_wifi;
        }
        else{
            // wifi does not match file - need to load

            log_v_debug_P( PSTR("Wifi firmware image fail") );

            goto load_image;
        }
    }
    else{

        log_v_debug_P( PSTR("Wifi MD5 mismatch, possible bad file load") );        
    }

    if( memcmp( wifi_digest, cfg_digest, MD5_LEN ) == 0 ){

        // wifi matches cfg, this is ok, run.
        // maybe the file is bad.
        
        goto run_wifi;
    }
    else{

        log_v_debug_P( PSTR("Wifi MD5 mismatch, possible bad config") );                
    }

    if( memcmp( wifi_digest, file_digest, MD5_LEN ) == 0 ){

        // in this case, file matches wifi, so our wifi image is valid
        // and so is our file.
        // but our cfg is mismatched.
        // so we'll restore it and then run the wifi

        cfg_v_set( CFG_PARAM_WIFI_MD5, file_digest );

        log_v_debug_P( PSTR("Wifi MD5 mismatch, restored from file") );

        goto run_wifi;
    }

    // probably don't want to actually assert here...

    // try to run anyway
    goto run_wifi;



load_image:
    
    #ifdef ENABLE_USB
    usb_v_detach();
    #endif

    log_v_debug_P( PSTR("Loading wifi image...") );


    if( esp_i8_load_flash( state->fw_file ) < 0 ){

        log_v_debug_P( PSTR("error") );
        goto error;
    }

    memset( wifi_digest, 0xff, MD5_LEN );

    if( esp_i8_md5( file_len, wifi_digest ) < 0 ){

        log_v_debug_P( PSTR("error") );
        goto restart;
    }

    // verify

    log_v_debug_P( PSTR("Wifi flash load done") );

    if( state->fw_file > 0 ){

        fs_f_close( state->fw_file );
    }

    // restart
    sys_reboot();

    THREAD_EXIT( pt );

error:

    hal_wifi_v_reset(); // hold chip in reset

    status_led_v_set( 1, STATUS_LED_RED );

    if( state->fw_file > 0 ){

        fs_f_close( state->fw_file );
    }


    log_v_debug_P( PSTR("wifi load fail") );

    THREAD_EXIT( pt );

run_wifi:

    trace_printf( "Starting wifi!\r\n" );

    thread_t_create( wifi_comm_thread,
                     PSTR("wifi_comm"),
                     0,
                     0 );

    thread_t_create( wifi_connection_manager_thread,
                     PSTR("wifi_connection_manager"),
                     0,
                     0 );

    if( state->fw_file > 0 ){

        fs_f_close( state->fw_file );
    }

PT_END( pt );
}

PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static socket_t sock;

    sock = sock_s_create( SOCK_DGRAM );
    sock_v_bind( sock, 7 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        EVENT( EVENT_ID_DEBUG_0, 2 );

        if( sock_i16_sendto( sock, sock_vp_get_data( sock ), sock_i16_get_bytes_read( sock ), 0 ) >= 0 ){
            
        }
    }

PT_END( pt );
}


int8_t get_route( ip_addr_t *subnet, ip_addr_t *subnet_mask ){

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

    list_v_init( &netmsg_list );

    hal_wifi_v_init();

    wifi_status = WIFI_STATE_BOOT;


    thread_t_create( THREAD_CAST(wifi_loader_thread),
                        PSTR("wifi_loader"),
                        0,
                        sizeof(loader_thread_state_t) );


    thread_t_create( wifi_status_thread,
                        PSTR("wifi_status"),
                        0,
                        0 );

    thread_t_create( wifi_echo_thread,
                        PSTR("wifi_echo"),
                        0,
                        0 );
}

void wifi_v_shutdown( void ){

    wifi_i8_send_msg_blocking( WIFI_DATA_ID_SHUTDOWN, 0, 0 );
}

bool wifi_b_connected( void ){

    return ( wifi_status_reg & WIFI_STATUS_CONNECTED ) != 0;
}

int8_t wifi_i8_rssi( void ){

    return wifi_rssi;
}

void wifi_v_get_ssid( char ssid[WIFI_SSID_LEN] ){

    if( router == 1 ){

        kv_i8_get( __KV__wifi_ssid2, ssid, WIFI_SSID_LEN );
    }
    else if( router == 2 ){

        kv_i8_get( __KV__wifi_ssid3, ssid, WIFI_SSID_LEN ); 
    }
    else if( router == 3 ){

        kv_i8_get( __KV__wifi_ssid4, ssid, WIFI_SSID_LEN );
    }
    else{

        cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );
    }
}

bool wifi_b_ap_mode( void ){

    return ( wifi_status_reg & WIFI_STATUS_AP_MODE ) != 0;
}

bool wifi_b_ap_mode_enabled( void ){

    if( default_ap_mode ){

        return TRUE;
    }

    bool wifi_enable_ap = FALSE;
    cfg_i8_get( CFG_PARAM_WIFI_ENABLE_AP, &wifi_enable_ap );
    
    return wifi_enable_ap;    
}

bool wifi_b_attached( void ){

    return wifi_status >= WIFI_STATE_ALIVE;
}

int8_t wifi_i8_get_status( void ){

    return wifi_status;
}

uint32_t wifi_u32_get_received( void ){

    return wifi_udp_received;
}

bool wifi_b_running( void ){

    return TRUE;
}

#endif