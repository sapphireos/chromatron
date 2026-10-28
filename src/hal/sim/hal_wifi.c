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

#include "cpu.h"
#include "system.h"
#include "wifi.h"
#include "netmsg.h"
#include "memory.h"
#include "ip.h"
#include "threading.h"
#include "timers.h"
#include "sockets.h"
#include "cnt_of_array.h"
#include "logging.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*

Sim network interface.

There is no physical radio: the host network stack is the radio.
UDP netmsgs are transported over host UDP sockets, and the routing
entry (route_sim, in netmsg.c) hooks the netmsg layer into these
functions.

*/

#define SIM_WIFI_MAX_PORTS      16
#define SIM_RX_BUF_LEN          1024
#define SIM_RX_IDLE_WAIT_MS     10

typedef struct {
    uint16_t lport;
    int sock;
} sim_conn_t;

static sim_conn_t sim_conn[SIM_WIFI_MAX_PORTS];

static sim_conn_t *get_conn( uint16_t lport ){

    for( uint8_t i = 0; i < cnt_of_array(sim_conn); i++ ){

        if( sim_conn[i].lport == lport ){

            return &sim_conn[i];
        }
    }

    return 0;
}

PT_THREAD( sim_wifi_rx_thread( pt_t *pt, void *state ) );

int8_t sim_wifi_i8_get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask );
void sim_wifi_v_open_close_port( uint8_t protocol, uint16_t port, bool open );

// sim network route: host UDP sockets.
// The linker script keeps and orders the .routing sections (same
// scheme as the firmware builds), so this entry is visited by the
// netmsg table walk even though it lives in a separate object file.
ROUTING_TABLE routing_table_entry_t route_sim = {
    sim_wifi_i8_get_route,
    wifi_i8_send_udp,
    sim_wifi_v_open_close_port,
    ROUTE_FLAGS_ALLOW_GLOBAL_BROADCAST
};

void hal_wifi_v_init( void ){

    memset( sim_conn, 0, sizeof(sim_conn) );

    thread_t_create( sim_wifi_rx_thread, PSTR("sim_wifi_rx"), 0, 0 );
}

// poll open sockets and feed received datagrams into the netmsg layer
PT_THREAD( sim_wifi_rx_thread( pt_t *pt, void *state ) ){

PT_BEGIN( pt );

    while( 1 ){

        bool got_data = FALSE;

        for( uint8_t i = 0; i < cnt_of_array(sim_conn); i++ ){

            if( sim_conn[i].lport == 0 ){

                continue;
            }

            uint8_t buf[SIM_RX_BUF_LEN];
            struct sockaddr_in sourceAddr;
            socklen_t socklen = sizeof(sourceAddr);

            int s = recvfrom( sim_conn[i].sock,
                              buf, sizeof(buf), MSG_DONTWAIT,
                              (struct sockaddr *)&sourceAddr, &socklen );

            if( s <= 0 ){

                continue;
            }

            got_data = TRUE;

            netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

            if( rx_netmsg < 0 ){

                continue;
            }

            netmsg_state_t *nm_state = netmsg_vp_get_state( rx_netmsg );

            // set up addressing info
            nm_state->laddr.port       = sim_conn[i].lport;
            nm_state->raddr.port       = ntohs(sourceAddr.sin_port);
            nm_state->raddr.ipaddr.ip3 = sourceAddr.sin_addr.s_addr >> 0;
            nm_state->raddr.ipaddr.ip2 = sourceAddr.sin_addr.s_addr >> 8;
            nm_state->raddr.ipaddr.ip1 = sourceAddr.sin_addr.s_addr >> 16;
            nm_state->raddr.ipaddr.ip0 = sourceAddr.sin_addr.s_addr >> 24;

            // allocate data buffer
            nm_state->data_handle = mem2_h_alloc2( s, MEM_TYPE_SOCKET_BUFFER );

            if( nm_state->data_handle < 0 ){

                netmsg_v_release( rx_netmsg );

                continue;
            }

            memcpy( mem2_vp_get_ptr( nm_state->data_handle ), buf, s );

            netmsg_v_receive( rx_netmsg );
        }

        // no traffic: back off so we don't busy spin
        if( !got_data ){

            TMR_WAIT( pt, SIM_RX_IDLE_WAIT_MS );
        }

        THREAD_YIELD( pt );
    }

PT_END( pt );
}

