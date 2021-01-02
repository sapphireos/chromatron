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
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#ifdef ENABLE_WIFI

static uint8_t wifi_mac[6];
static int8_t wifi_rssi;
static uint8_t wifi_bssid[6];
static int8_t wifi_router;
static int8_t wifi_channel;
static uint32_t wifi_uptime;
static uint8_t wifi_connects;
static uint32_t wifi_udp_received;
static uint32_t wifi_udp_sent;
static uint32_t wifi_udp_dropped;
static bool default_ap_mode;

static uint32_t wifi_arp_hits;
static uint32_t wifi_arp_misses;

static bool connected;

static uint8_t tx_power = WIFI_MAX_HW_TX_POWER;

KV_SECTION_META kv_meta_t wifi_cfg_kv[] = {
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ssid" },
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_password" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid2" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password2" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid3" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password3" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_ssid4" },
    { SAPPHIRE_TYPE_STRING32,      0, KV_FLAGS_PERSIST,           0,                  0,                   "wifi_password4" },
    { SAPPHIRE_TYPE_BOOL,          0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_enable_ap" },    
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_ssid" },
    { SAPPHIRE_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ap_password" },

    { SAPPHIRE_TYPE_UINT8,         0, KV_FLAGS_PERSIST,           &tx_power,          0,                   "wifi_tx_power" },
};

KV_SECTION_META kv_meta_t wifi_info_kv[] = {
    { SAPPHIRE_TYPE_MAC48,         0, KV_FLAGS_READ_ONLY,   &wifi_mac,                         0,   "wifi_mac" },
    { SAPPHIRE_TYPE_INT8,          0, KV_FLAGS_READ_ONLY,   &wifi_rssi,                        0,   "wifi_rssi" },
    { SAPPHIRE_TYPE_UINT32,        0, KV_FLAGS_READ_ONLY,   &wifi_uptime,                      0,   "wifi_uptime" },
    { SAPPHIRE_TYPE_UINT8,         0, KV_FLAGS_READ_ONLY,   &wifi_connects,                    0,   "wifi_connects" },

    { SAPPHIRE_TYPE_INT8,          0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_channel,  0,   "wifi_channel" },
    { SAPPHIRE_TYPE_MAC48,         0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_bssid,    0,   "wifi_bssid" },
    { SAPPHIRE_TYPE_INT8,          0, KV_FLAGS_READ_ONLY | KV_FLAGS_PERSIST,   &wifi_router,   0,   "wifi_router" },

    { SAPPHIRE_TYPE_UINT32,        0, 0,                    &wifi_udp_received,                0,   "wifi_udp_received" },
    { SAPPHIRE_TYPE_UINT32,        0, 0,                    &wifi_udp_sent,                    0,   "wifi_udp_sent" },
    { SAPPHIRE_TYPE_UINT32,        0, 0,                    &wifi_udp_dropped,                 0,   "wifi_udp_dropped" },

    { SAPPHIRE_TYPE_UINT32,        0, 0,                    &wifi_arp_hits,                    0,   "wifi_arp_hits" },
    { SAPPHIRE_TYPE_UINT32,        0, 0,                    &wifi_arp_misses,                  0,   "wifi_arp_misses" },
};


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_arp_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) );

// debug: 
static const char *TAG = "wifi station";

static esp_err_t event_handler(void *ctx, system_event_t *event);
static bool scan_done;
static bool connect_done;

typedef struct{
    int sock;
    uint16_t lport;
} esp_conn_t;

static esp_conn_t esp_conn[WIFI_MAX_PORTS];



void wifi_v_init( void ){

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .listen_interval = 3,
        },
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
	
    esp_read_mac( wifi_mac, ESP_MAC_WIFI_STA );

    uint64_t current_device_id = 0;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &current_device_id );
    uint64_t device_id = 0;
    memcpy( &device_id, wifi_mac, sizeof(wifi_mac) );

    if( current_device_id != device_id ){

        cfg_v_set( CFG_PARAM_DEVICE_ID, &device_id );
    }


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    // set tx power
    esp_wifi_set_max_tx_power( tx_power * 4 );

    // set power state
    // esp_wifi_set_ps( WIFI_PS_NONE ); // disable 
    // esp_wifi_set_ps( WIFI_PS_MIN_MODEM );
    esp_wifi_set_ps( WIFI_PS_MAX_MODEM );

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

            trace_printf("err %d\n", rc);
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

bool wifi_b_ap_mode( void ){

	return FALSE;
}

bool wifi_b_shutdown( void ){

    return FALSE;
}

int8_t wifi_i8_get_status( void ){

    return 0;
}

int8_t wifi_i8_get_channel( void ){

    return wifi_channel;
}

