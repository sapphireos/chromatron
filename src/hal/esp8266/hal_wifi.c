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

#include "system.h"
#include "config.h"
#include "wifi.h"
#include "threading.h"
#include "timers.h"
#include "logging.h"
#include "memory.h"
#include "list.h"
#include "sockets.h"
#include "hal_io.h"
#include "random.h"

#include "hal_arp.h"

#ifdef DEFAULT_WIFI
#include "test_ssid.h"
#endif

#include "mem.h"
#include "espconn.h"

#ifdef ENABLE_WIFI

static uint8_t wifi_mac[6];
static int8_t wifi_rssi;
static uint8_t wifi_bssid[6];
static int8_t wifi_router = -1;
static int8_t wifi_channel;
static uint32_t wifi_uptime;
static uint8_t wifi_connects;

static uint32_t wifi_udp_received;
static uint32_t wifi_udp_sent;
static uint32_t wifi_udp_dropped;
static uint32_t wifi_udp_high_load;
static uint32_t wifi_max_rx_size;

static uint32_t wifi_arp_hits;
static uint32_t wifi_arp_misses;
static uint32_t wifi_arp_msg_recovered;
static uint32_t wifi_arp_msg_fails;

static bool default_ap_mode;

static bool connected;

static bool wifi_shutdown;

static uint8_t scan_backoff;
static uint8_t current_scan_backoff;

static uint8_t tx_power = WIFI_MAX_HW_TX_POWER;

static bool enable_modem_sleep = FALSE; // default to disable modem sleep, it can be unstable and lose traffic

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

    { CATBUS_TYPE_UINT8,         0, KV_FLAGS_PERSIST,           &tx_power,          0,                   "wifi_tx_power" },
};

KV_SECTION_META kv_meta_t wifi_info_kv[] = {
    { CATBUS_TYPE_MAC48,         0, KV_FLAGS_READ_ONLY,   &wifi_mac,                         0,   "wifi_mac" },
    { CATBUS_TYPE_INT8,          0, KV_FLAGS_READ_ONLY,   &wifi_rssi,                        0,   "wifi_rssi" },
    { CATBUS_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,   &wifi_uptime,                      0,   "wifi_uptime" },
    { CATBUS_TYPE_UINT8,         0, KV_FLAGS_READ_ONLY,   &wifi_connects,                    0,   "wifi_connects" },

    { CATBUS_TYPE_INT8,          0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_channel,  0,   "wifi_channel" },
    { CATBUS_TYPE_MAC48,         0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_bssid,    0,   "wifi_bssid" },
    { CATBUS_TYPE_INT8,          0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_router,   0,   "wifi_router" },

    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_udp_received,                0,   "wifi_udp_received" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_udp_sent,                    0,   "wifi_udp_sent" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_udp_dropped,                 0,   "wifi_udp_dropped" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_udp_high_load,               0,   "wifi_udp_high_load" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_max_rx_size,                 0,   "wifi_max_rx_size" },

    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_hits,                    0,   "wifi_arp_hits" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_misses,                  0,   "wifi_arp_misses" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_msg_recovered,           0,   "wifi_arp_msg_recovered" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_msg_fails,               0,   "wifi_arp_msg_fails" },

    { CATBUS_TYPE_BOOL,          0, KV_FLAGS_PERSIST,     &enable_modem_sleep,               0,   "wifi_enable_modem_sleep" },
};


#define RX_SIGNAL SIGNAL_SYS_6


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_arp_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_arp_sender_thread( pt_t *pt, void *state ) );

static struct espconn esp_conn[WIFI_MAX_PORTS];
static esp_udp udp_conn[WIFI_MAX_PORTS];

static list_t conn_list;
static list_t rx_list;
static list_t arp_q_list;

static char hostname[32];
static uint8_t disconnect_reason;

