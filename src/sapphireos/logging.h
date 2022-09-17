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

#ifndef _LOGGING_H
#define _LOGGING_H

#include <inttypes.h>

// uncomment to disable the logging module.
// this can save considerable ROM space and improve runtime performance.
// #define NO_LOGGING

// #define LOG_MAX_BUFFER_SIZE // this is defined in target.h

// uncomment this to enable ICMP packet tracing
// #define LOG_ICMP

#define LOG_MAX_SIZE            ( (uint32_t)256 * 1024 )

#ifndef NO_LOGGING
    #define LOG_ENABLE
#endif

// check if flash file system is disabled,
// if so, disable logging module
#ifndef ENABLE_FFS
    #ifndef TRACE
        #undef LOG_ENABLE
    #endif
#endif


#define LOG_STR_BUF_SIZE        192

#define LOG_LEVEL_DEBUG         0
#define LOG_LEVEL_INFO          1
#define LOG_LEVEL_WARN          2
#define LOG_LEVEL_ERROR         3
#define LOG_LEVEL_CRITICAL      4

#define LOG_LEVEL               LOG_LEVEL_DEBUG


#ifdef LOG_ENABLE
        
    #include "system.h"
    #include "netmsg.h"

    void log_v_init( void );
    void _log_v_icmp( netmsg_t netmsg, PGM_P file, uint16_t line );
    void _log_v_print_P( uint8_t level, PGM_P file, uint16_t line, PGM_P format, ... );
    
    #define log_v_print_P( level, format, ... ) _log_v_print_P( level, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )
    
    #define log_v_debug_P( format, ... )    _log_v_print_P( 0, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )
    #define log_v_info_P( format, ... )     _log_v_print_P( 1, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )
    #define log_v_warn_P( format, ... )     _log_v_print_P( 2, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )
    #define log_v_error_P( format, ... )    _log_v_print_P( 3, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )
    #define log_v_critical_P( format, ... ) _log_v_print_P( 4, __SOURCE_FILE__, __LINE__, format, ##__VA_ARGS__ )

    #define log_v_icmp( netmsg ) _log_v_icmp( netmsg, __SOURCE_FILE__, __LINE__ )

    void log_v_flush( void );
#else
    #define log_v_init()
    #define log_v_icmp( ... )
    #define log_v_print_P( ... )
    #define log_v_debug_P( ... )
    #define log_v_info_P( ... )  
    #define log_v_warn_P( ... )    
    #define log_v_error_P( ... )    
    #define log_v_critical_P( ... ) 
    #define log_v_flush()

#endif


#endif

