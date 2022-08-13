/*
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
 */

#include "Arduino.h"
#include "comm_intf.h"
#include "comm_printf.h"
#include "wifi.h"
#include "version.h"
#include "options.h"
#include "uart.h"
#include <ESP8266WiFi.h>

extern "C"{
    #include "crc.h"
    #include "wifi_cmd.h"
    #include "list.h"
    #include "memory.h"
    #include "comm_printf.h"
}

typedef struct{
    uint16_t data_id;
    uint16_t len;
} buffered_msg_t;

#define WIFI_COMM_TRIES         3
#define WIFI_COMM_TIMEOUT       20000

static int8_t _intf_i8_transmit_msg( uint8_t data_id, uint8_t *data, uint16_t len );

static void assert_irq( void ){

    digitalWrite( IRQ_GPIO, HIGH );    
}

static void release_irq( void ){

    digitalWrite( IRQ_GPIO, LOW );    
}

static uint32_t start_timeout( void ){

    return micros();
}

static int32_t elapsed( uint32_t start ){

    uint32_t now = micros();
    int32_t distance = (int32_t)( now - start );

    // check for rollover
    if( distance < 0 ){

        distance = ( UINT32_MAX - now ) + abs(distance);
    }

    return distance;
}



static list_t tx_q;
static uint16_t comm_errors;

static process_stats_t process_stats;



#define COMM_STATE_IDLE          0
#define COMM_STATE_RX_HEADER     1
#define COMM_STATE_RX_DATA       2


void intf_v_led_on( void ){

    if( opt_b_get_led_quiet() ){

        return;
    }

    digitalWrite( LED_GPIO, LOW );
}

void intf_v_led_off( void ){

    digitalWrite( LED_GPIO, HIGH );
}

static void _intf_v_flush(){

    Serial.flush();
    while( Serial.read() >= 0 );
}

static void _send_info_msg( void ){

    wifi_msg_info_t info_msg;
    info_msg.version_major = VERSION_MAJOR;
    info_msg.version_minor = VERSION_MINOR;
    info_msg.version_patch = VERSION_PATCH;

    WiFi.macAddress( info_msg.mac );
    
    IPAddress ip_addr = wifi_ip_get_ip();
    info_msg.ip.ip0 = ip_addr[3];
    info_msg.ip.ip1 = ip_addr[2];
    info_msg.ip.ip2 = ip_addr[1];
    info_msg.ip.ip3 = ip_addr[0];

    ip_addr = wifi_ip_get_subnet();
    info_msg.subnet.ip0 = ip_addr[3];
    info_msg.subnet.ip1 = ip_addr[2];
    info_msg.subnet.ip2 = ip_addr[1];
    info_msg.subnet.ip3 = ip_addr[0];

    ip_addr = WiFi.gatewayIP();
    info_msg.gateway.ip0 = ip_addr[3];
    info_msg.gateway.ip1 = ip_addr[2];
    info_msg.gateway.ip2 = ip_addr[1];
    info_msg.gateway.ip3 = ip_addr[0];

    ip_addr = WiFi.dnsIP();
    info_msg.dns.ip0 = ip_addr[3];
    info_msg.dns.ip1 = ip_addr[2];
    info_msg.dns.ip2 = ip_addr[1];
    info_msg.dns.ip3 = ip_addr[0];

    info_msg.rssi           = WiFi.RSSI();
    info_msg.wifi_channel   = wifi_i8_get_channel();

    info_msg.rx_udp_overruns        = wifi_u32_get_rx_udp_overruns();
    info_msg.udp_received           = wifi_u32_get_udp_received();
    info_msg.udp_sent               = wifi_u32_get_udp_sent();

    mem_rt_data_t rt_data;
    mem2_v_get_rt_data( &rt_data );

    info_msg.comm_errors            = comm_errors;

    info_msg.mem_heap_peak          = rt_data.peak_usage;
    info_msg.mem_used               = rt_data.used_space;

    info_msg.intf_max_time          = process_stats.intf_max_time;
    info_msg.wifi_max_time          = process_stats.wifi_max_time;
    info_msg.mem_max_time           = process_stats.mem_max_time;

    info_msg.intf_avg_time          = process_stats.intf_avg_time;
    info_msg.wifi_avg_time          = process_stats.wifi_avg_time;
    info_msg.mem_avg_time           = process_stats.mem_avg_time;

    info_msg.wifi_router            = wifi_i8_get_router();
    
    info_msg.flash_id               = spi_flash_get_id();

    _intf_i8_transmit_msg( WIFI_DATA_ID_INFO, (uint8_t *)&info_msg, sizeof(info_msg) );    
}

