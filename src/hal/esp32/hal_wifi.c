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

#include "hal_arp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"


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

static uint32_t wifi_arp_hits;
static uint32_t wifi_arp_misses;

static bool default_ap_mode;
static bool ap_mode;
static bool connected;
static bool wifi_shutdown;
static uint8_t disconnect_reason;

static uint8_t scan_backoff;
static uint8_t current_scan_backoff;

static uint8_t tx_power = WIFI_MAX_HW_TX_POWER;

static uint8_t wifi_power_mode;
static bool disable_modem_sleep;

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

    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_hits,                    0,   "wifi_arp_hits" },
    { CATBUS_TYPE_UINT32,        0, 0,                    &wifi_arp_misses,                  0,   "wifi_arp_misses" },

    { CATBUS_TYPE_UINT8,         0, KV_FLAGS_READ_ONLY,   &wifi_power_mode,                  0,   "wifi_power_mode" },
    { CATBUS_TYPE_BOOL,          0, KV_FLAGS_PERSIST,     &disable_modem_sleep,              0,   "wifi_disable_modem_sleep" },
};

// this lives in the wifi driver because it is the easiest place to get to hardware specific code
// this will init in safe mode
KV_SECTION_META kv_meta_t safe_mode_config_kv[] = {
    { CATBUS_TYPE_BOOL,          0, 0,                          0, cfg_i8_kv_handler,             "enable_brownout_restart" },
};


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_arp_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) );
PT_THREAD( brownout_restart_thread( pt_t *pt, void *state ) );

static void event_handler( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data );
static volatile bool scan_done;
static bool connect_done;

typedef struct{
    int sock;
    uint16_t lport;
} esp_conn_t;

static esp_conn_t esp_conn[WIFI_MAX_PORTS];

static char hostname[32];


#include "esp_partition.h"
 
static uint32_t coredump_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    const esp_partition_t *pt = esp_partition_find_first( ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump" );

    if( pt == NULL ){

        trace_printf( "Core dump partition not found.\r\n" );

        return 0;
    }

    uint32_t temp = 0xffffffff;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            esp_partition_read( pt, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            esp_partition_read( pt, 0, &temp, sizeof(temp) );

            // check if partition is erased
            if( temp == 0xffffffff ){

                len = 0;
            }
            else{

                len = pt->size;    
            }

            break;

        case FS_VFILE_OP_DELETE:
            esp_partition_erase_range( pt, 0, pt->size );

            len = 0;
            break;

        default:
            len = 0;

            break;
    }

    return len;
}




static void set_hostname( void ){

    // set up hostname
    char mac_str[16];
    memset( mac_str, 0, sizeof(mac_str) );
    snprintf( &mac_str[0], 3, "%02x", wifi_mac[3] );
    snprintf( &mac_str[2], 3, "%02x", wifi_mac[4] ); 
    snprintf( &mac_str[4], 3, "%02x", wifi_mac[5] );

    memset( hostname, 0, sizeof(hostname) );
    strlcpy( hostname, "Chromatron_", sizeof(hostname) );

    strlcat( hostname, mac_str, sizeof(hostname) );

    // esp_err_t err = tcpip_adapter_set_hostname( TCPIP_ADAPTER_IF_STA, hostname );

    esp_netif_t *esp_netif = NULL;
    esp_netif = esp_netif_next( esp_netif );
    esp_err_t err = esp_netif_set_hostname( esp_netif, hostname );

    if( err != ESP_OK ){

        log_v_error_P( PSTR("fail to set hostname: %d"), err );
    }
}


void hal_wifi_v_init( void ){

    log_v_info_P( PSTR("ESP32 SDK version:%s"), esp_get_idf_version() );

    // log reset reason for ESP32
    log_v_info_P( PSTR("ESP reset reason: %d"), esp_reset_reason() );

    if( cfg_b_get_boolean( __KV__enable_brownout_restart ) ){

        thread_t_create( 
             brownout_restart_thread,
             PSTR("brownout_restart"),
             0,
             0 );
    }

    fs_f_create_virtual( PSTR("coredump"), coredump_vfile_handler );



    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    esp_read_mac( wifi_mac, ESP_MAC_WIFI_STA );
    
    // hostname must be set before esp_wifi_init in IDF 4.x
    set_hostname();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register( WIFI_EVENT, ESP_EVENT_ANY_ID,            event_handler, NULL, NULL );
    esp_event_handler_instance_register( IP_EVENT,   IP_EVENT_STA_GOT_IP,         event_handler, NULL, NULL );
    

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .listen_interval = 3,
        },
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    
    uint64_t current_device_id = 0;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &current_device_id );
    uint64_t device_id = 0;
    memcpy( &device_id, wifi_mac, sizeof(wifi_mac) );

    if( current_device_id != device_id ){

        cfg_v_set( CFG_PARAM_DEVICE_ID, &device_id );
    }


    esp_wifi_set_mode( WIFI_MODE_NULL );

    // set tx power
    esp_wifi_set_max_tx_power( tx_power * 4 );

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

    thread_t_create_critical( 
                wifi_echo_thread,
                PSTR("wifi_echo"),
                0,
                0 );
}

