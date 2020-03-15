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



void wifi_v_init( void ){
	
	trace_printf( "wifi_v_init\r\n" );

	wifi_station_disconnect();

	// set opmode without saving to flash (since we always set this)
	wifi_set_opmode_current( STATION_MODE );

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

	return FALSE;
}

int8_t wifi_i8_rssi( void ){

	return 0;
}

void wifi_v_get_ssid( char ssid[WIFI_SSID_LEN] ){

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

#else

bool wifi_b_connected( void ){

	return FALSE;
}


#endif


