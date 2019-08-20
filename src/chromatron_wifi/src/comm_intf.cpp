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

#include "Arduino.h"
#include "comm_intf.h"
#include "comm_printf.h"
#include "wifi.h"
#include "version.h"
#include "options.h"
#include "uart.h"
#include <ESP8266WiFi.h>

extern "C"{
    #include "gfx_lib.h"
    #include "vm_runner.h"
    #include "crc.h"
    #include "wifi_cmd.h"
    #include "vm_wifi_cmd.h"
    #include "kvdb.h"
    #include "vm_core.h"
    #include "list.h"
    #include "memory.h"
    #include "catbus_packer.h"
    #include "comm_printf.h"
}

typedef struct{
    uint8_t data_id;
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

    info_msg.rx_udp_overruns        = wifi_u32_get_rx_udp_overruns();
    info_msg.udp_received           = wifi_u32_get_udp_received();
    info_msg.udp_sent               = wifi_u32_get_udp_sent();

    mem_rt_data_t rt_data;
    mem2_v_get_rt_data( &rt_data );

    info_msg.comm_errors            = comm_errors;

    info_msg.mem_heap_peak          = rt_data.peak_usage;
    info_msg.mem_used               = rt_data.used_space;

    info_msg.intf_max_time          = process_stats.intf_max_time;
    info_msg.vm_max_time            = process_stats.vm_max_time;
    info_msg.wifi_max_time          = process_stats.wifi_max_time;
    info_msg.mem_max_time           = process_stats.mem_max_time;

    info_msg.intf_avg_time          = process_stats.intf_avg_time;
    info_msg.vm_avg_time            = process_stats.vm_avg_time;
    info_msg.wifi_avg_time          = process_stats.wifi_avg_time;
    info_msg.mem_avg_time           = process_stats.mem_avg_time;

    info_msg.wifi_router            = wifi_i8_get_router();

    _intf_i8_transmit_msg( WIFI_DATA_ID_INFO, (uint8_t *)&info_msg, sizeof(info_msg) );    
}

