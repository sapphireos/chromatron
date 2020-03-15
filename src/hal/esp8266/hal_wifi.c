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

#include "test_ssid.h"

#ifdef ENABLE_WIFI

static uint8_t wifi_mac[6];
static int8_t wifi_rssi;
static int8_t wifi_router;
static int8_t wifi_channel;
static uint32_t wifi_uptime;
static uint8_t wifi_connects;
static uint32_t wifi_udp_received;
static uint32_t wifi_udp_sent;
static bool default_ap_mode;

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


	// char *ssid = SSID;
 //    char *password = PASSWORD;
    
 //    struct station_config config = {0};
 //    strcpy( &config.ssid, ssid );
 //    strcpy( &config.password, password );

 //    wifi_station_set_config( &config );

 //    wifi_station_connect();
}

void wifi_v_shutdown( void ){


}

bool wifi_b_connected( void ){

	return FALSE;
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

PT_THREAD( wifi_connection_manager_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint8_t scan_timeout;
    
    // check if we are connected
    while( !wifi_b_connected() ){

        wifi_rssi = -127;
        
        THREAD_WAIT_WHILE( pt, !wifi_b_attached() );

        bool ap_mode = _wifi_b_ap_mode_enabled();

        // get SSIDs from database
        wifi_msg_connect_t msg;
        memset( &msg, 0, sizeof(msg) );

        cfg_i8_get( CFG_PARAM_WIFI_SSID, msg.ssid[0] );
        cfg_i8_get( CFG_PARAM_WIFI_PASSWORD, msg.pass[0] );
        
        kv_i8_get( __KV__wifi_ssid2, msg.ssid[1], sizeof(msg.ssid[1]) );
        kv_i8_get( __KV__wifi_password2, msg.pass[1], sizeof(msg.pass[1]) );  

        kv_i8_get( __KV__wifi_ssid3, msg.ssid[2], sizeof(msg.ssid[2]) );
        kv_i8_get( __KV__wifi_password3, msg.pass[2], sizeof(msg.pass[2]) );  

        kv_i8_get( __KV__wifi_ssid4, msg.ssid[3], sizeof(msg.ssid[3]) );
        kv_i8_get( __KV__wifi_password4, msg.pass[3], sizeof(msg.pass[3]) );  

        // check if no APs are configured
        if( ( msg.ssid[0][0] == 0 ) &&
            ( msg.ssid[1][0] == 0 ) &&
            ( msg.ssid[2][0] == 0 ) &&
            ( msg.ssid[3][0] == 0 ) ){

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
            // wifi_i8_send_msg( WIFI_DATA_ID_SCAN, (uint8_t *)&msg, sizeof(msg) );

            scan_timeout = 200;
            while( ( wifi_router < 0 ) && ( scan_timeout > 0 ) ){

                scan_timeout--;

                TMR_WAIT( pt, 50 );

                // get_info();
            }

            if( wifi_router < 0 ){

                goto end;
            }

            log_v_debug_P( PSTR("Connecting...") );
            
            
            // wifi_i8_send_msg( WIFI_DATA_ID_CONNECT, 0, 0 );


            thread_v_set_alarm( tmr_u32_get_system_time_ms() + WIFI_CONNECT_TIMEOUT );    
            THREAD_WAIT_WHILE( pt, ( !wifi_b_connected() ) &&
                                   ( thread_b_alarm_set() ) );
        }
        // AP mode
        else{
            // should have gotten the MAC by now
            ASSERT( wifi_mac[0] != 0 );

            // AP mode
            wifi_msg_ap_connect_t ap_msg;

            cfg_i8_get( CFG_PARAM_WIFI_AP_SSID, ap_msg.ssid );
            cfg_i8_get( CFG_PARAM_WIFI_AP_PASSWORD, ap_msg.pass );

            // check if AP mode SSID is set:
            if( ( ap_msg.ssid[0] == 0 ) || ( default_ap_mode ) ){

                // set up default AP
                memset( ap_msg.ssid, 0, sizeof(ap_msg.ssid) );
                memset( ap_msg.pass, 0, sizeof(ap_msg.pass) );

                strlcpy_P( ap_msg.ssid, PSTR("Chromatron_"), sizeof(ap_msg.ssid) );

                char mac[16];
                memset( mac, 0, sizeof(mac) );
                snprintf_P( &mac[0], 3, PSTR("%02x"), wifi_mac[3] );
                snprintf_P( &mac[2], 3, PSTR("%02x"), wifi_mac[4] ); 
                snprintf_P( &mac[4], 3, PSTR("%02x"), wifi_mac[5] );

                strncat( ap_msg.ssid, mac, sizeof(ap_msg.ssid) );

                strlcpy_P( ap_msg.pass, PSTR("12345678"), sizeof(ap_msg.pass) );

                default_ap_mode = TRUE;
            }
            else if( strnlen( ap_msg.pass, sizeof(ap_msg.pass) ) < WIFI_AP_MIN_PASS_LEN ){

                log_v_warn_P( PSTR("AP mode password must be at least 8 characters.") );

                // disable ap mode
                bool temp = FALSE;
                cfg_v_set( CFG_PARAM_WIFI_ENABLE_AP, &temp );

                goto end;
            }

            log_v_debug_P( PSTR("Starting AP: %s"), ap_msg.ssid );

            // check if wifi settings were present
            if( ap_msg.ssid[0] != 0 ){        

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



