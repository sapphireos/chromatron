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

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include "wifi.h"
#include "irq_line.h"

#include "comm_intf.h"
#include "comm_printf.h"

#include "options.h"

extern "C"{
    #include "crc.h"
    #include "wifi_cmd.h"
    #include "list.h"
    #include "hash.h"
}

static char hostname[32];

static char ssid[WIFI_SSID_LEN];
static char pass[WIFI_PASS_LEN];

static uint16_t ports[WIFI_MAX_PORTS];
static WiFiUDP udp[WIFI_MAX_PORTS];
static uint8_t port_rx_depth[WIFI_MAX_PORTS];

static list_t rx_q;
static list_t tx_q;

static uint8_t last_status;
static uint8_t g_wifi_status;

static uint16_t rx_udp_overruns;
static uint32_t udp_received;
static uint32_t udp_sent;
static uint16_t connects;

static bool mdns_connected;
static bool request_ap_mode;
static bool request_connect;
static bool request_disconnect;
static bool request_ports;
static bool request_shutdown;
static bool ap_mode;
static bool scanning;

static IPAddress soft_AP_IP(192,168,4,1);
static IPAddress soft_AP_gateway(192,168,4,1);
static IPAddress soft_AP_subnet(255,255,255,0);

static int32_t elapsed_millis( uint32_t start ){

    uint32_t now = millis();
    int32_t distance = (int32_t)( now - start );

    // check for rollover
    if( distance < 0 ){

        distance = ( UINT32_MAX - now ) + abs(distance);
    }

    return distance;
}


static void led_on( void ){

    // intf_v_led_on();
}

static void led_off( void ){

    // intf_v_led_off();
}


static void flush_port( WiFiUDP *udp_socket ){

    while( udp_socket->read() >= 0 );
}


uint8_t wifi_u8_get_status( void ){

    return g_wifi_status;
}

void wifi_v_set_status_bits( uint8_t bits ){

    g_wifi_status |= bits;
}

void wifi_v_clr_status_bits( uint8_t bits ){

    g_wifi_status &= ~bits;
}

IPAddress wifi_ip_get_ip( void ){

    if( ap_mode ){

        return WiFi.softAPIP();
    }
    
    return WiFi.localIP();
}

IPAddress wifi_ip_get_subnet( void ){

    if( ap_mode ){

        IPAddress subnet;
        subnet[0] = 255;
        subnet[1] = 255;
        subnet[2] = 255;
        subnet[3] = 0;

        return subnet;
    }
    
    return WiFi.subnetMask();
}


void wifi_v_init( void ){

    list_v_init( &tx_q );
    list_v_init( &rx_q );

    // disable auto connect, we rely on the main CPU for that.
    if( WiFi.getAutoConnect() == true ){

        WiFi.setAutoConnect( false );
    }

    // disable saving of wifi config to flash.
    // if this is left on, there will be a noticeable delay
    // when doing a wifi connect (blocking the CPU) while the 
    // ESP8266 saves the router information to flash.
    WiFi.persistent(false);

    // change to station only mode.
    // for some reason the default is station + AP mode.
    ap_mode = false;
    WiFi.mode( WIFI_STA );


    // set host name
    uint8_t mac[6];
    WiFi.macAddress( mac );

    char mac_str[16];
    memset( mac_str, 0, sizeof(mac_str) );
    snprintf( &mac_str[0], 3, "%02x", mac[3] );
    snprintf( &mac_str[2], 3, "%02x", mac[4] ); 
    snprintf( &mac_str[4], 3, "%02x", mac[5] );

    memset( hostname, 0, sizeof(hostname) );
    strlcpy( hostname, "Chromatron_", sizeof(hostname) );

    strlcat( hostname, mac_str, sizeof(hostname) );

    WiFi.hostname(hostname);
}

void wifi_v_send_status( void ){

    wifi_msg_status_t status_msg;
    status_msg.flags = wifi_u8_get_status();

    if( opt_b_get_high_speed() ){

        status_msg.flags |= WIFI_STATUS_160MHz;
    }

    intf_i8_send_msg( WIFI_DATA_ID_STATUS, (uint8_t *)&status_msg, sizeof(status_msg) );
}

