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
#include "hal_sim.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>

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
// loopback broadcast: firmware broadcasts (DISCOVER/ANNOUNCE/SHUTDOWN) are
// sent here, and every listening port also gets a "tap" socket bound here,
// so sim traffic stays on this host
#define SIM_BROADCAST_ADDR      "127.255.255.255"

typedef struct {
    uint16_t lport;
    int sock;
    int tap; // broadcast tap socket on SIM_BROADCAST_ADDR:lport; -1 if none
} sim_conn_t;

static sim_conn_t sim_conn[SIM_WIFI_MAX_PORTS];

// per-sim local IP ($SIM_IP, see hal_sim.c): sockets bind here instead of
// INADDR_ANY, so several sim processes on one host don't collide on the
// fixed firmware ports and each presents a distinct source IP
static uint32_t sim_bind_ip; // network byte order

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

    const char *str = getenv( "SIM_IP" );
    sim_bind_ip = ( str != 0 ) ? inet_addr( str ) : htonl( INADDR_LOOPBACK );

    thread_t_create( sim_wifi_rx_thread, PSTR("sim_wifi_rx"), 0, 0 );
}

// pull one pending datagram from fd into the netmsg layer; lport is the
// firmware local port to attribute it to (the tap socket receives for the
// connection's data port)
static bool sim_b_rx_datagram( int fd, uint16_t lport ){

    uint8_t buf[SIM_RX_BUF_LEN];
    struct sockaddr_in sourceAddr;
    socklen_t socklen = sizeof(sourceAddr);

    int s = recvfrom( fd, buf, sizeof(buf), MSG_DONTWAIT,
                      (struct sockaddr *)&sourceAddr, &socklen );

    if( s <= 0 ){

        return FALSE;
    }

    netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

    if( rx_netmsg < 0 ){

        return TRUE;
    }

    netmsg_state_t *nm_state = netmsg_vp_get_state( rx_netmsg );

    // set up addressing info
    nm_state->laddr.port       = lport;
    nm_state->raddr.port       = ntohs(sourceAddr.sin_port);
    nm_state->raddr.ipaddr.ip3 = sourceAddr.sin_addr.s_addr >> 0;
    nm_state->raddr.ipaddr.ip2 = sourceAddr.sin_addr.s_addr >> 8;
    nm_state->raddr.ipaddr.ip1 = sourceAddr.sin_addr.s_addr >> 16;
    nm_state->raddr.ipaddr.ip0 = sourceAddr.sin_addr.s_addr >> 24;

    // allocate data buffer
    nm_state->data_handle = mem2_h_alloc2( s, MEM_TYPE_SOCKET_BUFFER );

    if( nm_state->data_handle < 0 ){

        netmsg_v_release( rx_netmsg );
        return TRUE;
    }

    memcpy( mem2_vp_get_ptr( nm_state->data_handle ), buf, s );

    netmsg_v_receive( rx_netmsg );

    return TRUE;
}

// poll open sockets (and their broadcast taps) and feed received datagrams
// into the netmsg layer
PT_THREAD( sim_wifi_rx_thread( pt_t *pt, void *state ) ){

PT_BEGIN( pt );

    while( 1 ){

        bool got_data = FALSE;

        for( uint8_t i = 0; i < cnt_of_array(sim_conn); i++ ){

            if( sim_conn[i].lport == 0 ){

                continue;
            }

            if( sim_b_rx_datagram( sim_conn[i].sock, sim_conn[i].lport ) ){

                got_data = TRUE;
            }

            if( ( sim_conn[i].tap >= 0 ) &&
                sim_b_rx_datagram( sim_conn[i].tap, sim_conn[i].lport ) ){

                got_data = TRUE;
            }
        }

        // no traffic: back off so we don't busy spin
        if( !got_data ){

            TMR_WAIT( pt, SIM_RX_IDLE_WAIT_MS );
        }

        THREAD_YIELD( pt );
    }

PT_END( pt );
}