static void process_data( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( data_id == WIFI_DATA_ID_INFO ){

        _send_info_msg();        
    }
    else if( data_id == WIFI_DATA_ID_SCAN ){

        if( len != sizeof(wifi_msg_connect_t) ){

            return;
        }

        wifi_msg_connect_t *msg = (wifi_msg_connect_t *)data;

        for( uint32_t i = 0; i < WIFI_MAX_APS; i++ ){

            wifi_v_set_ap_info( msg->ssid[i], msg->pass[i], i );    
        }

        wifi_v_scan();
    }
    else if( data_id == WIFI_DATA_ID_CONNECT ){

        wifi_v_connect();
    }
    else if( data_id == WIFI_DATA_ID_AP_MODE ){

        if( len != sizeof(wifi_msg_ap_connect_t) ){

            return;
        }

        wifi_msg_ap_connect_t *msg = (wifi_msg_ap_connect_t *)data;

        wifi_v_set_ap_mode( msg->ssid, msg->pass );
    }
    else if( data_id == WIFI_DATA_ID_PORTS ){

        if( len != sizeof(wifi_msg_ports_t) ){

            return;
        }

        wifi_msg_ports_t *msg = (wifi_msg_ports_t *)data;
    
        wifi_v_set_ports( msg->ports );
    }
    else if( data_id == WIFI_DATA_ID_PEEK_UDP ){

        wifi_msg_udp_header_t *header = wifi_h_get_rx_udp_header();

        if( header == 0 ){

            return;
        }

        _intf_i8_transmit_msg( WIFI_DATA_ID_PEEK_UDP, (uint8_t *)header, sizeof(wifi_msg_udp_header_t) );
    }
    else if( data_id == WIFI_DATA_ID_GET_UDP ){
        
        wifi_msg_udp_header_t *header = wifi_h_get_rx_udp_header();

        if( header == 0 ){

            return;
        }

        uint8_t *data = (uint8_t *)( header + 1 );

        _intf_i8_transmit_msg( WIFI_DATA_ID_GET_UDP, data, header->len );

        wifi_v_rx_udp_clear_last();
    }
    else if( data_id == WIFI_DATA_ID_SEND_UDP ){

        wifi_msg_udp_header_t *udp_header = (wifi_msg_udp_header_t *)data;
        uint8_t *data_ptr = (uint8_t *)( udp_header + 1 );
    
        uint16_t udp_len = len - sizeof(wifi_msg_udp_header_t);

        if( udp_len == udp_header->len ){

            wifi_v_send_udp( udp_header, data_ptr );
        }
    }
    else if( data_id == WIFI_DATA_ID_SET_OPTIONS ){

        wifi_msg_set_options_t *msg = (wifi_msg_set_options_t *)data;

        opt_v_set_options( msg );
    }
    else if( data_id == WIFI_DATA_ID_SHUTDOWN ){
        
        wifi_v_shutdown();
    }
}

static uint8_t __attribute__ ((aligned (4))) data_buf[1024];

void intf_v_process( void ){

    // check if TX Q is empty and no network traffic
    if( list_b_is_empty( &tx_q ) && !wifi_b_rx_udp_pending() ){

        release_irq();
    }
    else{

        assert_irq();   
    }

    // check for data
    if( Serial.available() == 0 ){

        return;
    }

    char c = Serial.read();

    if( c == WIFI_COMM_QUERY_READY ){

        // signal serial is ready
        Serial.write( WIFI_COMM_READY );

        return;
    }
    else if( c == WIFI_COMM_GET_MSG ){

        release_irq();

        if( list_b_is_empty( &tx_q ) ){

            // no queued messages, but do we have a network packet?
            if( wifi_b_rx_udp_pending() ){

                // send a status message
                wifi_v_send_status();
            }
            else{
                
                Serial.write( WIFI_COMM_NAK );
                return;
            }
        }

        Serial.write( WIFI_COMM_ACK );

        list_node_t ln = list_ln_remove_tail( &tx_q );

        buffered_msg_t *msg = (buffered_msg_t *)list_vp_get_data( ln );
        uint8_t *data = (uint8_t *)( msg + 1 );

        _intf_i8_transmit_msg( msg->data_id, data, msg->len );

        list_v_release_node( ln );

        return;
    }
    else if( c != WIFI_COMM_DATA ){

        return;
    }

    // starting a data frame
    uint32_t comm_timeout = start_timeout();
    while( Serial.available() < (int32_t)sizeof(wifi_data_header_t) ){

        if( elapsed( comm_timeout ) > WIFI_COMM_TIMEOUT ){

            comm_errors++;
            Serial.write( WIFI_COMM_NAK );
            // Serial.write( 1 );
            return;
        }
    }

    // read header
    wifi_data_header_t header;
    Serial.readBytes( (uint8_t *)&header, sizeof(header) );

    // check CRC
    if( crc_u16_block( (uint8_t *)&header, sizeof(header) ) != 0 ){

        comm_errors++;
        Serial.write( WIFI_COMM_NAK );
        // Serial.write( 2 );
        return;
    }

    // check if we're sending a large block
    if( header.len > WIFI_MAX_SINGLE_SHOT_LEN ){

        Serial.write( WIFI_COMM_ACK );
    }

    if( header.len > 0 ){
    
        // receive data
        uint16_t count = 0;
        comm_timeout = start_timeout();
        while( count < header.len ){

            if( elapsed( comm_timeout ) > WIFI_COMM_TIMEOUT ){

                comm_errors++;
                Serial.write( WIFI_COMM_NAK );
                // Serial.write( 3 );
                return;
            }

            uint16_t available = Serial.available();

            if( available > ( header.len - count ) ){

                available = ( header.len - count );
            }

            Serial.readBytes( &data_buf[count], available );
            count += available;
        }

        // get CRC
        uint16_t crc;
        comm_timeout = start_timeout();
        while( Serial.available() < (int32_t)sizeof(crc) ){

            if( elapsed( comm_timeout ) > WIFI_COMM_TIMEOUT ){

                comm_errors++;

                Serial.write( WIFI_COMM_NAK );
                // Serial.write( 4 );
                return;
            }
        }

        Serial.readBytes( (uint8_t *)&crc, sizeof(crc) );

        if( crc_u16_block( data_buf, header.len ) != crc ){

            comm_errors++;

            Serial.write( WIFI_COMM_NAK );
            // Serial.write( 5 );
            return;
        }   
    }

    Serial.write( WIFI_COMM_ACK );

    // data is ready
    process_data( header.data_id, data_buf, header.len );
}

