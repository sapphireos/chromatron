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
#include "irq_line.h"
#include "version.h"
#include "options.h"
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



static bool connected;
static uint32_t comm_timeout;
// static bool request_vm_frame_sync;
// static uint8_t vm_frame_sync_index;
// static bool request_vm_frame_sync_status;
// static uint8_t vm_frame_sync_status;
static volatile bool request_reset_ready_timeout;

#ifdef USE_HSV_BRIDGE
static uint16_t hsv_index;
static bool request_hsv_array;
#else
static uint16_t rgb_index;
static bool request_rgb_pix0;
static bool request_rgb_array;
#endif

static uint16_t comm_errors;

static process_stats_t process_stats;

static volatile uint32_t last_rx_ready_ts;
static volatile bool is_rx_ready;
static uint32_t last_status_ts;

static wifi_data_header_t intf_data_header;
static uint8_t  __attribute__ ((aligned (4))) intf_comm_buf[WIFI_BUF_LEN];
static uint8_t intf_comm_state;

static wifi_msg_udp_header_t udp_header;

static wifi_msg_udp_header_t rx_udp_header;
static uint16_t rx_udp_index;
static bool udp_busy;
static uint32_t udp_timeout;

static list_t tx_q;



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

static bool rx_ready( void ){

    noInterrupts();
    bool temp = is_rx_ready;
    interrupts();

    return temp;
}

static void clear_ready_flag( void ){

    // clear ready flag
    noInterrupts();
    is_rx_ready = false;

    // make sure to reset the ready timeout!
    request_reset_ready_timeout = true;
    interrupts();
}

static int8_t _intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( len > WIFI_MAIN_MAX_DATA_LEN ){

        comm_errors++;

        return -1;
    }
    else if( !rx_ready() ){

        comm_errors++;

        return -2;  
    }

    wifi_data_header_t header;
    header.len      = len;
    header.data_id  = data_id;
    header.reserved = 0;
    header.crc      = 0;

    uint16_t crc = crc_u16_start();
    crc = crc_u16_partial_block( crc, (uint8_t *)&header, sizeof(header) );

    crc = crc_u16_partial_block( crc, data, len );

    header.crc = crc_u16_finish( crc );

    clear_ready_flag();    

    Serial.write( WIFI_COMM_DATA );
    Serial.write( (uint8_t *)&header, sizeof(header) );
    Serial.write( data, len );

    return 0;
}

static void _send_info_msg( void ){

    wifi_msg_info_t info_msg;
    info_msg.version =  ( ( VERSION_MAJOR & 0x0f ) << 12 ) |
                        ( ( VERSION_MINOR & 0x1f ) << 7 ) |
                        ( ( VERSION_PATCH & 0x7f ) << 0 );

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

    intf_i8_send_msg( WIFI_DATA_ID_INFO, (uint8_t *)&info_msg, sizeof(info_msg) );    
}