void wifi_handle_event_cb(System_Event_t *evt)
{
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED:
            // os_printf("connect to ssid %s, channel %d\n",
            // evt->event_info.connected.ssid,
            // evt->event_info.connected.channel);
            break;
        
        case EVENT_STAMODE_DISCONNECTED:
            /*os_printf("disconnect from ssid %s, reason %d\n",
            evt->event_info.disconnected.ssid,
            evt->event_info.disconnected.reason);*/

            disconnect_reason = evt->event_info.disconnected.reason;
            connected = FALSE;
            // log_v_debug_P( PSTR("wifi disconnected") );
            break;
        
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            // os_printf("mode: %d -> %d\n",
            // evt->event_info.auth_change.old_mode,
            // evt->event_info.auth_change.new_mode);
            break;
        
        case EVENT_STAMODE_GOT_IP:
            // os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
            // IP2STR(&evt->event_info.got_ip.ip),
            // IP2STR(&evt->event_info.got_ip.mask),
            // IP2STR(&evt->event_info.got_ip.gw));
            // os_printf("\n");
            break;
        
        case EVENT_SOFTAPMODE_STACONNECTED:
            // os_printf("station: " MACSTR "join, AID = %d\n",
            // MAC2STR(evt->event_info.sta_connected.mac),
            // evt->event_info.sta_connected.aid);
            break;
        
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            // os_printf("station: " MACSTR "leave, AID = %d\n",
            // MAC2STR(evt->event_info.sta_disconnected.mac),
            // evt->event_info.sta_disconnected.aid);
            break;

        default:
            break;
    }
}

void hal_wifi_v_init( void ){
	
	wifi_station_disconnect();

	// set opmode without saving to flash (since we always set this)
	wifi_set_opmode_current( STATION_MODE );

	thread_t_create_critical( 
                 wifi_connection_manager_thread,
                 PSTR("wifi_connection_manager"),
                 0,
                 0 );

    thread_t_create_critical( 
                 wifi_rx_process_thread,
                 PSTR("wifi_rx_processor"),
                 0,
                 0 );

    thread_t_create_critical( 
                wifi_status_thread,
                PSTR("wifi_status"),
                0,
                0 );

    thread_t_create_critical( 
                wifi_arp_thread,
                PSTR("wifi_arp"),
                0,
                0 );

    if( sys_u8_get_mode() != SYS_MODE_SAFE ){
        
        thread_t_create_critical( 
                    wifi_arp_sender_thread,
                    PSTR("wifi_arp_tx_q"),
                    0,
                    0 );
    }

	wifi_get_macaddr( 0, wifi_mac );

    uint64_t current_device_id = 0;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &current_device_id );
    uint64_t device_id = 0;
    memcpy( &device_id, wifi_mac, sizeof(wifi_mac) );

    if( current_device_id != device_id ){

        cfg_v_set( CFG_PARAM_DEVICE_ID, &device_id );
    }

    #ifdef DEFAULT_WIFI
    char ssid[32];
    char pass[32];
    memset( ssid, 0, 32 );
    memset( pass, 0, 32 );
    strcpy(ssid, SSID);
    strcpy(pass, PASSWORD);
    cfg_v_set( CFG_PARAM_WIFI_SSID, ssid );
    cfg_v_set( CFG_PARAM_WIFI_PASSWORD, pass );
    #endif

    list_v_init( &conn_list );
    list_v_init( &rx_list );
    list_v_init( &arp_q_list );

    // set tx power
    system_phy_set_max_tpw( tx_power * 4 );

    // set sleep mode
    wifi_set_sleep_type( MODEM_SLEEP_T );
    // wifi_set_sleep_type( NONE_SLEEP_T );

    // disable auto reconnect (we will manage this)
    wifi_station_set_auto_connect( FALSE );
    wifi_station_set_reconnect_policy( FALSE );

    // MUST set a callback to detect connection loss!
    wifi_set_event_handler_cb( wifi_handle_event_cb );

    // set up hostname
    char mac_str[16];
    memset( mac_str, 0, sizeof(mac_str) );
    snprintf( &mac_str[0], 3, "%02x", wifi_mac[3] );
    snprintf( &mac_str[2], 3, "%02x", wifi_mac[4] ); 
    snprintf( &mac_str[4], 3, "%02x", wifi_mac[5] );

    memset( hostname, 0, sizeof(hostname) );
    strlcpy( hostname, "Chromatron_", sizeof(hostname) );

    strlcat( hostname, mac_str, sizeof(hostname) );

    wifi_station_set_hostname( hostname );

 //    struct station_config config = {0};
 //    strcpy( &config.ssid, ssid );
 //    strcpy( &config.password, password );

 //    wifi_station_set_config( &config );

 //    wifi_station_connect();
}


