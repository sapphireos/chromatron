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

#include <Arduino.h>

#include "list.h"
#include "memory.h"
#include "wifi_cmd.h"
#include "comm_printf.h"
#include "comm_intf.h"

void intf_v_printf( const char *format, ... ){

    char buf[128];
    
    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    // print string
    uint32_t len = vsnprintf( buf, sizeof(buf), format, ap );

    va_end( ap );

    len++; // add null terminator
    buf[len] = 0;

    intf_i8_send_msg( WIFI_DATA_ID_DEBUG_PRINT, (uint8_t *)buf, len );
}

void intf_v_serial_printf( const char *format, ... ){

    delay(20);

    char buf[256];
    
    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    // print string
    uint32_t len = vsnprintf( buf, sizeof(buf), format, ap );

    va_end( ap );

    len++; // add null terminator
    buf[len] = 0;

    // reset serial baud rate
    Serial.begin(115200);

    Serial.println(buf);
}

void intf_v_assert_printf( const char *format, va_list ap ){

    delay(20);

    char buf[256];
    
    // print string
    uint32_t len = vsnprintf( buf, sizeof(buf), format, ap );

    len++; // add null terminator
    buf[len] = 0;

    // reset serial baud rate
    Serial.begin(115200);

    Serial.println(buf);
}