void wifi_v_process( void ){
    
    if( ( WiFi.status() == WL_CONNECTED ) || ( WiFi.getMode() == WIFI_AP ) ){

        // check if status is changing
        if( ( last_status & WIFI_STATUS_CONNECTED ) == 0 ){

            connects++;

            wifi_v_set_status_bits( WIFI_STATUS_CONNECTED );
            wifi_v_send_status();

            intf_v_printf("Connected!");
        }

        // if in station mode:
        // NOTE:
        // MDNS will CRASH in AP mode!!!

        if( WiFi.getMode() == WIFI_STA ){

            // check if MDNS is up
            if( mdns_connected ){

                MDNS.update();
            }
            else{
                // enable MDNS
                if( MDNS.begin( hostname ) ){

                    MDNS.addService( "catbus", "udp", 44632 );
                    MDNS.addServiceTxt( "catbus", "udp", "service", "chromatron" );            

                    int8_t midi_channel = opt_i8_get_midi_channel();

                    if( midi_channel >= 0 ){

                        MDNS.addService( "_apple_midi", "_udp", 5004 );
                    }

                    mdns_connected = true;

                    intf_v_printf("MDNS connected");
                }
                else{

                    // MDNS fail - probably failed the IGMP join
                    // intf_v_printf("MDNS fail");
                }
            }        
        }
    }
    else{

        // check if status is changing
        if( ( last_status & WIFI_STATUS_CONNECTED ) != 0 ){

            wifi_v_clr_status_bits( WIFI_STATUS_CONNECTED );
            wifi_v_send_status();
        }
    }

    if( scanning ){

        int32_t networks_found = WiFi.scanComplete();

        if( networks_found >= 0 ){

            wifi_msg_scan_results_t msg;
            memset( &msg, 0, sizeof(msg) );

            String network_ssid;
            int32_t network_rssi;
            // int32_t channel;

            for( int32_t i = 0; i < networks_found; i++ ){

                network_ssid = WiFi.SSID( i );
                network_rssi = WiFi.RSSI( i );
                // channel = WiFi.channel( i );

                // intf_v_printf("%d %d %s", network_rssi, channel, network_ssid.c_str());

                msg.count++;
                msg.networks[i].ssid_hash = hash_u32_string( (char *)network_ssid.c_str() );
                msg.networks[i].rssi = network_rssi;

                if( msg.count >= WIFI_SCAN_RESULTS_LEN ){

                    break;
                }
            }
        
            if( intf_i8_send_msg( WIFI_DATA_ID_WIFI_SCAN_RESULTS, (uint8_t *)&msg, sizeof(msg) ) >= 0 ){

                scanning = false;
            }
        }
    }
    else if( request_connect ){

        WiFi.mode( WIFI_STA );
        ap_mode = false;

        WiFi.begin( ssid, pass );    
        
        request_connect = false;
        request_disconnect = false;

        wifi_v_clr_status_bits( WIFI_STATUS_AP_MODE );
    }
    else if( request_ap_mode ){

        WiFi.mode( WIFI_AP );
        ap_mode = true;

        // configure IP
        WiFi.softAPConfig(soft_AP_IP, soft_AP_gateway, soft_AP_subnet);

        // NOTE!
        // password must be at least 8 characters.
        // if it is less, the ESP will use a default SSID
        // with no password.

        WiFi.softAP( ssid, pass );

        request_ap_mode = false;
        request_connect = false;
        request_disconnect = false;

        wifi_v_set_status_bits( WIFI_STATUS_AP_MODE );
    }
    else if( request_disconnect ){

        MDNS.close();
        mdns_connected = false;

        WiFi.disconnect();

        request_connect = false;
        request_disconnect = false;

        wifi_v_clr_status_bits( WIFI_STATUS_AP_MODE );
    }
    else if( request_shutdown ){

        MDNS.close();
        mdns_connected = false;

        delay(100);

        request_shutdown = false;
        request_disconnect = true;
    }

    if( request_ports ){

        request_ports = false;

        for( uint32_t i = 0; i < WIFI_MAX_PORTS; i++ ){

            // check for port mismatch
            if( ports[i] != udp[i].localPort() ){

                port_rx_depth[i] = 0;

                if( ports[i] == 0 ){

                    udp[i].stop();
                }
                else{

                    udp[i].begin( ports[i] );
                }
            }
        }
    }


    // check UDP
    for( uint32_t i = 0; i < WIFI_MAX_PORTS; i++ ){

        // is udp socket open?
        if( udp[i].localPort() == 0 ){

            continue;
        }

        // check if data is available on this port
        int32_t packet_len = udp[i].parsePacket();

        if( packet_len <= 0 ){

            continue;
        }


        // if( rx_size >= RX_BUF_SIZE ){
        if( list_u8_count( &rx_q ) >= RX_BUF_SIZE ){

            if( rx_udp_overruns < 65535 ){

                rx_udp_overruns++;
            }

            // receive overrun!
            // flush the current received data
            flush_port( &udp[i] );

            break;
        }

        uint32_t rx_depth = port_rx_depth[i];

        // check if this port exceeds its bandwidth
        if( rx_depth >= MAX_PORT_DEPTH ){

            if( rx_udp_overruns < 65535 ){
                
                rx_udp_overruns++;
            }

            // port receive overrun!
            // flush the current received data
            flush_port( &udp[i] );

            continue;
        }

        // allocate memory
        list_node_t ln = list_ln_create_node2( 0, sizeof(wifi_msg_udp_header_t) + packet_len, MEM_TYPE_NET );

        if( ln < 0 ){

            // buffer full
            break;
        }

        list_v_insert_head( &rx_q, ln );


        // data available!
        led_on();

        IPAddress remote_ip = udp[i].remoteIP();

        wifi_msg_udp_header_t *rx_header = (wifi_msg_udp_header_t *)list_vp_get_data( ln );
        uint8_t *rx_data = (uint8_t *)( rx_header + 1 );

    
        rx_header->lport = udp[i].localPort();
        rx_header->addr.ip3 = remote_ip[0];
        rx_header->addr.ip2 = remote_ip[1];
        rx_header->addr.ip1 = remote_ip[2];
        rx_header->addr.ip0 = remote_ip[3];
        rx_header->rport = udp[i].remotePort();
        rx_header->len = (uint16_t)packet_len;
        udp[i].read( rx_data, packet_len );
        rx_header->crc = crc_u16_block( rx_data, packet_len );
    
        udp_received++;

        port_rx_depth[i]++;

        led_off();
    }

    // check transmit buffer
    if( list_u8_count( &tx_q ) > 0 ){

        list_node_t ln = list_ln_remove_tail( &tx_q );

        if( ln > 0 ){

            uint32_t *timeout = (uint32_t *)list_vp_get_data( ln );
            wifi_msg_udp_header_t *header = (wifi_msg_udp_header_t *)( timeout + 1 );

            if( elapsed_millis( *timeout ) > 1000 ){

                list_v_release_node( ln );

                intf_v_printf( "TX timeout: %u -> %u", header->lport, header->rport );

                return;                                
            }

            uint8_t *tx_data = (uint8_t *)( header + 1 );

            // find matching UDP socket
            int16_t lport_index = -1;
            for( uint32_t i = 0; i < WIFI_MAX_PORTS; i++ ){

                if( udp[i].localPort() == header->lport ){

                    lport_index = i;

                    break;
                }
            }

            if( lport_index >= 0 ){

                IPAddress raddr = IPAddress( header->addr.ip3, header->addr.ip2, header->addr.ip1, header->addr.ip0 );
                IPAddress limited_broadcast = IPAddress(255, 255, 255, 255);

                // check if we're using the limited  broadcast, if so,
                // change it to a subnet broadcast.  the reason for this is
                // that other than DHCP, LWIP won't route a limited broadcast
                // on the wifi interface.
                if( raddr == limited_broadcast ){

                    raddr = ~(uint32_t)WiFi.subnetMask() | (uint32_t)WiFi.gatewayIP();
                }


                if( udp[lport_index].beginPacket( raddr, header->rport ) != 0 ){

                    udp[lport_index].write( tx_data, header->len );

                    if( udp[lport_index].endPacket() != 0 ){

                        udp_sent++;
                    }
                }

                list_v_release_node( ln );
            }
            else{

                // port not found
                // re-enqueue message
                // it is possible that the open port command has not be received yet
                list_v_insert_head( &tx_q, ln );
            }    
        }
    }


    // update stats

    uint8_t wifi_status = wifi_u8_get_status();

    if( ( wifi_status != last_status ) ){

        wifi_v_send_status();

        last_status = wifi_status;
    }
}

