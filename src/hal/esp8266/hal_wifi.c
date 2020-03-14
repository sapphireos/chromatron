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
#include "wifi.h"

#include "test_ssid.h"

#ifdef ENABLE_WIFI

#define WIFI_OPMODE_STA 	0x01
#define WIFI_OPMODE_AP 		0x02
#define WIFI_OPMODE_STA_AP 	0x03

void wifi_v_init( void ){
	// return;
	trace_printf( "wifi_v_init\r\n" );

	wifi_station_disconnect();

	// set opmode without saving to flash (since we always set this)
	wifi_set_opmode_current( WIFI_OPMODE_STA );

	char *ssid = SSID;
    char *password = PASSWORD;
    
    struct station_config config = {0};
    strcpy( &config.ssid, ssid );
    strcpy( &config.password, password );

    wifi_station_set_config( &config );

    wifi_station_connect();
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