static bool is_rx( void ){

    for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){
        
        if( esp_conn[i].lport == 0 ){

            continue;
        }

        int s = 0;
        int rc = ioctl( esp_conn[i].sock, FIONREAD, &s );

        if( rc < 0 ){

            trace_printf("is_rx err %d\n", rc);
        }

        if( s > 0 ){

            return TRUE;
        }
    }

    return FALSE;
}


PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !wifi_b_connected() );

        THREAD_YIELD( pt );
        THREAD_WAIT_WHILE( pt, !is_rx() );
        THREAD_WAIT_WHILE( pt, sock_b_rx_pending() ); // wait while sockets are busy

        for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){
            
            if( esp_conn[i].lport == 0 ){

                continue;
            }

            uint8_t buf[MAX_IP_PACKET_SIZE];
            struct sockaddr_in sourceAddr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(sourceAddr);

            int s = recvfrom( esp_conn[i].sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&sourceAddr, &socklen );

            if( s < 0 ){

                continue;
            }

            uint16_t len = s;

            // trace_printf("recv %d @ %d from 0x%x:%d\n", s, esp_conn[i].lport, sourceAddr.sin_addr.s_addr, htons(sourceAddr.sin_port));

            netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

            if( rx_netmsg < 0 ){

                log_v_debug_P( PSTR("rx udp alloc fail") );     

                wifi_udp_dropped++;

                break;
            }
            
            netmsg_state_t *nm_state = netmsg_vp_get_state( rx_netmsg );

            // set up addressing info
            nm_state->laddr.port       = esp_conn[i].lport;
            nm_state->raddr.port       = htons(sourceAddr.sin_port);
            nm_state->raddr.ipaddr.ip3 = sourceAddr.sin_addr.s_addr >> 0;
            nm_state->raddr.ipaddr.ip2 = sourceAddr.sin_addr.s_addr >> 8;
            nm_state->raddr.ipaddr.ip1 = sourceAddr.sin_addr.s_addr >> 16;
            nm_state->raddr.ipaddr.ip0 = sourceAddr.sin_addr.s_addr >> 24;

            // allocate data buffer
            nm_state->data_handle = mem2_h_alloc2( len, MEM_TYPE_SOCKET_BUFFER );

            if( nm_state->data_handle < 0 ){

                log_v_error_P( PSTR("rx udp no handle") );     

                netmsg_v_release( rx_netmsg );

                wifi_udp_dropped++;

                break;
            }      

            // we can get a fast ptr because we've already verified the handle
            uint8_t *data = mem2_vp_get_ptr_fast( nm_state->data_handle );
            memcpy( data, buf, len );

            wifi_udp_received++;

            netmsg_v_receive( rx_netmsg );

            #ifdef SOCK_SINGLE_BUF
            break; // we can only receive one at a time in single buf mode
            #endif
        }
    }   

PT_END( pt );
}

int8_t hal_wifi_i8_igmp_join( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    tcpip_adapter_ip_info_t info;
        memset( &info, 0, sizeof(info) );
        tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &info );
            
    ip4_addr_t ipgroup;
    ipgroup.addr = HTONL(ip_u32_to_int( mcast_ip ));

    return igmp_joingroup( &info.ip, &ipgroup );
}

int8_t hal_wifi_i8_igmp_leave( ip_addr4_t mcast_ip ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    tcpip_adapter_ip_info_t info;
        memset( &info, 0, sizeof(info) );
        tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &info );
            
    ip4_addr_t ipgroup;
    ipgroup.addr = HTONL(ip_u32_to_int( mcast_ip ));

    return igmp_leavegroup( &info.ip, &ipgroup );
}