uint32_t wifi_u32_get_received( void ){

    return 0;
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

        log_v_debug_P( PSTR("msg failed %d"), status );

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


static int8_t has_ssid( char *check_ssid ){

	char ssid[WIFI_SSID_LEN];
	
	memset( ssid, 0, sizeof(ssid) );
	cfg_i8_get( CFG_PARAM_WIFI_SSID, ssid );

   	if( strncmp( check_ssid, ssid, WIFI_SSID_LEN ) == 0 ){

   		return 0;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid2, ssid, sizeof(ssid) );
       
   	if( strncmp( check_ssid, ssid, WIFI_SSID_LEN ) == 0 ){

   		return 1;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid3, ssid, sizeof(ssid) );
       
   	if( strncmp( check_ssid, ssid, WIFI_SSID_LEN ) == 0 ){

   		return 2;
   	}

   	memset( ssid, 0, sizeof(ssid) );
   	kv_i8_get( __KV__wifi_ssid4, ssid, sizeof(ssid) );
       
   	if( strncmp( check_ssid, ssid, WIFI_SSID_LEN ) == 0 ){

   		return 3;
   	}

   	return -1;
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


#define SCAN_LIST_SIZE      32

static void scan_cb( void ){

    scan_done = FALSE;

    uint16_t ap_record_list_size = SCAN_LIST_SIZE;

    mem_handle_t h = mem2_h_alloc( ap_record_list_size * sizeof(wifi_ap_record_t) );

    if( h < 0 ){

        return;
    }

    wifi_ap_record_t *ap_info = mem2_vp_get_ptr_fast( h );
    uint16_t ap_count = 0;
    memset(ap_info, 0, ap_record_list_size * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_record_list_size, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    int8_t best_rssi = -127;
    uint8_t best_channel = 0;
    uint8_t *best_bssid = 0;
    int8_t best_router = -1;

    for( uint32_t i = 0; i < ap_count; i++ ){

        trace_printf("%s %u %d\n", ap_info[i].ssid, ap_info[i].primary, ap_info[i].rssi);

        int8_t router = has_ssid( (char *)ap_info[i].ssid );
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

        return;
    }

    trace_printf("router: %d\n", best_router);

    // select router
    wifi_router = best_router;
    memcpy( wifi_bssid, best_bssid, sizeof(wifi_bssid) );
    wifi_channel = best_channel;
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        connect_done = TRUE;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            connected = FALSE;
            ESP_LOGI(TAG,"disconnect\n");
            break;
        }
    case SYSTEM_EVENT_SCAN_DONE:
        scan_done = TRUE;
        break;
    default:
        break;
    }
    return ESP_OK;
}


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    log_v_debug_P( PSTR("ARP table size: %d"), ARP_TABLE_SIZE );

    static uint8_t scan_timeout;

    connected = FALSE;
    
    // check if we are connected
    while( !wifi_b_connected() ){

        wifi_rssi = -127;
        
        bool ap_mode = _wifi_b_ap_mode_enabled();

        // check if no APs are configured
        if( !is_ssid_configured() ){

            // no SSIDs are configured

            // switch to AP mode
            ap_mode = TRUE;
        }
 
        // station mode
        if( !ap_mode ){
station_mode:            

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

            esp_wifi_disconnect();
            ESP_ERROR_CHECK(esp_wifi_start());

            // check if we can try a fast connect with the last connected router
            if( wifi_router >= 0 ){

                log_v_debug_P( PSTR("Fast connect...") );
            }
            else{

                // scan first
                wifi_router = -1;
                log_v_debug_P( PSTR("Scanning...") );
                
                scan_done = FALSE;
                
                if( esp_wifi_scan_start(NULL, FALSE) != 0 ){

                	log_v_error_P( PSTR("Scan error") );
                }

                scan_timeout = 200;
                while( ( scan_done == FALSE ) && ( scan_timeout > 0 ) ){

                    scan_timeout--;

                    TMR_WAIT( pt, 50 );
                }

                if( scan_done ){

                    scan_cb();
                }

                if( wifi_router < 0 ){

                    goto end;
                }

                log_v_debug_P( PSTR("Connecting...") );
            }

            connect_done = FALSE;
            
            wifi_config_t wifi_config = {0};
            wifi_config.sta.bssid_set = 1;
            memcpy( wifi_config.sta.bssid, wifi_bssid, sizeof(wifi_config.sta.bssid) );
            wifi_config.sta.channel = wifi_channel;
            wifi_v_get_ssid( (char *)wifi_config.sta.ssid );
            get_pass( wifi_router, (char *)wifi_config.sta.password );

            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
            
            esp_wifi_connect();

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( !connect_done ) &&
                                   ( thread_b_alarm_set() ) );

           	// check if we're connected
           	if( connect_done ){

                connected = TRUE;

           		// get IP info
           		tcpip_adapter_ip_info_t info;
           		memset( &info, 0, sizeof(info) );
           		tcpip_adapter_get_ip_info( TCPIP_ADAPTER_IF_STA, &info );

				cfg_v_set( CFG_PARAM_IP_ADDRESS, &info.ip );
			    cfg_v_set( CFG_PARAM_IP_SUBNET_MASK, &info.netmask );
                cfg_v_set( CFG_PARAM_INTERNET_GATEWAY, &info.gw );

                tcpip_adapter_dns_info_t dns_info;
                tcpip_adapter_get_dns_info( TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns_info );
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

                log_v_debug_P( PSTR("Connection failed") );
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

            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

            esp_wifi_disconnect();

            log_v_debug_P( PSTR("Starting AP: %s"), ap_ssid );

            // set bandwidth to 20 MHz
            esp_wifi_set_bandwidth( WIFI_IF_AP, WIFI_BW_HT20 );

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
 
                ESP_ERROR_CHECK(esp_wifi_set_config( WIFI_IF_AP, &ap_config ));

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

#else

bool wifi_b_connected( void ){

	return FALSE;
}

bool wifi_b_attached( void ){

	return TRUE;
}

#endif

