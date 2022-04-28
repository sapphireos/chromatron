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

static bool telemtry_enable;
static bool telemtry_station_enable;

KV_SECTION_META kv_meta_t telemetry_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemtry_enable,           0,   "telemtry_enable" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemtry_station_enable,   0,   "telemtry_station_enable" },
};

void telemetry_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    if( !telemtry_enable ){

        return;
    }

    if( rf_mac_i8_init() < 0 ){

        return;
    }
    

}

#else

void telemetry_v_init( void ){


}


#endif