void udp_recv_callback( void *arg, char *pdata, unsigned short len ){

	struct espconn* conn = (struct espconn*)arg;

	remot_info *remote_info = 0;
   
    espconn_get_connection_info( conn, &remote_info, 0 );

 //    if(remote_info->remote_port != 44632){
	// trace_printf("RX: %d bytes from %d.%d.%d.%d:%u -> %u\n", 
	// 	len, 
	// 	remote_info->remote_ip[0],
	// 	remote_info->remote_ip[1],
	// 	remote_info->remote_ip[2],
	// 	remote_info->remote_ip[3],
	// 	remote_info->remote_port,
 //        conn->proto.udp->local_port);
 //    }

    // check rx size
    if( list_u16_size( &rx_list ) > WIFI_MAX_RX_SIZE ){

        log_v_debug_P( PSTR("rx udp buffer full") );     

        goto drop;
    }

    netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

    if( rx_netmsg < 0 ){

        log_v_debug_P( PSTR("rx udp alloc fail: buffer count: %d size: %d"), 
            list_u8_count( &rx_list ), 
            list_u16_size( &rx_list ) );     

        goto drop;
    }
    
    netmsg_state_t *state = netmsg_vp_get_state( rx_netmsg );

    // set up addressing info
    state->laddr.port       = conn->proto.udp->local_port;
    state->raddr.port       = remote_info->remote_port;
    state->raddr.ipaddr.ip3 = remote_info->remote_ip[0];
    state->raddr.ipaddr.ip2 = remote_info->remote_ip[1];
    state->raddr.ipaddr.ip1 = remote_info->remote_ip[2];
    state->raddr.ipaddr.ip0 = remote_info->remote_ip[3];

    // allocate data buffer
    state->data_handle = mem2_h_alloc2( len, MEM_TYPE_SOCKET_BUFFER );

    if( state->data_handle < 0 ){

        log_v_error_P( PSTR("rx udp no handle") );     

        netmsg_v_release( rx_netmsg );

        goto drop;
    }      

    // we can get a fast ptr because we've already verified the handle
    uint8_t *data = mem2_vp_get_ptr_fast( state->data_handle );
    memcpy( data, pdata, len );

    // post to rx Q
    if( list_u8_count( &rx_list ) == ( WIFI_MAX_RX_NETMSGS - 1 ) ){

        if( sock_b_rx_pending() ){

            log_v_error_P( PSTR("rx q almost full, dropping pending socket") );     

            // clear pending data on whatever socket is blocking.
            sock_v_clear_rx_pending();
        }
    }
    else if( list_u8_count( &rx_list ) >= WIFI_MAX_RX_NETMSGS ){

        log_v_warn_P( PSTR("rx q full: len: %d lport: %u rport: %d.%d.%d.%d:%u"), 
            len, 
            state->laddr.port, 
            state->raddr.ipaddr.ip3, 
            state->raddr.ipaddr.ip2, 
            state->raddr.ipaddr.ip1, 
            state->raddr.ipaddr.ip0, 
            state->raddr.port );     

        if( sock_b_rx_pending() ){

            // clear pending data on whatever socket is blocking.
            sock_v_clear_rx_pending();
        }

        // drop this message
        netmsg_v_release( rx_netmsg );

        goto drop;
    }
    else if( list_u8_count( &rx_list ) == (WIFI_MAX_RX_NETMSGS * 0.75 ) ){

        log_v_warn_P( PSTR("rx q 75%%") );     
    }
    else if( list_u8_count( &rx_list ) > (WIFI_MAX_RX_NETMSGS * 0.75 ) ){

        wifi_udp_high_load++;
    }

    wifi_udp_received++;

    list_v_insert_head( &rx_list, rx_netmsg );

    uint16_t q_size = list_u16_size( &rx_list );
    if( q_size > wifi_max_rx_size ){

        wifi_max_rx_size = q_size;
    }

    thread_v_signal( RX_SIGNAL );

    return;


drop:

    wifi_udp_dropped++;
}


PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        log_v_debug_P( PSTR("ESP8266 SDK %s"), system_get_sdk_version() );    
    }
    
    
    while(1){

        if( list_u8_count( &rx_list ) == 0 ){

            THREAD_WAIT_SIGNAL( pt, RX_SIGNAL );    
        }

        THREAD_WAIT_WHILE( pt, list_u8_count( &rx_list ) == 0 );
        THREAD_WAIT_WHILE( pt, sock_b_rx_pending() ); // wait while sockets are busy

        netmsg_t rx_netmsg = list_ln_remove_tail( &rx_list );
        netmsg_v_receive( rx_netmsg );

        THREAD_YIELD( pt );
    }   

PT_END( pt );
}

int8_t hal_wifi_i8_igmp_join( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }
            
    struct ip_info info;
            memset( &info, 0, sizeof(info) );
            wifi_get_ip_info( STATION_IF, &info );    
            
    ip_addr_t ipgroup;
    ipgroup.addr = HTONL(ip_u32_to_int( mcast_ip ));

    return espconn_igmp_join( &info.ip, &ipgroup );
}