esp_conn_t* get_conn( uint16_t lport ){

	for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

		if( esp_conn[i].lport == 0 ){

			continue;
		}

		if( esp_conn[i].lport == lport ){

			return &esp_conn[i];
		}
	}

	return 0;
}

void open_close_port( uint8_t protocol, uint16_t port, bool open ){

    if( protocol == IP_PROTO_UDP ){

        if( open ){

        	// trace_printf("Open port: %u\n", port);

        	int8_t index = -1;
        	for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

        		if( esp_conn[i].lport == 0 ){

        			index = i;
        			break;
        		}
        	}

        	if( index < 0 ){

        		ASSERT( FALSE );

        		return;
        	}

        	memset( &esp_conn[index], 0, sizeof(esp_conn[index]) );

            esp_conn[index].lport = port;
            esp_conn[index].sock  = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );	

            ASSERT( esp_conn[index].sock >= 0 );
            
            struct sockaddr_in destAddr;
            destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            destAddr.sin_family = AF_INET;
            destAddr.sin_port = htons(port);

            bind( esp_conn[index].sock, (struct sockaddr *)&destAddr, sizeof(destAddr) );
                
            // enable broadcast receive on socket.
            // note REUSEADDR also needs to be enabled for broadcasts to receive.
            int val = 1;
            setsockopt( esp_conn[index].sock, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &val, sizeof(val) );
        }
        else{

        	// trace_printf("Close port: %u\n", port);
			
			esp_conn_t* conn = get_conn( port );

			if( conn != 0 ){

				conn->lport = 0;

                shutdown( conn->sock, 0 );
                close( conn->sock );
			}
        }
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

	return connected && ap_mode;
}

#define WIFI_MICROAMPS_PS_NONE  80000
#define WIFI_MICROAMPS_PS_MODEM_MIN  25000
#define WIFI_VOLTS 3.3

#define WIFI_POWER_PS_NONE  ( WIFI_MICROAMPS_PS_NONE * WIFI_VOLTS )
#define WIFI_POWER_PS_MODEM_MIN  ( WIFI_MICROAMPS_PS_MODEM_MIN * WIFI_VOLTS )

uint32_t wifi_u32_get_power( void ){

    wifi_ps_type_t ps_mode = WIFI_PS_NONE;

    esp_wifi_get_ps( &ps_mode );

    if( ps_mode == WIFI_PS_NONE ){

        return WIFI_PS_NONE;
    }
    
    return WIFI_POWER_PS_MODEM_MIN;
}

void wifi_v_reset_scan_timeout( void ){

    scan_backoff = 0;
    current_scan_backoff = 0;
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

    // get esp conn
    esp_conn_t* conn = get_conn( netmsg_state->laddr.port );

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


    uint32_t dest_ip = 0;
    memcpy( &dest_ip, &netmsg_state->raddr.ipaddr, sizeof(dest_ip) );

    // trace_printf("sendto: 0x%lx %d.%d.%d.%d:%u\n", dest_ip, netmsg_state->raddr.ipaddr.ip3,netmsg_state->raddr.ipaddr.ip2,netmsg_state->raddr.ipaddr.ip1,netmsg_state->raddr.ipaddr.ip0, netmsg_state->raddr.port);

    struct sockaddr_in destAddr;
    destAddr.sin_addr.s_addr = dest_ip;
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(netmsg_state->raddr.port);

    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        if( !hal_arp_b_find( netmsg_state->raddr.ipaddr ) ){

            wifi_arp_misses++;
        }
        else{

            wifi_arp_hits++;
        }
    }

    int status = sendto( conn->sock, data, data_len, 0, (struct sockaddr *)&destAddr, sizeof(destAddr) );

    if( status < 0 ){

        log_v_debug_P( PSTR("msg failed %d from %d to %d.%d.%d.%d:%d err: %d"), 
            status,
            netmsg_state->laddr.port,
            netmsg_state->raddr.ipaddr.ip3,
            netmsg_state->raddr.ipaddr.ip2,
            netmsg_state->raddr.ipaddr.ip1,
            netmsg_state->raddr.ipaddr.ip0,
            netmsg_state->raddr.port,
            errno );

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

static void scan_cb( void ){

    scan_done = FALSE;

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    mem_handle_t h = mem2_h_alloc( ap_count * sizeof(wifi_ap_record_t) );

    if( h < 0 ){

        return;
    }

    wifi_ap_record_t *ap_info = mem2_vp_get_ptr_fast( h );

    memset(ap_info, 0, ap_count * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));

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

    for( uint32_t i = 0; i < ap_count; i++ ){

        // trace_printf( "%s %u %d", ap_info[i].ssid, ap_info[i].primary, ap_info[i].rssi );

        int8_t router = -1;

        if( strncmp( (char *)ap_info[i].ssid, ssid[0], WIFI_SSID_LEN ) == 0 ){

            router = 0;
        }
        else if( strncmp( (char *)ap_info[i].ssid, ssid[1], WIFI_SSID_LEN ) == 0 ){

            router = 1;
        }
        else if( strncmp( (char *)ap_info[i].ssid, ssid[2], WIFI_SSID_LEN ) == 0 ){

            router = 2;
        }
        else if( strncmp( (char *)ap_info[i].ssid, ssid[3], WIFI_SSID_LEN ) == 0 ){

            router = 3;
        }

        if( router < 0 ){

            continue;
        }

        if( ap_info[i].rssi > best_rssi ){

            best_bssid      = ap_info[i].bssid;
            best_rssi       = ap_info[i].rssi;
            best_channel    = ap_info[i].primary;
            best_router     = router;
        }
    }

    mem2_v_free( h );

    if( best_rssi == -127 ){

        // log_v_debug_P( PSTR("no routers found") );

        return;
    }

    // select router
    wifi_router = best_router;
    memcpy( wifi_bssid, best_bssid, sizeof(wifi_bssid) );
    wifi_channel = best_channel;
}


