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

#ifdef DEFAULT_WIFI
#include "test_ssid.h"
#endif

#include "espconn.h"

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
static bool default_ap_mode;

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
    { SAPPHIRE_TYPE_MAC48,         0, 0, &wifi_mac,                         0,   "wifi_mac" },
    { SAPPHIRE_TYPE_INT8,          0, 0, &wifi_rssi,                        0,   "wifi_rssi" },
    { SAPPHIRE_TYPE_INT8,          0, 0, &wifi_channel,                     0,   "wifi_channel" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_uptime,                      0,   "wifi_uptime" },
    { SAPPHIRE_TYPE_UINT8,         0, 0, &wifi_connects,                    0,   "wifi_connects" },

    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_udp_received,                0,   "wifi_udp_received" },
    { SAPPHIRE_TYPE_UINT32,        0, 0, &wifi_udp_sent,                    0,   "wifi_udp_sent" },
};


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_status_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_arp_thread( pt_t *pt, void *state ) );
PT_THREAD( wifi_echo_thread( pt_t *pt, void *state ) );

static struct espconn esp_conn[WIFI_MAX_PORTS];
static esp_udp udp_conn[WIFI_MAX_PORTS];

static list_t conn_list;
static list_t rx_list;


void wifi_v_init( void ){
	
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

    thread_t_create_critical( 
                wifi_echo_thread,
                PSTR("wifi_echo"),
                0,
                0 );

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

    // set tx power
    system_phy_set_max_tpw( tx_power * 4 );

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

    netmsg_t rx_netmsg = netmsg_nm_create( NETMSG_TYPE_UDP );

    if( rx_netmsg < 0 ){

        log_v_debug_P( PSTR("rx udp alloc fail") );     

        return;
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

        return;
    }      

    // we can get a fast ptr because we've already verified the handle
    uint8_t *data = mem2_vp_get_ptr_fast( state->data_handle );
    memcpy( data, pdata, len );

    // post to rx Q
    if( list_u8_count( &rx_list ) >= WIFI_MAX_RX_NETMSGS ){

        log_v_warn_P( PSTR("rx q full") );     

        netmsg_v_release( rx_netmsg );

        return;
    }

    list_v_insert_head( &rx_list, rx_netmsg );
}



PT_THREAD( wifi_rx_process_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        THREAD_WAIT_WHILE( pt, list_u8_count( &rx_list ) == 0 );
        THREAD_WAIT_WHILE( pt, sock_b_rx_pending() ); // wait while sockets are busy

        netmsg_t rx_netmsg = list_ln_remove_tail( &rx_list );
        netmsg_v_receive( rx_netmsg );
    }   

PT_END( pt );
}


struct espconn* get_conn( uint16_t lport ){

	for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

		if( esp_conn[i].type == ESPCONN_INVALID ){

			continue;
		}

		if( esp_conn[i].proto.udp->local_port == lport ){

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

            esp_conn[index].type 			= ESPCONN_UDP;
			esp_conn[index].state 			= ESPCONN_NONE; 
			esp_conn[index].proto.udp 		= &udp_conn[index];
			esp_conn[index].recv_callback 	= udp_recv_callback;
			esp_conn[index].sent_callback 	= 0;
			esp_conn[index].link_cnt 		= 0;
			esp_conn[index].reverse 		= 0;

			udp_conn[index].remote_port 	= 0;
			udp_conn[index].local_port 		= port;
 
			espconn_create( &esp_conn[index] );
        }
        else{

        	// trace_printf("Close port: %u\n", port);
			
			struct espconn* conn = get_conn( port );

			if( conn != 0 ){

				espconn_delete( conn );
				conn->type = ESPCONN_INVALID;	
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

uint32_t wifi_u32_get_received( void ){

    return 0;
}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){

	int8_t status = 0;

    if( !wifi_b_connected() ){

        return NETMSG_TX_ERR_RELEASE;
    }

    netmsg_state_t *netmsg_state = netmsg_vp_get_state( netmsg );

    ASSERT( netmsg_state->type == NETMSG_TYPE_UDP );

    uint16_t data_len = 0;

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

    // get esp conn
    struct espconn* conn = get_conn( netmsg_state->laddr.port );

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

    // trace_printf("sendto: %d.%d.%d.%d:%u\n", netmsg_state->raddr.ipaddr.ip3,netmsg_state->raddr.ipaddr.ip2,netmsg_state->raddr.ipaddr.ip1,netmsg_state->raddr.ipaddr.ip0, netmsg_state->raddr.port);
    if( espconn_sendto( conn, data, data_len ) != 0 ){

        log_v_debug_P( PSTR("msg failed") );

        return NETMSG_TX_ERR_RELEASE;   
    }
    
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
	   	
	   	kv_i8_get( __KV__wifi_ssid2, pass, WIFI_PASS_LEN );
	}
	else if( router == 2 ){
	   	
	   	kv_i8_get( __KV__wifi_ssid3, pass, WIFI_PASS_LEN );
	}
	else if( router == 3 ){
	   	
	   	kv_i8_get( __KV__wifi_ssid4, pass, WIFI_PASS_LEN );
	}
}

void scan_cb( void *result, STATUS status ){

	struct bss_info* info = (struct bss_info*)result;

	trace_printf("scan: %d\n", status);

	int8_t best_rssi = -127;
	uint8_t best_channel = 0;
	uint8_t *best_bssid = 0;
	int8_t best_router = -1;

	while( info != 0 ){

		trace_printf("%s %u %d\n", info->ssid, info->channel, info->rssi);

		int8_t router = has_ssid( (char *)info->ssid );
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

	trace_printf("router: %d\n", best_router);

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


PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

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
            // scan first
            wifi_router = -1;
            log_v_debug_P( PSTR("Scanning...") );
            
            struct scan_config scan_config;
            memset( &scan_config, 0, sizeof(scan_config) );
            
            // light LED while scanning since the CPU will freeze
            io_v_set_esp_led( 1 );

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

                goto end;
            }

            log_v_debug_P( PSTR("Connecting...") );
            
			struct station_config sta_config;
			memset( &sta_config, 0, sizeof(sta_config) );
			memcpy( sta_config.bssid, wifi_bssid, sizeof(sta_config.bssid) );
			sta_config.bssid_set = 1;
			sta_config.channel = wifi_channel;
			wifi_v_get_ssid( (char *)sta_config.ssid );
			get_pass( wifi_router, (char *)sta_config.password );

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

                // wifi_i8_send_msg( WIFI_DATA_ID_AP_MODE, (uint8_t *)&ap_msg, sizeof(ap_msg) );



                thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
                THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                       ( thread_b_alarm_set() ) );

                if( !wifi_b_connected() ){

                    log_v_warn_P( PSTR("AP mode failed.") );

                    goto station_mode;
                }
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

        THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

        if( wifi_station_get_connect_status() == STATION_GOT_IP ){

            wifi_uptime++;
            connected = TRUE;
        }
        else{

            wifi_uptime = 0;
            connected = FALSE;
        }

        wifi_rssi = wifi_station_get_rssi();
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

            TMR_WAIT( pt, 16000 );

            hal_arp_v_gratuitous_arp();
        }
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