int8_t hal_wifi_i8_igmp_leave( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    struct ip_info info;
            memset( &info, 0, sizeof(info) );
            wifi_get_ip_info( STATION_IF, &info );    
            
    ip_addr_t ipgroup;
    ipgroup.addr = HTONL(ip_u32_to_int( mcast_ip ));

    return espconn_igmp_leave( &info.ip, &ipgroup );
}

struct espconn* get_conn( uint8_t protocol, uint16_t lport ){

	for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

		if( esp_conn[i].type == ESPCONN_INVALID ){

			continue;
		}

        if( protocol == IP_PROTO_UDP ){

            if( esp_conn[i].type != ESPCONN_UDP ){

                continue;
            }
        }
        else{

            ASSERT( FALSE );
        }

		if( esp_conn[i].proto.udp->local_port == lport ){

			return &esp_conn[i];
		}
	}

	return 0;
}

void open_close_port( uint8_t protocol, uint16_t port, bool open ){

    if( open ){

        // trace_printf("Open port: %u\n", port);

        int8_t index = -1;
        for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

            if( esp_conn[i].type == ESPCONN_INVALID ){

                index = i;
                break;
            }
        }

        if( index < 0 ){

            ASSERT( FALSE );

            return;
        }

        memset( &esp_conn[index], 0, sizeof(esp_conn[index]) );
        memset( &udp_conn[index], 0, sizeof(udp_conn[index]) );

        if( protocol == IP_PROTO_UDP ){

            esp_conn[index].type            = ESPCONN_UDP;
            esp_conn[index].proto.udp       = &udp_conn[index];
            esp_conn[index].recv_callback   = udp_recv_callback;
            esp_conn[index].sent_callback   = 0;
        }
        else{

            ASSERT( FALSE );
        }
        
        esp_conn[index].state           = ESPCONN_NONE; 
        esp_conn[index].link_cnt        = 0;
        esp_conn[index].reverse         = 0;

        udp_conn[index].remote_port     = 0;
        udp_conn[index].local_port      = port;

        espconn_create( &esp_conn[index] );
    }
    else{

        // trace_printf("Close port: %u\n", port);
        
        struct espconn* conn = get_conn( protocol, port );

        if( conn != 0 ){

            espconn_delete( conn );
            conn->type = ESPCONN_INVALID;   
        }
    }
}

static bool is_low_power_mode( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // safe mode, we're not going to do anything fancy

        return FALSE;
    }

    if( enable_modem_sleep ){

        return TRUE;
    }

    return FALSE;
}