static void event_handler( void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data ){

    if( event_base == WIFI_EVENT ){

        if( event_id == WIFI_EVENT_STA_START ){

            trace_printf("WIFI_EVENT_STA_START\r\n");
        }
        else if( event_id == WIFI_EVENT_STA_STOP ){

            trace_printf("WIFI_EVENT_STA_STOP\r\n");
        }
        else if( event_id == WIFI_EVENT_STA_CONNECTED ){

            trace_printf("WIFI_EVENT_STA_CONNECTED\r\n");
        }
        else if( event_id == WIFI_EVENT_STA_DISCONNECTED ){

            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;

            trace_printf("WIFI_EVENT_STA_DISCONNECTED\r\n");
            disconnect_reason = event->reason;
            connected = FALSE;
            connect_done = TRUE;
        }
        else if( event_id == WIFI_EVENT_SCAN_DONE ){

            trace_printf("WIFI_EVENT_SCAN_DONE\r\n");
            scan_done = TRUE;
        }
    }
    else if( event_base == IP_EVENT ){

        if( event_id == IP_EVENT_STA_GOT_IP ){

            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ip_addr4_t *ip = (ip_addr4_t *)&event->ip_info.ip;

            trace_printf("wifi connected, IP: %d.%d.%d.%d\n", ip->ip3, ip->ip2, ip->ip1, ip->ip0);
            connect_done = TRUE;
            connected = TRUE;
        }
    }
}


static bool is_low_power_mode( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // safe mode, we're not going to do anything fancy

        return FALSE;
    }

    if( disable_modem_sleep ){

        return FALSE;
    }

    return TRUE;
}

static void apply_power_save_mode( void ){

    // set power state
    if( is_low_power_mode() ){

        wifi_power_mode = WIFI_PS_MIN_MODEM;
    }
    else{

        wifi_power_mode = WIFI_PS_NONE;
    }

    wifi_ps_type_t current_mode = 0;
    esp_wifi_get_ps( &current_mode );

    if( current_mode != wifi_power_mode ){

        esp_wifi_set_ps( wifi_power_mode );    
    }
}

PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // log_v_debug_P( PSTR("ARP table size: %d"), ARP_TABLE_SIZE );

    static uint16_t scan_timeout;

    connected = FALSE;
    wifi_rssi = -127;

    esp_wifi_disconnect();
    TMR_WAIT( pt, 100 );
    
    esp_wifi_stop();    

    THREAD_WAIT_WHILE( pt, wifi_shutdown );
    
    // check if we are connected
    while( !wifi_b_connected() && !wifi_shutdown ){

        wifi_rssi = -127;

        esp_wifi_disconnect();
        TMR_WAIT( pt, 100 ); // this delay seems to be important
        // without it, esp_wifi_stop might hang for as many as 5 seconds.

        esp_wifi_stop();    

        current_scan_backoff = scan_backoff;

        // scan backoff delay
        while( ( current_scan_backoff > 0 ) && !_wifi_b_ap_mode_enabled() ){

            current_scan_backoff--;
            TMR_WAIT( pt, 1000 );
        }
        
        ap_mode = _wifi_b_ap_mode_enabled();

        // check if no APs are configured
        if( !is_ssid_configured() ){

            // no SSIDs are configured

            // switch to AP mode
            ap_mode = TRUE;
        }
 
        // station mode
        if( !ap_mode ){
station_mode:          

            connected = FALSE;  
            connect_done = FALSE;
            disconnect_reason = 0;
        
            esp_wifi_set_mode( WIFI_MODE_STA );

            esp_wifi_start();

            apply_power_save_mode();
            
            // check if we can try a fast connect with the last connected router
            if( wifi_router >= 0 ){

                log_v_debug_P( PSTR("Fast connect...") );
            }
            else{

                // scan first
                wifi_router = -1;
                // log_v_debug_P( PSTR("Scanning...") );
                
                scan_done = FALSE;
                    
                esp_err_t err = esp_wifi_scan_start(NULL, FALSE);
                if( err != 0 ){

                	log_v_error_P( PSTR("Scan error: %d"), err );

                    esp_wifi_scan_stop();

                    goto end;
                }

                scan_timeout = 500;
                while( ( scan_done == FALSE ) && ( scan_timeout > 0 ) ){

                    scan_timeout--;

                    TMR_WAIT( pt, 50 );
                }

                if( scan_done ){

                    scan_cb();
                }
                else{

                    log_v_error_P( PSTR("scan timeout!") );

                    // call the scan callback anyway.
                    // the ESP32 seems to sometimes fail to signal scan completion so we timeout.
                    // or maybe it fails to scan entirely? can't tell so far.
                    scan_cb();
                }

                esp_wifi_scan_stop();

                if( wifi_router < 0 ){

                    // router not found

                    if( scan_backoff == 0 ){

                        scan_backoff = 1;
                    }
                    else if( scan_backoff < 64 ){

                        scan_backoff *= 2;
                    }
                    else if( scan_backoff < 192 ){

                        scan_backoff += 64;
                    }

                    goto end;
                }

                log_v_debug_P( PSTR("Connecting...") );
            }

            connect_done = FALSE;

            // set_hostname();
            
            wifi_config_t wifi_config = {0};
            wifi_config.sta.bssid_set = 1;
            memcpy( wifi_config.sta.bssid, wifi_bssid, sizeof(wifi_config.sta.bssid) );
            wifi_config.sta.channel = wifi_channel;
            wifi_v_get_ssid( (char *)wifi_config.sta.ssid );
            get_pass( wifi_router, (char *)wifi_config.sta.password );

            log_v_debug_P( PSTR("BSSID: %02x:%02x:%02x:%02x:%02x:%02x ch: %d"), 
                wifi_bssid[0],
                wifi_bssid[1],
                wifi_bssid[2],
                wifi_bssid[3],
                wifi_bssid[4],
                wifi_bssid[5],
                wifi_channel
            );

            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
            
            esp_wifi_connect();

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( !connect_done ) &&
                                   ( thread_b_alarm_set() ) );

           	// check if we're connected
           	if( connect_done && connected ){

           		// get IP info
           		tcpip_adapter_ip_info_t info;
           		memset( &info, 0, sizeof(info) );
           		tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &info );

				cfg_v_set( CFG_PARAM_IP_ADDRESS, &info.ip );
			    cfg_v_set( CFG_PARAM_IP_SUBNET_MASK, &info.netmask );
                cfg_v_set( CFG_PARAM_INTERNET_GATEWAY, &info.gw );

                tcpip_adapter_dns_info_t dns_info;
                tcpip_adapter_get_dns_info( TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_MAIN, &dns_info );
			    cfg_v_set( CFG_PARAM_DNS_SERVER, &dns_info.ip );

                // get RSSI
                wifi_ap_record_t wifi_info;
                if( esp_wifi_sta_get_ap_info( &wifi_info ) == 0 ){

                    wifi_rssi = wifi_info.rssi;
                }
           	}
            else{

                // connection failed
                wifi_router = -1;
                wifi_channel = -1;
                memset( wifi_bssid, 0, sizeof(wifi_bssid) );

                log_v_debug_P( PSTR("Connection failed: %d"), disconnect_reason );
            }

            kv_i8_persist( __KV__wifi_channel );
            kv_i8_persist( __KV__wifi_bssid );
            kv_i8_persist( __KV__wifi_router );
        }
        // AP mode
        else{

            // ESP32 soft AP.... basically doesn't work.
            // provide a safe mode bail out.

            if( sys_u8_get_mode() == SYS_MODE_SAFE ){

                goto station_mode;
            }

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

                strncat( ap_ssid, mac, sizeof(ap_ssid) - strnlen( ap_ssid, sizeof(ap_ssid) ) );

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

            esp_wifi_disconnect();

            log_v_debug_P( PSTR("Starting AP: %s"), ap_ssid );

            esp_err_t err;

            // check if wifi settings were present
            if( ap_ssid[0] != 0 ){     

                wifi_config_t ap_config = {
                    .ap = {
                        .channel = 0,
                        .authmode = WIFI_AUTH_WPA2_PSK,
                        .ssid_hidden = 0,
                        .max_connection = 8,
                        .beacon_interval = 100
                    }
                };

                memcpy( ap_config.ap.ssid, ap_ssid, sizeof(ap_ssid) );
                memcpy( ap_config.ap.password, ap_pass, sizeof(ap_pass) );
                ap_config.ap.ssid_len = strlen( ap_ssid );

                err = esp_wifi_set_mode( WIFI_MODE_AP );

                if( err != ESP_OK ){

                    log_v_error_P( PSTR("Wifi error: %d"), err );
                    goto station_mode;
                }

                err = esp_wifi_set_config( WIFI_IF_AP, &ap_config );

                if( err != ESP_OK ){

                    log_v_error_P( PSTR("Wifi error: %d"), err );
                    goto station_mode;
                }

                // set bandwidth to 20 MHz
                // this does not work.  ESP-IDF bug.
                err = esp_wifi_set_bandwidth( WIFI_IF_AP, WIFI_BW_HT20 );

                if( err != ESP_OK ){

                    log_v_error_P( PSTR("Wifi error: %d"), err );
                    goto station_mode;
                }

                if( esp_wifi_start() != ESP_OK ){

                    log_v_warn_P( PSTR("AP mode failed") );

                    goto station_mode;
                }

                connected = TRUE;
            }
        }