static void process_data( uint8_t data_id, uint8_t *data, uint16_t len ){

    if( data_id == WIFI_DATA_ID_CONNECT ){

        if( len != sizeof(wifi_msg_connect_t) ){

            return;
        }

        wifi_msg_connect_t *msg = (wifi_msg_connect_t *)data;

        wifi_v_set_ap( msg->ssid, msg->pass );
    }
    else if( data_id == WIFI_DATA_ID_AP_MODE ){

        if( len != sizeof(wifi_msg_ap_connect_t) ){

            return;
        }

        wifi_msg_ap_connect_t *msg = (wifi_msg_ap_connect_t *)data;

        wifi_v_set_ap_mode( msg->ssid, msg->pass );
    }
    else if( data_id == WIFI_DATA_ID_WIFI_SCAN ){

        wifi_v_scan();
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
    else if( data_id == WIFI_DATA_ID_INIT_VM ){

        uint32_t *vm_id = (uint32_t *)data;

        for( uint32_t i = 0; i < 31; i++ ){

            if( ( *vm_id & ( 1 << i ) ) != 0 ){

                vm_i8_start( i );
            }
        }
    }
    else if( data_id == WIFI_DATA_ID_VM_FRAME_SYNC ){

        wifi_msg_vm_frame_sync_t *msg = (wifi_msg_vm_frame_sync_t *)data;

        vm_v_start_frame_sync( 0, msg, len );
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DATA ){

        wifi_msg_vm_sync_data_t *msg = (wifi_msg_vm_sync_data_t *)data;

        vm_v_frame_sync_data( 0, msg, len );
    }
    else if( data_id == WIFI_DATA_ID_VM_SYNC_DONE ){

        wifi_msg_vm_sync_done_t *msg = (wifi_msg_vm_sync_done_t *)data;

        vm_v_frame_sync_done( 0, msg, len );
    }
    else if( data_id == WIFI_DATA_ID_REQUEST_FRAME_SYNC ){
        
        uint32_t *vm_id = (uint32_t *)data;

        vm_v_request_frame_data( *vm_id );
    }
    else if( data_id == WIFI_DATA_ID_RUN_VM ){

        vm_v_run_vm();
    }
    else if( data_id == WIFI_DATA_ID_RUN_FADER ){

        vm_v_run_faders();
    }
    else if( data_id == WIFI_DATA_ID_KV_DATA ){

        wifi_msg_kv_data_t *msg = (wifi_msg_kv_data_t *)data;
        data = (uint8_t *)( msg + 1 );
        len -= sizeof(wifi_msg_kv_data_t);

        if( kvdb_i8_set( msg->meta.hash, msg->meta.type, data, len ) < 0 ){

            kvdb_i8_add( msg->meta.hash, msg->meta.type, msg->meta.count + 1, data, len );
        }

        kvdb_v_set_tag( msg->meta.hash, msg->tag );
    }
    else if( data_id == WIFI_DATA_ID_UDP_EXT ){

        wifi_msg_udp_header_t *msg = (wifi_msg_udp_header_t *)data;
        uint8_t *data_ptr = (uint8_t *)( msg + 1 );
        
        memcpy( &udp_header, msg, sizeof(udp_header) );

        uint16_t udp_len = len - sizeof(udp_header);

        if( udp_len == udp_header.len ){

            wifi_v_send_udp( &udp_header, data_ptr );
        }
    }
    else if( data_id == WIFI_DATA_ID_UDP_BUF_READY ){

        udp_busy = false;
    }
    else if( data_id == WIFI_DATA_ID_VM_RUN_FUNC ){

        wifi_msg_vm_run_func_t *msg = (wifi_msg_vm_run_func_t *)data;

        vm_v_run_func( msg->vm_id, msg->func_addr );
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
}

static void set_rx_ready( void ){

    // flush serial buffers
    _intf_v_flush();

    irqline_v_strobe_irq();
}

void intf_v_process( void ){

    noInterrupts();
    if( request_reset_ready_timeout ){

        request_reset_ready_timeout = false;

        last_rx_ready_ts = start_timeout();
    }   
    interrupts();


    // check if udp busy timeout 
    if( elapsed( udp_timeout ) > 100000 ){

        if( udp_busy ){

            intf_v_printf( "UDP busy timeout" );

            udp_timeout = start_timeout();
            udp_busy = false;
        }   
    }


    if( ( intf_comm_state != COMM_STATE_IDLE ) &&
        ( elapsed( comm_timeout ) > 20000 ) ){


        comm_errors++;

        // reset comm state
        intf_comm_state = COMM_STATE_IDLE;

        set_rx_ready();
    }

    if( intf_comm_state == COMM_STATE_IDLE ){    
        
        char c = Serial.read();

        if( c == WIFI_COMM_RESET ){

            if( connected == false ){
                
                connected = true;

                irqline_v_enable();
            }

            set_rx_ready();
        }
        else if( c == WIFI_COMM_DATA ){

            intf_comm_state = COMM_STATE_RX_HEADER;

            comm_timeout = start_timeout();
        }
    }
    else if( intf_comm_state == COMM_STATE_RX_HEADER ){    
        
        if( Serial.available() >= (int32_t)(sizeof(wifi_data_header_t) ) ){

            Serial.readBytes( (uint8_t *)&intf_data_header, sizeof(intf_data_header) );

            intf_comm_state = COMM_STATE_RX_DATA;
        }   
    }    
    else if( intf_comm_state == COMM_STATE_RX_DATA ){    

        if( Serial.available() >= intf_data_header.len ){

            Serial.readBytes( intf_comm_buf, intf_data_header.len );

            // we've copied the data out of the FIFO, so we can go ahead and send RX ready
            // before we process the data.
            set_rx_ready();

            // check crc

            uint16_t msg_crc = intf_data_header.crc;
            intf_data_header.crc = 0;
            uint16_t crc = crc_u16_start();
            crc = crc_u16_partial_block( crc, (uint8_t *)&intf_data_header, sizeof(intf_data_header) );
            crc = crc_u16_partial_block( crc, intf_comm_buf, intf_data_header.len );
            crc = crc_u16_finish( crc );

            if( crc == msg_crc ){

                process_data( intf_data_header.data_id, intf_comm_buf, intf_data_header.len );
            }
            else{

                comm_errors++;
            }

            intf_comm_state = COMM_STATE_IDLE;
        }
    }



    if( !rx_ready() ){

        // check timeout
        noInterrupts();
        uint32_t temp_last_rx_ready_ts = last_rx_ready_ts;
        interrupts();

        uint32_t elapsed_time = elapsed( temp_last_rx_ready_ts );

        if( elapsed_time > 50000 ){

            // query for ready status
            Serial.write( WIFI_COMM_QUERY_READY );

            noInterrupts();
            request_reset_ready_timeout = true;
            interrupts();
        }

        goto done;
    }

    #ifndef USE_HSV_BRIDGE
    if( request_rgb_pix0 ){

        request_rgb_pix0 = false;

        wifi_msg_rgb_pix0_t msg;
        msg.r = gfx_u16_get_pix0_red();
        msg.g = gfx_u16_get_pix0_green();
        msg.b = gfx_u16_get_pix0_blue();

        _intf_i8_send_msg( WIFI_DATA_ID_RGB_PIX0, (uint8_t *)&msg, sizeof(msg) );
    }
    else if( request_rgb_array ){

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

        _intf_i8_send_msg( WIFI_DATA_ID_RGB_ARRAY, 
                           (uint8_t *)&msg, 
                           sizeof(msg.index) + sizeof(msg.count) + ( count * 4 ) );

        rgb_index += count;

        if( rgb_index >= pix_count ){

            rgb_index = 0;
            request_rgb_array = false;
        }
    }
    #else
    if( request_hsv_array ){

        // // get pointers to the arrays
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

        _intf_i8_send_msg( WIFI_DATA_ID_HSV_ARRAY, 
                           (uint8_t *)&msg, 
                           sizeof(msg.index) + 
                           sizeof(msg.count) + 
                           sizeof(msg.padding) + 
                           ( count * 6 ) );

        hsv_index += count;

        if( hsv_index >= pix_count ){

            hsv_index = 0;
            request_hsv_array = false;
        }
    }
    #endif
    // else if( request_vm_frame_sync ){

    //     wifi_msg_vm_frame_sync_t msg;
    //     memset( &msg, 0, sizeof(msg) );

    //     if( vm_i8_get_frame_sync( vm_frame_sync_index, &msg ) == 0 ){

    //         _intf_i8_send_msg( WIFI_DATA_ID_VM_FRAME_SYNC, (uint8_t *)&msg, sizeof(msg) );

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

    //     _intf_i8_send_msg( WIFI_DATA_ID_FRAME_SYNC_STATUS, (uint8_t *)&msg, sizeof(msg) );        
    // }
    else if( wifi_b_rx_udp_pending() ){

        if( rx_udp_header.lport == 0 ){

            // check if UDP busy
            if( udp_busy ){

                goto done;
            }

            udp_busy = true;
            udp_timeout = start_timeout();

            rx_udp_index = 0;

            // get header
            wifi_i8_get_rx_udp_header( &rx_udp_header );

            uint16_t data_len = WIFI_MAIN_MAX_DATA_LEN - sizeof(rx_udp_header);
            if( data_len > rx_udp_header.len ){

                data_len = rx_udp_header.len;
            }
            
            wifi_data_header_t header;
            header.len      = data_len + sizeof(rx_udp_header);
            header.data_id  = WIFI_DATA_ID_UDP_HEADER;
            header.reserved = 0;
            header.crc      = 0;

            uint8_t *data = wifi_u8p_get_rx_udp_data();

            uint16_t crc = crc_u16_start();
            crc = crc_u16_partial_block( crc, (uint8_t *)&header, sizeof(header) );
            crc = crc_u16_partial_block( crc, (uint8_t *)&rx_udp_header, sizeof(rx_udp_header) );
            crc = crc_u16_partial_block( crc, &data[rx_udp_index], data_len );
            header.crc = crc_u16_finish( crc );
            
            clear_ready_flag();

            Serial.write( WIFI_COMM_DATA );
            Serial.write( (uint8_t *)&header, sizeof(header) );
            Serial.write( (uint8_t *)&rx_udp_header, sizeof(rx_udp_header) );
            Serial.write( &data[rx_udp_index], data_len );

            rx_udp_index += data_len;
        }
        else{

            uint16_t data_len = rx_udp_header.len - rx_udp_index;

            if( data_len > WIFI_MAIN_MAX_DATA_LEN ){

                data_len = WIFI_MAIN_MAX_DATA_LEN;
            }

            uint8_t *data = wifi_u8p_get_rx_udp_data();

            _intf_i8_send_msg( WIFI_DATA_ID_UDP_DATA, &data[rx_udp_index], data_len );

            rx_udp_index += data_len;

            // check if we've sent all data
            if( rx_udp_index >= rx_udp_header.len ){

                rx_udp_index = 0;
                rx_udp_header.lport = 0;
                wifi_v_rx_udp_clear_last();
            }
        }
    }
    else if( list_u8_count( &tx_q ) > 0 ){

        list_node_t ln = list_ln_remove_tail( &tx_q );

        wifi_data_header_t *header = (wifi_data_header_t *)list_vp_get_data( ln );
        uint8_t *data = (uint8_t *)( header + 1 );

        clear_ready_flag();    

        Serial.write( WIFI_COMM_DATA );
        Serial.write( (uint8_t *)header, sizeof(wifi_data_header_t) );
        Serial.write( data, header->len );

        list_v_release_node( ln );
    }


    if( elapsed( last_status_ts ) > 1000000 ){

        last_status_ts = start_timeout();

        wifi_v_send_status();
        _send_info_msg();
        vm_v_send_info();
    }


done:
    return;
}

void ICACHE_RAM_ATTR buf_ready_irq( void ){

    is_rx_ready = true;
    request_reset_ready_timeout = true;
}

#include "uart.h"

void intf_v_init( void ){

    noInterrupts();
    is_rx_ready = false;
    interrupts();

    pinMode( BUF_READY_GPIO, INPUT );
    attachInterrupt( digitalPinToInterrupt( BUF_READY_GPIO ), buf_ready_irq, FALLING );

    pinMode( LED_GPIO, OUTPUT );
    intf_v_led_off();

    Serial.begin( 4000000 );

    Serial.setRxBufferSize( WIFI_BUF_LEN );

    // flush serial buffers
    _intf_v_flush();

    list_v_init( &tx_q );

    if( ( (uint32_t)intf_comm_buf & 0x03 ) != 0 ){

        intf_v_printf("intf_comm_buf misalign");
    }
}



#ifndef USE_HSV_BRIDGE
void intf_v_request_rgb_pix0( void ){

    request_rgb_pix0 = true;
}

void intf_v_request_rgb_array( void ){

    request_rgb_array = true;
}

#else
void intf_v_request_hsv_array( void ){

    request_hsv_array = true;
}
#endif

void intf_v_request_vm_frame_sync( void ){

    // vm_frame_sync_index = 0;
    // request_vm_frame_sync = true;
}

void intf_v_get_mac( uint8_t mac[6] ){

    WiFi.macAddress( mac );
}

int8_t intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len ){

    // check if rx is ready
    if( rx_ready() ){

        return _intf_i8_send_msg( data_id, data, len );
    }   

    // check if tx q is full
    if( list_u8_count( &tx_q) >= MAX_TX_Q_SIZE ){

        return -1;
    }

    if( len > WIFI_MAIN_MAX_DATA_LEN ){

        return -2;
    }

    // buffer message
    list_node_t ln = list_ln_create_node2( 0, len + sizeof(wifi_data_header_t), MEM_TYPE_MSG );

    if( ln < 0 ){

        return -3;
    }    

    wifi_data_header_t *header = (wifi_data_header_t *)list_vp_get_data( ln );

    header->len      = len;
    header->data_id  = data_id;
    header->reserved = 0;
    header->crc      = 0;

    uint16_t crc = crc_u16_start();
    crc = crc_u16_partial_block( crc, (uint8_t *)header, sizeof(wifi_data_header_t) );
    crc = crc_u16_partial_block( crc, data, len );

    header->crc = crc_u16_finish( crc );

    memcpy( ( header + 1 ), data, len );

    list_v_insert_head( &tx_q, ln );

    return 0;
}

void intf_v_get_proc_stats( process_stats_t **stats ){

    *stats = &process_stats;
}