static void apply_power_save_mode( void ){

    // set power state
    if( is_low_power_mode() ){

        wifi_set_sleep_type( MODEM_SLEEP_T );
    }
    else{

        wifi_set_sleep_type( NONE_SLEEP_T );
    }
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


void wifi_v_shutdown( void ){

    wifi_shutdown = TRUE;
}

void wifi_v_powerup( void ){

    wifi_shutdown = FALSE;
}

void wifi_v_reset_scan_timeout( void ){

    scan_backoff = 0;
    current_scan_backoff = 0;
}

bool wifi_b_connected( void ){

	return connected;
}

bool wifi_b_attached( void ){

	return TRUE;
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

void wifi_v_switch_to_ap( void ){

    connected = FALSE;
    default_ap_mode = TRUE;
}

bool wifi_b_ap_mode( void ){

    if( wifi_get_opmode() == SOFTAP_MODE ){

        return TRUE;
    }

	return FALSE;
}

int8_t wifi_i8_get_status( void ){

    return 0;
}

int8_t wifi_i8_get_channel( void ){

    return wifi_channel;
}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){

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

    uint8_t protocol = IP_PROTO_UDP;

    // get esp conn
    struct espconn* conn = get_conn( protocol, netmsg_state->laddr.port );

    // check if general broadcast
    if( ip_b_check_broadcast( netmsg_state->raddr.ipaddr ) ){

        // change to subnet
        ip_addr4_t subnet;
        cfg_i8_get( CFG_PARAM_IP_SUBNET_MASK, &subnet );
        ip_addr4_t gw;
        cfg_i8_get( CFG_PARAM_INTERNET_GATEWAY, &gw );

        netmsg_state->raddr.ipaddr.ip3 = ~subnet.ip3 | gw.ip3;
        netmsg_state->raddr.ipaddr.ip2 = ~subnet.ip2 | gw.ip2;
        netmsg_state->raddr.ipaddr.ip1 = ~subnet.ip1 | gw.ip1;
        netmsg_state->raddr.ipaddr.ip0 = ~subnet.ip0 | gw.ip0;
    }

    conn->proto.udp->remote_ip[0] = netmsg_state->raddr.ipaddr.ip3;
    conn->proto.udp->remote_ip[1] = netmsg_state->raddr.ipaddr.ip2;
    conn->proto.udp->remote_ip[2] = netmsg_state->raddr.ipaddr.ip1;
    conn->proto.udp->remote_ip[3] = netmsg_state->raddr.ipaddr.ip0;
    conn->proto.udp->remote_port = netmsg_state->raddr.port;

    // int8_t arp_status = etharp_find_addr
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        if( !hal_arp_b_find( netmsg_state->raddr.ipaddr ) ){

            wifi_arp_misses++;

            // log_v_debug_P( PSTR("ARP not in cache: %d.%d.%d.%d"), netmsg_state->raddr.ipaddr.ip3,netmsg_state->raddr.ipaddr.ip2,netmsg_state->raddr.ipaddr.ip1,netmsg_state->raddr.ipaddr.ip0 );            

            // int8_t arp_status = hal_arp_i8_query( netmsg_state->raddr.ipaddr );

            // if( arp_status < 0 ){

            //     log_v_warn_P( PSTR("ARP query fail: %d"), arp_status );            
            // }

            // we will manually try the ARP on the sender q thread
            if( list_u8_count( &arp_q_list ) < 4 ){

                list_v_insert_head( &arp_q_list, netmsg );

                // don't release netmsg
                return NETMSG_TX_OK_NORELEASE; 
            }
            
            // q was full, just try and transmit anyway
        }
        else{

            wifi_arp_hits++;
        }
    }

    // trace_printf("sendto: %d.%d.%d.%d:%u\n", netmsg_state->raddr.ipaddr.ip3,netmsg_state->raddr.ipaddr.ip2,netmsg_state->raddr.ipaddr.ip1,netmsg_state->raddr.ipaddr.ip0, netmsg_state->raddr.port);
    int16_t send_status = espconn_sendto( conn, data, data_len );
    if( send_status != 0 ){

        log_v_warn_P( PSTR("msg failed: %d"), send_status );

        return NETMSG_TX_ERR_RELEASE;   
    }
    

    wifi_udp_sent++;

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

static void get_pass( int8_t router, char pass[WIFI_PASS_LEN] ){

	memset( pass, 0, WIFI_PASS_LEN );

	if( router == 0 ){

		cfg_i8_get( CFG_PARAM_WIFI_PASSWORD, pass );
	}
    else if( router == 1 ){
	   	
	   	kv_i8_get( __KV__wifi_password2, pass, WIFI_PASS_LEN );
	}
	else if( router == 2 ){
	   	
	   	kv_i8_get( __KV__wifi_password3, pass, WIFI_PASS_LEN );
	}
	else if( router == 3 ){
	   	
	   	kv_i8_get( __KV__wifi_password4, pass, WIFI_PASS_LEN );
	}
}

void scan_cb( void *result, STATUS status ){

	struct bss_info* info = (struct bss_info*)result;

	int8_t best_rssi = -127;
	uint8_t best_channel = 0;
	uint8_t *best_bssid = 0;
	int8_t best_router = -1;

    char ssid[4][WIFI_SSID_LEN];
    memset( ssid, 0, sizeof(ssid) );
    cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid[0] );
    kv_i8_get( __KV__wifi_ssid2, ssid[1], sizeof(ssid[1]) );
    kv_i8_get( __KV__wifi_ssid3, ssid[2], sizeof(ssid[2]) );
    kv_i8_get( __KV__wifi_ssid4, ssid[3], sizeof(ssid[3]) );

	while( info != 0 ){

		// trace_printf("%s %u %d\n", info->ssid, info->channel, info->rssi);

        int8_t router = -1;

        if( strncmp( (char *)info->ssid, ssid[0], WIFI_SSID_LEN ) == 0 ){

            router = 0;
        }
        else if( strncmp( (char *)info->ssid, ssid[1], WIFI_SSID_LEN ) == 0 ){

            router = 1;
        }
        else if( strncmp( (char *)info->ssid, ssid[2], WIFI_SSID_LEN ) == 0 ){

            router = 2;
        }
        else if( strncmp( (char *)info->ssid, ssid[3], WIFI_SSID_LEN ) == 0 ){

            router = 3;
        }

		if( router < 0 ){

			goto end;
		}

		if( info->rssi > best_rssi ){

			best_bssid 		= info->bssid;
			best_rssi 		= info->rssi;
			best_channel 	= info->channel;
			best_router 	= router;
		}

	end:
		info = info->next.stqe_next;
	}

	if( best_rssi == -127 ){

		return;
	}

	// select router
	wifi_router = best_router;
	memcpy( wifi_bssid, best_bssid, sizeof(wifi_bssid) );
	wifi_channel = best_channel;
}