static void process_data( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( data_id == WIFI_DATA_ID_INFO ){

        _send_info_msg();        
    }
    else if( data_id == WIFI_DATA_ID_CONNECT ){

        if( len != sizeof(wifi_msg_connect_t) ){

            return;
        }

        wifi_msg_connect_t *msg = (wifi_msg_connect_t *)data;

        for( uint32_t i = 0; i < WIFI_MAX_APS; i++ ){

            wifi_v_set_ap_info( msg->ssid[i], msg->pass[i], i );    
        }

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
    else if( data_id == WIFI_DATA_ID_GFX_PARAMS ){

        if( len != sizeof(gfx_params_t) ){

            return;
        }

        gfx_params_t *msg = (gfx_params_t *)data;
    
        gfx_v_set_params( msg );
    }
    else if( data_id == WIFI_DATA_ID_RESET_VM ){

        wifi_msg_reset_vm_t *msg = (wifi_msg_reset_vm_t *)data;

        vm_v_reset( msg->vm_id );
    }
    else if( data_id == WIFI_DATA_ID_LOAD_VM ){

        wifi_msg_load_vm_t *msg = (wifi_msg_load_vm_t *)data;

        uint16_t chunk_len = len - ( sizeof(msg->vm_id) + sizeof(msg->total_size) + sizeof(msg->offset) );

        vm_i8_load( msg->chunk, chunk_len, msg->total_size, msg->offset, msg->vm_id );
    }
    else if( ( data_id == WIFI_DATA_ID_INIT_VM ) ||
             ( data_id == WIFI_DATA_ID_RUN_VM ) ||
             ( data_id == WIFI_DATA_ID_VM_RUN_FUNC ) ){

        wifi_msg_vm_run_t *msg = (wifi_msg_vm_run_t *)data;


        wifi_msg_vm_info_t reply;
        reply.vm_id             = msg->vm_id;
        reply.padding           = 0;

        _intf_i8_transmit_msg( WIFI_DATA_ID_VM_INFO, (uint8_t *)&reply, sizeof(reply) );
    }
    // else if( data_id == WIFI_DATA_ID_INIT_VM ){

    //     wifi_msg_run_vm_t *msg = (wifi_msg_run_vm_t *)data;

    //     wifi_msg_run_vm_status_t status;
    //     status.vm_id = msg->vm_id;

    //     status.status = vm_i8_start( msg->vm_id );

    //     _intf_i8_transmit_msg( WIFI_DATA_ID_INIT_VM, (uint8_t *)&status, sizeof(status) );
    // }
    // else if( data_id == WIFI_DATA_ID_RUN_VM ){

    //     wifi_msg_run_vm_t *msg = (wifi_msg_run_vm_t *)data;

    //     wifi_msg_run_vm_status_t status;
    //     status.vm_id = msg->vm_id;

    //     status.status = vm_i8_run_vm( msg->vm_id );

    //     _intf_i8_transmit_msg( WIFI_DATA_ID_RUN_VM, (uint8_t *)&status, sizeof(status) );
    // }
    // else if( data_id == WIFI_DATA_ID_VM_RUN_FUNC ){

    //     wifi_msg_vm_run_func_t *msg = (wifi_msg_vm_run_func_t *)data;

    //     wifi_msg_run_vm_status_t status;
    //     status.vm_id = msg->vm_id;

    //     status.status = vm_i8_run_func( msg->vm_id, msg->func_addr );

    //     _intf_i8_transmit_msg( WIFI_DATA_ID_VM_RUN_FUNC, (uint8_t *)&status, sizeof(status) );
    // }
    else if( data_id == WIFI_DATA_ID_RUN_FADER ){

        vm_v_run_faders();

        _intf_i8_transmit_msg( WIFI_DATA_ID_RUN_FADER, 0, 0 );
    }
    #ifdef USE_HSV_BRIDGE
    else if( data_id == WIFI_DATA_ID_HSV_ARRAY ){

        uint8_t page = *data;
        uint16_t hsv_index = page * WIFI_HSV_DATA_N_PIXELS;

        // get pointers to the arrays
        uint16_t *h = gfx_u16p_get_hue();
        uint16_t *s = gfx_u16p_get_sat();
        uint16_t *v = gfx_u16p_get_val();

        uint16_t pix_count = gfx_u16_get_pix_count();

        wifi_msg_hsv_array_t msg;

        uint16_t remaining = pix_count - hsv_index;
        uint8_t count = WIFI_HSV_DATA_N_PIXELS;

        if( count > remaining ){

            count = remaining;
        }

        msg.index = hsv_index;
        msg.count = count;
        
        uint8_t transfer_bytes = count * 2;

        uint8_t *ptr = msg.hsv_array;
        memcpy( ptr, h + hsv_index, transfer_bytes );
        ptr += transfer_bytes;
        memcpy( ptr, s + hsv_index, transfer_bytes );
        ptr += transfer_bytes;

        uint16_t *val = (uint16_t *)ptr;
        v += hsv_index;
        for( uint32_t i = 0; i < count; i++ ){

            *val = gfx_u16_get_dimmed_val( *v );
            v++;
            val++;
        }

        _intf_i8_transmit_msg( WIFI_DATA_ID_HSV_ARRAY, 
                           (uint8_t *)&msg, 
                           sizeof(msg.index) + 
                           sizeof(msg.count) + 
                           sizeof(msg.padding) + 
                           ( count * 6 ) );
    }
    #else
    else if( data_id == WIFI_DATA_ID_RGB_PIX0 ){

        wifi_msg_rgb_pix0_t msg;
        msg.r = gfx_u16_get_pix0_red();
        msg.g = gfx_u16_get_pix0_green();
        msg.b = gfx_u16_get_pix0_blue();

        _intf_i8_transmit_msg( WIFI_DATA_ID_RGB_PIX0, (uint8_t *)&msg, sizeof(msg) );
    }
    else if( data_id == WIFI_DATA_ID_RGB_ARRAY ){

        uint8_t page = *data;
        uint16_t rgb_index = page * WIFI_RGB_DATA_N_PIXELS;

        // get pointers to the arrays
        uint8_t *r = gfx_u8p_get_red();
        uint8_t *g = gfx_u8p_get_green();
        uint8_t *b = gfx_u8p_get_blue();
        uint8_t *d = gfx_u8p_get_dither();

        uint16_t pix_count = gfx_u16_get_pix_count();

        wifi_msg_rgb_array_t msg;

        uint16_t remaining = pix_count - rgb_index;
        uint8_t count = WIFI_RGB_DATA_N_PIXELS;

        if( count > remaining ){

            count = remaining;
        }

        msg.index = rgb_index;
        msg.count = count;
        uint8_t *ptr = msg.rgbd_array;
        memcpy( ptr, r + rgb_index, count );
        ptr += count;
        memcpy( ptr, g + rgb_index, count );
        ptr += count;
        memcpy( ptr, b + rgb_index, count );
        ptr += count;
        memcpy( ptr, d + rgb_index, count );

        _intf_i8_transmit_msg( WIFI_DATA_ID_RGB_ARRAY, 
                           (uint8_t *)&msg, 
                           sizeof(msg.index) + sizeof(msg.count) + ( count * 4 ) );
    }
    #endif
    else if( data_id == WIFI_DATA_ID_KV_DATA ){

        wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)data;
        data = (uint8_t *)( msg + 1 );
        len -= sizeof(wifi_msg_kv_data_t);

        if( kvdb_i8_set( msg->meta.hash, msg->meta.type, data, len ) < 0 ){

            kvdb_i8_add( msg->meta.hash, msg->meta.type, msg->meta.count + 1, data, len );
        }

        kvdb_v_set_tag( msg->meta.hash, msg->tag );
    }
    else if( data_id == WIFI_DATA_ID_GET_KV_DATA ){

        uint32_t *hash = (uint32_t *)data;

        uint8_t buf[CATBUS_MAX_DATA + sizeof(wifi_msg_kv_data_t)];
        wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)buf;
        uint8_t *data = (uint8_t *)( msg + 1 );

        if( kvdb_i8_get_meta( *hash, &msg->meta ) < 0 ){

            return;
        }

        uint16_t data_len = type_u16_size( msg->meta.type ) * ( (uint16_t)msg->meta.count + 1 );

        kvdb_i8_get( *hash, msg->meta.type, data, CATBUS_MAX_DATA );        

        _intf_i8_transmit_msg( WIFI_DATA_ID_GET_KV_DATA, buf, data_len + sizeof(wifi_msg_kv_data_t) );
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

        opt_v_set_low_power( msg->low_power );
        opt_v_set_led_quiet( msg->led_quiet );
        opt_v_set_midi_channel( msg->midi_channel );
    }
    else if( data_id == WIFI_DATA_ID_SHUTDOWN ){
        
        for( uint32_t i = 0; i < 32; i++ ){

            vm_v_reset( i );
        }

        wifi_v_shutdown();
    }
    // else if( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ){

    //     wifi_msg_vm_frame_sync_t *msg = (wifi_msg_vm_frame_sync_t *)data;

    //     vm_v_start_frame_sync( 0, msg, len );
    // }
    // else if( data_id == WIFI_DATA_ID_VM_SYNC_DATA ){

    //     wifi_msg_vm_sync_data_t *msg = (wifi_msg_vm_sync_data_t *)data;

    //     vm_v_frame_sync_data( 0, msg, len );
    // }
    // else if( data_id == WIFI_DATA_ID_VM_SYNC_DONE ){

    //     wifi_msg_vm_sync_done_t *msg = (wifi_msg_vm_sync_done_t *)data;

    //     vm_v_frame_sync_done( 0, msg, len );
    // }
    // else if( data_id == WIFI_DATA_ID_REQUEST_FRAME_SYNC ){
        
    //     uint32_t *vm_id = (uint32_t *)data;

    //     vm_v_request_frame_data( *vm_id );
    // }
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

                Serial.write( WIFI_COMM_NAK );
                // Serial.write( 4 );
                return;
            }
        }

        Serial.readBytes( (uint8_t *)&crc, sizeof(crc) );

        if( crc_u16_block( data_buf, header.len ) != crc ){

            Serial.write( WIFI_COMM_NAK );
            // Serial.write( 5 );
            return;
        }   
    }

    Serial.write( WIFI_COMM_ACK );

    // data is ready
    process_data( header.data_id, data_buf, header.len );
    
    // else if( request_vm_frame_sync ){

    //     wifi_msg_vm_frame_sync_t msg;
    //     memset( &msg, 0, sizeof(msg) );

    //     if( vm_i8_get_frame_sync( vm_frame_sync_index, &msg ) == 0 ){

    //         _intf_i8_transmit_msg( WIFI_DATA_ID_VM_FRAME_SYNC, (uint8_t *)&msg, sizeof(msg) );

    //         vm_frame_sync_index++;
    //     }
    //     else{

    //         request_vm_frame_sync = false;
    //     }
    // }
    // else if( request_vm_frame_sync_status ){

    //     request_vm_frame_sync_status = false;

    //     wifi_msg_vm_frame_sync_status_t msg;
    //     msg.status = vm_frame_sync_status;
    //     msg.frame_number = vm_u16_get_frame_number();

    //     _intf_i8_transmit_msg( WIFI_DATA_ID_FRAME_SYNC_STATUS, (uint8_t *)&msg, sizeof(msg) );        
    // }
    // else if( wifi_b_rx_udp_pending() ){

    //     if( rx_udp_header.lport == 0 ){

    //         // check if UDP busy
    //         if( udp_busy ){

    //             goto done;
    //         }

    //         udp_busy = true;
    //         udp_timeout = start_timeout();

    //         rx_udp_index = 0;

    //         // get header
    //         wifi_i8_get_rx_udp_header( &rx_udp_header );

    //         uint16_t data_len = WIFI_MAIN_MAX_DATA_LEN - sizeof(rx_udp_header);
    //         if( data_len > rx_udp_header.len ){

    //             data_len = rx_udp_header.len;
    //         }
            
    //         wifi_data_header_t header;
    //         header.len      = data_len + sizeof(rx_udp_header);
    //         header.data_id  = WIFI_DATA_ID_UDP_HEADER;
    //         header.reserved = 0;
    //         header.crc      = 0;

    //         uint8_t *data = wifi_u8p_get_rx_udp_data();

    //         uint16_t crc = crc_u16_start();
    //         crc = crc_u16_partial_block( crc, (uint8_t *)&header, sizeof(header) );
    //         crc = crc_u16_partial_block( crc, (uint8_t *)&rx_udp_header, sizeof(rx_udp_header) );
    //         crc = crc_u16_partial_block( crc, &data[rx_udp_index], data_len );
    //         header.crc = crc_u16_finish( crc );
            
    //         clear_ready_flag();

    //         Serial.write( WIFI_COMM_DATA );
    //         Serial.write( (uint8_t *)&header, sizeof(header) );
    //         Serial.write( (uint8_t *)&rx_udp_header, sizeof(rx_udp_header) );
    //         Serial.write( &data[rx_udp_index], data_len );

    //         rx_udp_index += data_len;
    //     }
    //     else{

    //         uint16_t data_len = rx_udp_header.len - rx_udp_index;

    //         if( data_len > WIFI_MAIN_MAX_DATA_LEN ){

    //             data_len = WIFI_MAIN_MAX_DATA_LEN;
    //         }

    //         uint8_t *data = wifi_u8p_get_rx_udp_data();

    //         _intf_i8_transmit_msg( WIFI_DATA_ID_UDP_DATA, &data[rx_udp_index], data_len );

    //         rx_udp_index += data_len;

    //         // check if we've sent all data
    //         if( rx_udp_index >= rx_udp_header.len ){

    //             rx_udp_index = 0;
    //             rx_udp_header.lport = 0;
    //             wifi_v_rx_udp_clear_last();
    //         }
    //     }
    // }
    // else if( list_u8_count( &tx_q ) > 0 ){

    //     list_node_t ln = list_ln_remove_tail( &tx_q );

    //     wifi_data_header_t *header = (wifi_data_header_t *)list_vp_get_data( ln );
    //     uint8_t *data = (uint8_t *)( header + 1 );

    //     clear_ready_flag();    

    //     Serial.write( WIFI_COMM_DATA );
    //     Serial.write( (uint8_t *)header, sizeof(wifi_data_header_t) );
    //     Serial.write( data, header->len );

    //     list_v_release_node( ln );
    // }


    // if( elapsed( last_status_ts ) > 1000000 ){

    //     last_status_ts = start_timeout();

    //     wifi_v_send_status();
    //     _send_info_msg();
    //     vm_v_send_info();
    // }
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