int8_t hal_wifi_i8_poll_fds( struct pollfd *pfds, int8_t max_fds ){

    // hand the open sockets (data sockets and their broadcast taps) to the
    // idle wait (hal_sim_v_idle) so network traffic wakes the simulator
    // immediately instead of at the next poll
    int8_t count = 0;

    for( uint8_t i = 0; i < cnt_of_array(sim_conn); i++ ){

        if( sim_conn[i].lport == 0 ){

            continue;
        }

        if( count < max_fds ){

            pfds[count].fd = sim_conn[i].sock;
            pfds[count].events = POLLIN;
            pfds[count].revents = 0;

            count++;
        }

        if( ( sim_conn[i].tap >= 0 ) && ( count < max_fds ) ){

            pfds[count].fd = sim_conn[i].tap;
            pfds[count].events = POLLIN;
            pfds[count].revents = 0;

            count++;
        }
    }

    return count;
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

    // keep sim-originated global broadcasts on the loopback, so
    // announce/shutdown streams don't leak to the LAN (the sim is not
    // reachable from there anyway)
    if( dest_ip == (uint32_t)0xFFFFFFFF ){

        dest_ip = inet_addr( SIM_BROADCAST_ADDR );
    }

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
        sim_conn[index].tap   = -1;

        // adopt the socket reserved by sim_v_assign_ip: it already holds
        // the sim IP on SIM_IP_PROBE_PORT, and holding it across process
        // startup is what keeps a concurrently-launched sim from claiming
        // the same IP
        int sock = hal_sim_i8_take_reserved_sock();

        if( ( sock < 0 ) || ( port != SIM_IP_PROBE_PORT ) ){

            if( sock >= 0 ){

                close( sock );
            }

            sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );
        }

        sim_conn[index].sock = sock;

        if( sim_conn[index].sock < 0 ){

            log_v_debug_P( PSTR("sim udp socket failed: port %u err: %d"), port, errno );

            sim_conn[index].lport = 0;
            return;
        }

        // enable broadcast receive on socket
        int val = 1;
        setsockopt( sim_conn[index].sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &val, sizeof(val) );

        if( port != SIM_IP_PROBE_PORT ){

            // the adopted socket is already bound
            struct sockaddr_in bindAddr;
            bindAddr.sin_addr.s_addr = sim_bind_ip;
            bindAddr.sin_family = AF_INET;
            bindAddr.sin_port = htons( port );

            if( bind( sim_conn[index].sock, (struct sockaddr *)&bindAddr, sizeof(bindAddr) ) < 0 ){

                // low ports (like the wifi echo port 7) may not be bindable
                // without privileges; keep the socket, the service just won't work
                log_v_debug_P( PSTR("sim udp bind failed: port %u err: %d"), port, errno );
            }
        }

        // broadcast tap: a socket bound to a specific IP never receives
        // broadcasts, so listen on the loopback broadcast address for this
        // port too.  All sim processes may share the tuple (SO_REUSEADDR
        // fans out to every binder), which is how each sim hears discovery
        // traffic without fighting over the fixed firmware ports
        sim_conn[index].tap = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );

        if( sim_conn[index].tap >= 0 ){

            setsockopt( sim_conn[index].tap, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val) );

            struct sockaddr_in tapAddr;
            tapAddr.sin_addr.s_addr = inet_addr( SIM_BROADCAST_ADDR );
            tapAddr.sin_family = AF_INET;
            tapAddr.sin_port = htons( port );

            if( bind( sim_conn[index].tap, (struct sockaddr *)&tapAddr, sizeof(tapAddr) ) < 0 ){

                log_v_debug_P( PSTR("sim udp tap bind failed: port %u err: %d"), port, errno );

                close( sim_conn[index].tap );
                sim_conn[index].tap = -1;
            }
        }
    }
    else{

        sim_conn_t *conn = get_conn( port );

        if( conn != 0 ){

            close( conn->sock );

            if( conn->tap >= 0 ){

                close( conn->tap );
            }

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