static bool is_ssid_configured( void ){

	char ssid[WIFI_SSID_LEN];
	
	memset( ssid, 0, sizeof(ssid) );
	cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );
       
   	if( ssid[0] != 0 ){

   		return TRUE;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid2, ssid, sizeof(ssid) );
       
   	if( ssid[0] != 0 ){

   		return TRUE;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid3, ssid, sizeof(ssid) );
       
   	if( ssid[0] != 0 ){

   		return TRUE;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid4, ssid, sizeof(ssid) );
       
   	if( ssid[0] != 0 ){

   		return TRUE;
   	}

   	return FALSE;
}

// static void start_mdns_igmp( void ){

//     struct ip_info sta_ip;
//     wifi_get_ip_info( STATION_IF, &sta_ip );

//     ip_addr_t ip;
//     ip.addr = sta_ip.ip.addr;

//     ip_addr_t mcast_ip;
//     mcast_ip.addr = 0xe00000fb;

//     espconn_igmp_join( &ip, &mcast_ip );
// }

// static void stop_mdns_igmp( void ){
    
//     struct ip_info sta_ip;
//     wifi_get_ip_info( STATION_IF, &sta_ip );

//     ip_addr_t ip;
//     ip.addr = sta_ip.ip.addr;

//     ip_addr_t mcast_ip;
//     mcast_ip.addr = 0xe00000fb;

//     espconn_igmp_leave( &ip, &mcast_ip );
// }

// static void start_mdns( void ){

//     if( sys_u8_get_mode() == SYS_MODE_SAFE ){

//         return;
//     }

//     start_mdns_igmp();
    
    
    
// }

static bool is_led_quiet_mode( void ){

    if( ( cfg_b_get_boolean( CFG_PARAM_ENABLE_LED_QUIET_MODE ) ) &&
        ( tmr_u64_get_system_time_us() > 10000000 ) ){

        return TRUE;
    }

    return FALSE;
}

PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint8_t scan_timeout;

    connected = FALSE;
    wifi_rssi = -127;

    wifi_set_opmode_current( NULL_MODE );

    THREAD_WAIT_WHILE( pt, wifi_shutdown );

    // check if we are connected
    while( !wifi_b_connected() && !wifi_shutdown ){

        wifi_rssi = -127;

        // reset power save mode to off
        // wifi_set_sleep_type( NONE_SLEEP_T );

        wifi_set_opmode_current( NULL_MODE );

        current_scan_backoff = scan_backoff;

        // scan backoff delay
        while( ( current_scan_backoff > 0 ) && !_wifi_b_ap_mode_enabled() ){

            current_scan_backoff--;
            TMR_WAIT( pt, 1000 );
        }

        
        bool ap_mode = _wifi_b_ap_mode_enabled();

        // check if no APs are configured
        if( !is_ssid_configured() ){

            // no SSIDs are configured

            // switch to AP mode
            ap_mode = TRUE;
        }
 
        // station mode
        if( !ap_mode ){
// station_mode:            

            wifi_set_opmode_current( STATION_MODE );

            // check if we can try a fast connect with the last connected router
            if( wifi_router >= 0 ){

                log_v_debug_P( PSTR("Fast connect...") );
            }
            else{

                // scan first
                wifi_router = -1;
                log_v_debug_P( PSTR("Scanning...") );
                
                struct scan_config scan_config;
                memset( &scan_config, 0, sizeof(scan_config) );
                
                // light LED while scanning since the CPU will freeze
                if( !is_led_quiet_mode() ){
                    
                    io_v_set_esp_led( 1 );
                }

                wifi_station_disconnect();
                if( wifi_station_scan( &scan_config, scan_cb ) != TRUE ){

                	log_v_error_P( PSTR("Scan error") );
                }

                scan_timeout = 200;
                while( ( wifi_router < 0 ) && ( scan_timeout > 0 ) ){

                    scan_timeout--;

                    TMR_WAIT( pt, 50 );
                }

                io_v_set_esp_led( 0 );

                if( wifi_router < 0 ){

                    // router not found

                    if( scan_backoff == 0 ){

                        scan_backoff = 1;
                    }
                    else if( scan_backoff < 64 ){

                        scan_backoff *= 2;

                        log_v_debug_P( PSTR("scan backoff: %d"), scan_backoff );

                        TMR_WAIT( pt, rnd_u16_get_int() >> 5 ); // add 2 seconds of random delay
                    }
                    else if( scan_backoff < 192 ){

                        scan_backoff += 64;

                        log_v_debug_P( PSTR("scan backoff: %d"), scan_backoff );

                        TMR_WAIT( pt, rnd_u16_get_int() >> 4 ); // add 4 seconds of random delay
                    }

                    goto end;
                }            
            }
    
			struct station_config sta_config;
			memset( &sta_config, 0, sizeof(sta_config) );
			memcpy( sta_config.bssid, wifi_bssid, sizeof(sta_config.bssid) );
			sta_config.bssid_set = 1;
			sta_config.channel = wifi_channel;
			wifi_v_get_ssid( (char *)sta_config.ssid );
			get_pass( wifi_router, (char *)sta_config.password );

            log_v_debug_P( PSTR("Connecting to %s ch: %d"), (char *)sta_config.ssid, wifi_channel );

			wifi_station_set_config_current( &sta_config );
			wifi_station_connect();

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( wifi_station_get_connect_status() != STATION_GOT_IP ) &&
                                   ( thread_b_alarm_set() ) );

           	// check if we're connected
           	if( wifi_station_get_connect_status() == STATION_GOT_IP ){

           		connected = TRUE;

           		// get IP info
           		struct ip_info info;
           		memset( &info, 0, sizeof(info) );
           		wifi_get_ip_info( STATION_IF, &info );

				cfg_v_set( CFG_PARAM_IP_ADDRESS, &info.ip );
			    cfg_v_set( CFG_PARAM_IP_SUBNET_MASK, &info.netmask );
                cfg_v_set( CFG_PARAM_INTERNET_GATEWAY, &info.gw );

                ip_addr_t dns_ip = espconn_dns_getserver( 0 );
			    cfg_v_set( CFG_PARAM_DNS_SERVER, &dns_ip );
           	}
            else{

                // connection failed
                wifi_router = -1;
                wifi_channel = -1;
                memset( wifi_bssid, 0, sizeof(wifi_bssid) );

                log_v_debug_P( PSTR("Connection failed: %d"), wifi_station_get_connect_status() );
            }

            kv_i8_persist( __KV__wifi_channel );
            kv_i8_persist( __KV__wifi_bssid );
            kv_i8_persist( __KV__wifi_router );
        }
        // AP mode
        else{
            // should have gotten the MAC by now
            ASSERT( wifi_mac[0] != 0 );

            // AP mode
            char ap_ssid[WIFI_SSID_LEN];
            char ap_pass[WIFI_PASS_LEN];

            cfg_i8_get( CFG_PARAM_WIFI_AP_SSID, ap_ssid );
            cfg_i8_get( CFG_PARAM_WIFI_AP_PASSWORD, ap_pass );

            // check if AP mode SSID is set:
            if( ( ap_ssid[0] == 0 ) || ( default_ap_mode ) ){

                // set up default AP
                memset( ap_ssid, 0, sizeof(ap_ssid) );
                memset( ap_pass, 0, sizeof(ap_pass) );

                strlcpy_P( ap_ssid, PSTR("Chromatron_"), sizeof(ap_ssid) );

                char mac[16];
                memset( mac, 0, sizeof(mac) );
                snprintf_P( &mac[0], 3, PSTR("%02x"), wifi_mac[3] );
                snprintf_P( &mac[2], 3, PSTR("%02x"), wifi_mac[4] ); 
                snprintf_P( &mac[4], 3, PSTR("%02x"), wifi_mac[5] );

                strncat( ap_ssid, mac, sizeof(ap_ssid) );

                strlcpy_P( ap_pass, PSTR("12345678"), sizeof(ap_pass) );

                default_ap_mode = TRUE;
            }
            else if( strnlen( ap_pass, sizeof(ap_pass) ) < WIFI_AP_MIN_PASS_LEN ){

                log_v_warn_P( PSTR("AP mode password must be at least 8 characters.") );

                // disable ap mode
                bool temp = FALSE;
                cfg_v_set( CFG_PARAM_WIFI_ENABLE_AP, &temp );

                goto end;
            }

            log_v_debug_P( PSTR("Starting AP: %s"), ap_ssid );

            // check if wifi settings were present
            if( ap_ssid[0] != 0 ){     

                struct softap_config ap_config;
                memset( &ap_config, 0, sizeof(ap_config) );
                // wifi_softap_get_config( &ap_config );
                memcpy( (char *)ap_config.ssid, ap_ssid, sizeof(ap_ssid) );
                memcpy( (char *)ap_config.password, ap_pass, sizeof(ap_pass) );

                ap_config.ssid_len          = strlen(ap_ssid);
                ap_config.channel           = 0;
                ap_config.authmode          = AUTH_WPA2_PSK;
                ap_config.ssid_hidden       = 0;
                ap_config.max_connection    = 4;
                ap_config.beacon_interval   = 100;

                wifi_set_opmode_current( SOFTAP_MODE );

                wifi_softap_set_config_current( &ap_config );
                
                wifi_softap_dhcps_start();

                connected = TRUE;
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
            log_v_debug_P( PSTR("Wifi connected to: %s ch: %d"), ssid, wifi_channel );
        }
        else{

            log_v_debug_P( PSTR("Wifi soft AP up") );    
        }

        // start_mdns();
    }

    THREAD_WAIT_WHILE( pt, wifi_b_connected()  && !wifi_shutdown );
    
    log_v_debug_P( PSTR("Wifi disconnected: %d Last RSSI: %d ch: %d"), disconnect_reason, wifi_rssi, wifi_channel );

    wifi_v_reset_scan_timeout();

    THREAD_RESTART( pt );