void intf_v_init( void ){

    pinMode( IRQ_GPIO, OUTPUT );
    pinMode( CTS_GPIO, INPUT );

    release_irq();

    pinMode( LED_GPIO, OUTPUT );
    intf_v_led_off();

    Serial.begin( 4000000 );

    Serial.setRxBufferSize( WIFI_ESP_BUF_SIZE );

    // flush serial buffers
    _intf_v_flush();

    list_v_init( &tx_q );
}


void intf_v_get_mac( uint8_t mac[6] ){

    WiFi.macAddress( mac );
}

int8_t intf_i8_rts( void ){
    
    // assert RTS
    Serial.write( WIFI_COMM_RTS );

    uint32_t timeout = start_timeout();

    while( elapsed( timeout ) < WIFI_COMM_TIMEOUT ){

        if( Serial.available() > 0 ){

            char c = Serial.read();

            if( c == WIFI_COMM_CTS ){

                return 0;
            }
        }
    }

    return -1;
}

int8_t intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    // check if tx q is full
    if( list_u8_count( &tx_q ) >= MAX_TX_Q_SIZE ){

        return -1;
    }

    if( len > WIFI_MAX_MCU_BUF ){

        return -2;
    }

    // buffer message
    list_node_t ln = list_ln_create_node2( 0, len + sizeof(buffered_msg_t), MEM_TYPE_MSG );

    if( ln < 0 ){

        return -3;
    }    

    buffered_msg_t *msg = (buffered_msg_t *)list_vp_get_data( ln );
    msg->data_id    = data_id;
    msg->len        = len;
    uint8_t *buf    = (uint8_t *)( msg + 1 );

    memcpy( buf, data, len );

    list_v_insert_head( &tx_q, ln );

    assert_irq();

    return 0;
}

static int8_t _intf_i8_transmit_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    uint8_t tries = WIFI_COMM_TRIES;

    // build header and compute CRC while we wait for CTS
    wifi_data_header_t header;
    header.data_id  = data_id;
    header.len      = len;
    header.flags    = 0;
    header.reserved = 0;
    header.header_crc = crc_u16_block( (uint8_t *)&header, sizeof(header) - sizeof(header.header_crc) );

    uint16_t data_crc = crc_u16_block( data, len );


    while( tries > 0 ){

        tries--;

        if( intf_i8_rts() < 0 ){

            continue;
        }

        Serial.write( WIFI_COMM_DATA );
        Serial.write( (uint8_t *)&header, sizeof(wifi_data_header_t) );

        if( len > 0 ){

            Serial.write( data, len );
            Serial.write( (uint8_t *)&data_crc, sizeof(data_crc) );
        }

        uint32_t timeout = start_timeout();

        while( elapsed( timeout ) < WIFI_COMM_TIMEOUT ){

            if( Serial.available() > 0 ){

                char c = Serial.read();

                if( c == WIFI_COMM_ACK ){

                    return 0;
                }
                else if( c == WIFI_COMM_NAK ){

                    break;
                }
            }
        }
    }

    return -1;
}

void intf_v_get_proc_stats( process_stats_t **stats ){

    *stats = &process_stats;
}

