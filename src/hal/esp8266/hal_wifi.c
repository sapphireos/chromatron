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

#include "test_ssid.h"

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
    { SAPPHIRE_TYPE_KEY128,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_md5" },
    { SAPPHIRE_TYPE_UINT32,        0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_fw_len" },

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

static list_t conn_list;

void wifi_v_init( void ){
	
	trace_printf( "wifi_v_init\r\n" );

	wifi_station_disconnect();

	// set opmode without saving to flash (since we always set this)
	wifi_set_opmode_current( STATION_MODE );

	thread_t_create_critical( 
                 wifi_connection_manager_thread,
                 PSTR("wifi_connection_manager"),
                 0,
                 0 );

	wifi_get_macaddr( 0, wifi_mac );

	char *ssid = SSID;
    char *password = PASSWORD;

    cfg_v_set( CFG_PARAM_WIFI_SSID, ssid );
    cfg_v_set( CFG_PARAM_WIFI_PASSWORD, password );

    list_v_init( &conn_list );
    
 //    struct station_config config = {0};
 //    strcpy( &config.ssid, ssid );
 //    strcpy( &config.password, password );

 //    wifi_station_set_config( &config );

 //    wifi_station_connect();
}

static struct espconn esp_conn[8];
static esp_udp udp_conn[8];

void udp_recv_callback( void *arg, char *pdata, unsigned short len ){

	struct espconn* conn = (struct espconn*)arg;

	remot_info *remote_info = 0;
   
    espconn_get_connection_info( conn, &remote_info, 0 );

	trace_printf("RX: %d bytes from %d.%d.%d.%d:%u\n", 
		len, 
		remote_info->remote_ip[0],
		remote_info->remote_ip[1],
		remote_info->remote_ip[2],
		remote_info->remote_ip[3],
		remote_info->remote_port);
}


void open_close_port( uint8_t protocol, uint16_t port, bool open ){

    if( protocol == IP_PROTO_UDP ){

        if( open ){

        	trace_printf("Open port: %u\n", port);

        	int8_t index = -1;
        	for( uint8_t i = 0; i < cnt_of_array(esp_conn); i++ ){

        		if( esp_conn[i].type == ESPCONN_INVALID ){

        			index = i;
        			break;
        		}
        	}

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

        	trace_printf("Close port: %u\n", port);
			
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

	return 0;
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

}

int8_t wifi_i8_get_status( void ){

}

uint32_t wifi_u32_get_received( void ){

}

int8_t wifi_i8_send_udp( netmsg_t netmsg ){


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

		int8_t router = has_ssid( info->ssid );
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
            
            struct scan_config scan_config = {
            	.ssid = 0,
            	.bssid = 0,
            	.channel = 0,
            	.show_hidden = 0,
            	.scan_type = 0,
            	.scan_time = 0,
            };

            wifi_station_disconnect();
            if( wifi_station_scan( &scan_config, scan_cb ) != TRUE ){

            	log_v_error_P( PSTR("Scan error") );
            }

            scan_timeout = 200;
            while( ( wifi_router < 0 ) && ( scan_timeout > 0 ) ){

                scan_timeout--;

                TMR_WAIT( pt, 50 );
            }

            if( wifi_router < 0 ){

                goto end;
            }

            log_v_debug_P( PSTR("Connecting...") );
            
			struct station_config sta_config;
			memset( &sta_config, 0, sizeof(sta_config) );
			memcpy( sta_config.bssid, wifi_bssid, sizeof(sta_config.bssid) );
			sta_config.bssid_set = 1;
			sta_config.channel = wifi_channel;
			wifi_v_get_ssid( sta_config.ssid );
			get_pass( wifi_router, sta_config.password );

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

			    uint32_t dns_ip = dns_getserver( 0 );

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

#else

bool wifi_b_connected( void ){

	return FALSE;
}

bool wifi_b_attached( void ){

	return TRUE;
}

#endif