PT_END( pt );
}

PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        uint8_t status = wifi_station_get_connect_status();

        if( ( status == STATION_GOT_IP ) || wifi_b_ap_mode() ){

            wifi_uptime++;
            connected = TRUE;
        }
        else{

            if( connected ){

                // if previously connected, log status
                log_v_debug_P( PSTR("lost connection: %d"), status );
            }

            wifi_uptime = 0;
            connected = FALSE;
        }

        if( connected ){

            wifi_rssi = wifi_station_get_rssi();

            apply_power_save_mode();
        }
    }

PT_END( pt );
}

// the ESP8266 sometimes stops responding to ARP requests.
// so, we will periodically send a gratuitous ARP.
PT_THREAD( wifi_arp_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        hal_arp_v_gratuitous_arp();
        TMR_WAIT( pt, 2000 );

        hal_arp_v_gratuitous_arp();
        TMR_WAIT( pt, 4000 );

        hal_arp_v_gratuitous_arp();

        while( wifi_b_connected() ){

            TMR_WAIT( pt, ARP_GRATUITOUS_INTERVAL * 1000 );

            hal_arp_v_gratuitous_arp();
        }
    }

PT_END( pt );
}

PT_THREAD( wifi_arp_sender_thread( pt_t *pt, void *state ) )
{
    netmsg_t netmsg = arp_q_list.tail;
    netmsg_state_t *netmsg_state = 0;

    if( netmsg > 0 ){

        netmsg_state = netmsg_vp_get_state( netmsg );
    }

PT_BEGIN( pt );

    while(1){

        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, list_u8_count( &arp_q_list ) == 0 );

        // check ARP table
        if( hal_arp_b_find( netmsg_state->raddr.ipaddr ) ){

            // entry found, great!
            int8_t status = wifi_i8_send_udp( netmsg );
            if( ( status == NETMSG_TX_OK_RELEASE ) || ( status == NETMSG_TX_OK_NORELEASE ) ){

                wifi_arp_msg_recovered++;
            }
            else{

                wifi_arp_msg_fails++;
            }

            // regardless of outcome, we're freeing this packet
            list_ln_remove_tail( &arp_q_list );
            netmsg_v_release( netmsg );
        }
        else{

            // no ARP entry, let's send a request, delay, and re-loop

            int8_t arp_status = hal_arp_i8_query( netmsg_state->raddr.ipaddr );

            if( arp_status < 0 ){

                log_v_warn_P( PSTR("ARP query fail: %d"), arp_status );            
            }

            #define ARP_WAIT_TIME 20 // 20 ms should be an eternity for ARP
            thread_v_set_alarm( tmr_u32_get_system_time_ms() + ARP_WAIT_TIME );
            THREAD_WAIT_WHILE( pt, ( !hal_arp_b_find( netmsg_state->raddr.ipaddr ) ) &&
                                   ( thread_b_alarm_set() ) );

            if( hal_arp_b_find( netmsg_state->raddr.ipaddr ) ){

                // entry found.

                // bounce back to top of loop so we transmit.

                continue;
            }

            // ARP failed!

            // bummer.  drop packet.

            list_ln_remove_tail( &arp_q_list );
            netmsg_v_release( netmsg );

            wifi_arp_msg_fails++;
        }
    }

PT_END( pt );
}

#else

bool wifi_b_connected( void ){

	return FALSE;
}

bool wifi_b_attached( void ){

	return TRUE;
}

#endif