void wifi_v_send_udp( wifi_msg_udp_header_t *header, uint8_t *data ){

    if( header->len > WIFI_UDP_BUF_LEN ){

        return;
    }

    // check tx buffer size
    if( list_u8_count( &tx_q ) >= TX_BUF_SIZE ){

        intf_v_printf( "TX FIFO overrun" );

        return;
    }

    list_node_t ln = list_ln_create_node2( 0, sizeof(uint32_t) + sizeof(wifi_msg_udp_header_t) + header->len, MEM_TYPE_NET );

    if( ln < 0 ){

        intf_v_printf( "TX alloc fail" );

        // buffer full
        return;
    }

    list_v_insert_head( &tx_q, ln );


    // add to buffer
    uint32_t *timeout = (uint32_t *)list_vp_get_data( ln );
    *timeout = millis();
    wifi_msg_udp_header_t *tx_header = (wifi_msg_udp_header_t *)( timeout + 1 );
    uint8_t *tx_data = (uint8_t *)( tx_header + 1 );
        
    memcpy( tx_header, header, sizeof(wifi_msg_udp_header_t) );
    memcpy( tx_data, data, header->len );
}


void wifi_v_set_ap( char *_ssid, char *_pass ){
    
    request_ap_mode = false;

    memset( ssid, 0, sizeof(ssid) );
    memset( pass, 0, sizeof(pass) );

    strncpy( ssid, _ssid, sizeof(ssid) );
    strncpy( pass, _pass, sizeof(pass) );

    // check if new SSID is empty string, if so,
    // this is a disconnect command
    if( ssid[0] == 0 ){

        request_disconnect = true;
    }
    else{

        request_connect = true;        
    }
}