// default route: anything that is not loopback or the local IP
// goes out on the host network
int8_t sim_wifi_i8_get_route( ip_addr4_t *subnet, ip_addr4_t *subnet_mask ){

    *subnet = ip_a_addr( 0, 0, 0, 0 );
    *subnet_mask = ip_a_addr( 0, 0, 0, 0 );

    return NETMSG_ROUTE_DEFAULT;
}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){

    netmsg_state_t *netmsg_state = netmsg_vp_get_state( netmsg );

    uint16_t data_len = 0;
    uint8_t *data = 0;

    if( netmsg_state->data_handle > 0 ){

        data = mem2_vp_get_ptr( netmsg_state->data_handle );
        data_len = mem2_u16_get_size( netmsg_state->data_handle );
    }

    sim_conn_t *conn = get_conn( netmsg_state->laddr.port );

    if( conn == 0 ){

        return NETMSG_TX_ERR_RELEASE;
    }

    uint32_t dest_ip = 0;
    memcpy( &dest_ip, &netmsg_state->raddr.ipaddr, sizeof(dest_ip) );

    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = dest_ip; // 0xFFFFFFFF is INADDR_BROADCAST
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons( netmsg_state->raddr.port );

    if( sendto( conn->sock, data, data_len, 0,
                (struct sockaddr *)&destAddr, sizeof(destAddr) ) < 0 ){

        log_v_debug_P( PSTR("sim udp sendto failed: %d.%d.%d.%d:%u err: %d"),
            netmsg_state->raddr.ipaddr.ip3,
            netmsg_state->raddr.ipaddr.ip2,
            netmsg_state->raddr.ipaddr.ip1,
            netmsg_state->raddr.ipaddr.ip0,
            netmsg_state->raddr.port,
            errno );

        return NETMSG_TX_ERR_RELEASE;
    }

    return NETMSG_TX_OK_RELEASE;
}

void sim_wifi_v_open_close_port( uint8_t protocol, uint16_t port, bool open ){

    if( protocol != IP_PROTO_UDP ){

        return;
    }

    if( open ){

        // check if already open
        if( get_conn( port ) != 0 ){

            return;
        }

        // find a free slot
        int8_t index = -1;
        for( uint8_t i = 0; i < cnt_of_array(sim_conn); i++ ){

            if( sim_conn[i].lport == 0 ){

                index = i;
                break;
            }
        }

        if( index < 0 ){

            return;
        }

        sim_conn[index].lport = port;
        sim_conn[index].sock  = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );

        if( sim_conn[index].sock < 0 ){

            log_v_debug_P( PSTR("sim udp socket failed: port %u err: %d"), port, errno );

            sim_conn[index].lport = 0;
            return;
        }

        // enable broadcast receive on socket
        int val = 1;
        setsockopt( sim_conn[index].sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &val, sizeof(val) );

        struct sockaddr_in bindAddr;
        bindAddr.sin_addr.s_addr = htonl( INADDR_ANY );
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons( port );

        if( bind( sim_conn[index].sock, (struct sockaddr *)&bindAddr, sizeof(bindAddr) ) < 0 ){

            // low ports (like the wifi echo port 7) may not be bindable
            // without privileges; keep the socket, the service just won't work
            log_v_debug_P( PSTR("sim udp bind failed: port %u err: %d"), port, errno );
        }
    }
    else{

        sim_conn_t *conn = get_conn( port );

        if( conn != 0 ){

            close( conn->sock );

            memset( conn, 0, sizeof(*conn) );
        }
    }
}

void wifi_v_shutdown( void ){

    // process exit releases the sockets; nothing to do
}

bool wifi_b_connected( void ){

    // the host network stack is always up
    return true;
}

bool wifi_b_ap_mode( void ){

    return false;
}

int8_t wifi_i8_rssi( void ){

    return 0;
}

void wifi_v_switch_to_ap( void ){

}

void wifi_v_reset_scan_timeout( void ){

}