end:
        if( !wifi_b_connected() ){

            TMR_WAIT( pt, 500 );
        }
    }

    if( wifi_b_connected() ){

        scan_backoff = 0;

        if( !_wifi_b_ap_mode_enabled() ){

            wifi_connects++;

            char ssid[WIFI_SSID_LEN];
            wifi_v_get_ssid( ssid );
            log_v_debug_P( PSTR("Wifi connected to: %s ch: %d"), ssid, wifi_channel );
        }
        else{

            log_v_debug_P( PSTR("Wifi soft AP up") );    
        }
    }

    THREAD_WAIT_WHILE( pt, wifi_b_connected() && !wifi_shutdown );
    
    log_v_debug_P( PSTR("Wifi disconnected. Last RSSI: %d ch: %d"), wifi_rssi, wifi_channel );

    THREAD_RESTART( pt );

PT_END( pt );
}

PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        if( wifi_b_connected() ){

            wifi_uptime++;
            connected = TRUE;

            wifi_ap_record_t wifi_info;
            if( esp_wifi_sta_get_ap_info( &wifi_info ) == 0 ){

                wifi_rssi = wifi_info.rssi;
            }

            apply_power_save_mode();
        }
        else{

            wifi_uptime = 0;
            connected = FALSE;
            wifi_rssi = -127;
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

PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static socket_t sock;

    sock = sock_s_create( SOS_SOCK_DGRAM );
    sock_v_bind( sock, 7 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_sendto( sock, sock_vp_get_data( sock ), sock_i16_get_bytes_read( sock ), 0 ) >= 0 ){
            
        }
    }

PT_END( pt );
}


#define BROWNOUT_RESTART_TIME 60000

PT_THREAD( brownout_restart_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        THREAD_EXIT( pt );
    }

    if( esp_reset_reason() == ESP_RST_BROWNOUT ){

        log_v_info_P( PSTR("Restart from brownout in %d seconds"), BROWNOUT_RESTART_TIME / 1000 );
    }
    else if( esp_reset_reason() == ESP_RST_PANIC ){

        log_v_info_P( PSTR("Restart from panic in %d seconds"), BROWNOUT_RESTART_TIME / 1000 );
    }
    else{

        THREAD_EXIT( pt );    
    }

    
    TMR_WAIT( pt, BROWNOUT_RESTART_TIME );

    log_v_info_P( PSTR("Restart from safe mode...") );

    sys_v_reboot_delay( SYS_MODE_NORMAL );

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