void wifi_v_set_ap_mode( char *_ssid, char *_pass ){
    
    request_connect = false;

    memset( ssid, 0, sizeof(ssid) );
    memset( pass, 0, sizeof(pass) );

    strncpy( ssid, _ssid, sizeof(ssid) );
    strncpy( pass, _pass, sizeof(pass) );

    // check if new SSID is empty string, if so,
    // this is a disconnect command
    if( ssid[0] == 0 ){

        request_disconnect = true;
    }
    else{

        request_ap_mode = true;
    }
}

void wifi_v_shutdown( void ){

    request_shutdown = true;
}

void wifi_v_scan( void ){

    if( scanning ){

        return;
    }

    WiFi.scanDelete();

    scanning = true;
    WiFi.scanNetworks(true, false);
}

void wifi_v_set_ports( uint16_t _ports[WIFI_MAX_PORTS] ){

    memcpy( ports, _ports, sizeof(ports) );
    request_ports = true;
}

bool wifi_b_rx_udp_pending( void ){

    return list_u8_count( &rx_q ) > 0;
}

int8_t wifi_i8_get_rx_udp_header( wifi_msg_udp_header_t *header ){

    if( list_u8_count( &rx_q ) == 0 ){

        return -1;
    }

    list_node_t ln = rx_q.tail;
    wifi_msg_udp_header_t *rx_header = (wifi_msg_udp_header_t *)list_vp_get_data( ln );

    *header = *rx_header;

    return 0;
}

uint8_t* wifi_u8p_get_rx_udp_data( void ){

    if( list_u8_count( &rx_q ) == 0 ){

        return 0;
    }

    list_node_t ln = rx_q.tail;
    wifi_msg_udp_header_t *rx_header = (wifi_msg_udp_header_t *)list_vp_get_data( ln );
    return (uint8_t *)( rx_header + 1 );
}

void wifi_v_rx_udp_clear_last( void ){

    if( list_u8_count( &rx_q ) == 0 ){

        return;
    }

    list_node_t ln = list_ln_remove_tail( &rx_q );
    wifi_msg_udp_header_t *rx_header = (wifi_msg_udp_header_t *)list_vp_get_data( ln );

    for( uint32_t i = 0; i < WIFI_MAX_PORTS; i++ ){

        if( udp[i].localPort() == rx_header->lport ){

            port_rx_depth[i]--;

            break;
        }
    }

    list_v_release_node( ln );
}




uint32_t wifi_u32_get_rx_udp_overruns( void ){

    return rx_udp_overruns;
}

uint32_t wifi_u32_get_udp_received( void ){

    return udp_received;
}

uint32_t wifi_u32_get_udp_sent( void ){

    return udp_sent;
}
