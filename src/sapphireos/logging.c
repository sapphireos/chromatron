/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#include <stdarg.h>

#include "cpu.h"

#include "config.h"
#include "netmsg.h"
#include "ip.h"
#include "timers.h"
#include "fs.h"
#include "os_irq.h"

#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"


#ifdef LOG_ENABLE

static char buf[LOG_STR_BUF_SIZE];
static file_t f = -1;
static uint32_t max_log_size;

void log_v_init( void ){

    if( f > 0 ){

        f = fs_f_close( f );
    }

    f = fs_f_open_P( PSTR("log.txt"), FS_MODE_WRITE_APPEND | FS_MODE_CREATE_IF_NOT_FOUND );

    if( cfg_i8_get( CFG_PARAM_MAX_LOG_SIZE, &max_log_size ) < 0 ){

        max_log_size = LOG_MAX_SIZE;
    }

    if( max_log_size > LOG_MAX_SIZE ){

        max_log_size = LOG_MAX_SIZE;
        cfg_v_set( CFG_PARAM_MAX_LOG_SIZE, &max_log_size );
    }
}

static void append_log( char *buf ){

    // trace_printf("%s", buf); // log prints already have newlines

    // check if file is not open
    if( f < 0 ){

        return;
    }

    // check file size
    int32_t file_size = fs_i32_get_size( f );

    // check for error (this will happen if the file is deleted)
    if( file_size < 0 ){

        goto cleanup;
    }

    // check if file is too large
    if( file_size >= (int32_t)max_log_size ){

        goto cleanup;
    }

    // write to file
    if( fs_i16_write( f, buf, strnlen( buf, LOG_STR_BUF_SIZE ) ) < 0 ){
    
        goto cleanup;
    }

    return;

cleanup:
    // close handle if error so we will try to reopen next time
    f = fs_f_close( f );
}


void _log_v_print_P( uint8_t level, PGM_P file, uint16_t line, PGM_P format, ... ){
    /*
     * TODO:
     * The print statements in this function will allow a buffer overflow,
     * despite my best intentions.  For now, the buffer is large enough for
     * most logging needs, and there is a canary check at the end of the buffer,
     * so if it does overflow, it will assert here.
     *
     */

    // do not run in an interrupt
    // if( osirq_b_is_irq() ){

    //     return;
    // }

    // check log level
    if( (int8_t)level < LOG_LEVEL ){

        return;
    }

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // mem_handle_t h = mem2_h_alloc( LOG_STR_BUF_SIZE );
    //
    // if( h < 0 ){
    //
    //     return;
    // }
    //
    // char *buf = mem2_vp_get_ptr( h );

    // ATOMIC;

    EVENT( EVENT_ID_LOG_RECORD, 0 );

    uint8_t len = 0;

    // print level
    switch( level ){
        case LOG_LEVEL_INFO:
            len += strlcpy_P( &buf[len], PSTR("info :"), LOG_STR_BUF_SIZE - len );
            break;

        case LOG_LEVEL_WARN:
            len += strlcpy_P( &buf[len], PSTR("warn :"), LOG_STR_BUF_SIZE - len );
            break;

        case LOG_LEVEL_ERROR:
            len += strlcpy_P( &buf[len], PSTR("error:"), LOG_STR_BUF_SIZE - len );
            break;

        case LOG_LEVEL_CRITICAL:
            len += strlcpy_P( &buf[len], PSTR("crit :"), LOG_STR_BUF_SIZE - len );
            break;

        default:
            len += strlcpy_P( &buf[len], PSTR("debug:"), LOG_STR_BUF_SIZE - len );
            break;
    }

    // print timestamp
    len += snprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, PSTR("%8lu:"), tmr_u32_get_system_time_ms() );

    // print filename
    #ifdef AVR
    len += snprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, PSTR("%16S:"), file ); // this version for ROM strings
    #else
    len += snprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, PSTR("%16s:"), file );
    #endif

    // print line
    len += snprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, PSTR("%4d:"), line );

    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    // print string
    len += vsnprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, format, ap );

    va_end( ap );

    // attach new line
    snprintf_P( &buf[len], LOG_STR_BUF_SIZE - len, PSTR("\r\n") );

    // write to log file
    append_log( buf );

    EVENT( EVENT_ID_LOG_RECORD, 1 );

    // mem2_v_free( h );

    // END_ATOMIC;
}

void _log_v_icmp( netmsg_t netmsg, PGM_P file, uint16_t line ){

    #ifdef LOG_ICMP

    // ATOMIC;

    // get the ip header
    ip_hdr_t *ip_hdr = (ip_hdr_t *)netmsg_vp_get_data( netmsg );

    if( ip_hdr->protocol == IP_PROTO_ICMP ){

        //_log_v_print_P( LOG_LEVEL_DEBUG, file, line, PSTR("ICMP") );


        _log_v_print_P( LOG_LEVEL_DEBUG,
                        file,
                        line,
                        PSTR("ICMP From:%d.%d.%d.%d To:%d.%d.%d.%d"),
                        ip_hdr->source_addr.ip3,
                        ip_hdr->source_addr.ip2,
                        ip_hdr->source_addr.ip1,
                        ip_hdr->source_addr.ip0,
                        ip_hdr->dest_addr.ip3,
                        ip_hdr->dest_addr.ip2,
                        ip_hdr->dest_addr.ip1,
                        ip_hdr->dest_addr.ip0 );

    }

    // END_ATOMIC;

    #endif
}

void log_v_flush( void ){

}

#endif