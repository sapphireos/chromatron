/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "list.h"
#include "memory.h"
#include "wifi_cmd.h"
#include "comm_printf.h"
#include "comm_intf.h"

void intf_v_printf( const char *format, ... ){

    char debug_print_buf[128];
    memset( debug_print_buf, 0, sizeof(debug_print_buf) );
    
    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    // print string
    uint32_t len = vsnprintf_P( debug_print_buf, sizeof(debug_print_buf), format, ap );

    va_end( ap );

    len++; // add null terminator

    intf_i8_send_msg( WIFI_DATA_ID_DEBUG_PRINT, (uint8_t *)debug_print_buf, len );
}