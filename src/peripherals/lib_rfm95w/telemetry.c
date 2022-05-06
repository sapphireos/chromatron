/*
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
 */


#include "sapphire.h"

#include "telemetry.h"
#include "rf_mac.h"

#ifdef ESP32

static bool telemetry_enable;
static bool telemetry_station_enable;

KV_SECTION_META kv_meta_t telemetry_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemetry_enable,           0,   "telemetry_enable" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemetry_station_enable,   0,   "telemetry_station_enable" },
};


PT_THREAD( telemetry_thread( pt_t *pt, void *state ) );
PT_THREAD( telemetry_base_station_thread( pt_t *pt, void *state ) );

void telemetry_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    if( !telemetry_enable ){

        return;
    }

    if( rf_mac_i8_init() < 0 ){

        return;
    }
    
    if( telemetry_station_enable ){

        thread_t_create( telemetry_base_station_thread,
                     PSTR("telemetry_base_station"),
                     0,
                     0 );
    }  
    else{

        thread_t_create( telemetry_thread,
                     PSTR("telemetry"),
                     0,
                     0 );
    }
}


PT_THREAD( telemetry_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    


    while( 1 ){

        THREAD_WAIT_WHILE( pt, !rf_mac_b_rx_available() );

        rf_mac_rx_pkt_t pkt;
        uint8_t buf[RFM95W_FIFO_LEN];

        rf_mac_i8_get_rx( &pkt, buf, sizeof(buf) );

        uint32_t *data = (uint32_t *)&buf[sizeof(rf_mac_header_0_t)];

        log_v_debug_P( PSTR("received %u"), *data );
    }

PT_END( pt );
}


PT_THREAD( telemetry_base_station_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint32_t count;
    count = 0;
    
    while( 1 ){

        TMR_WAIT( pt, 1000 );

        rf_mac_i8_send( 0, (uint8_t *)&count, sizeof(count) );

        log_v_debug_P( PSTR("send %u"), count );
        count++;
    }

PT_END( pt );
}







#else

void telemetry_v_init( void ){


}


#endif