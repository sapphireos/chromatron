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


#include "system.h"

#include "threading.h"
#include "fs.h"
#include "timers.h"

#include "event_log.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"


#ifdef ENABLE_EVENT_LOG
    
    #ifdef EVENT_LOG_IN_FILE
    PT_THREAD( event_log_thread( pt_t *pt, void *state ) );

    static file_t evt_file;
    #endif

    static volatile event_t log_buffer[EVENT_LOG_BUFFER_SIZE];
    static volatile uint8_t ins_idx;

    #ifdef EVENT_LOG_IN_FILE
    static volatile uint8_t ext_idx;
    static volatile uint8_t buf_size;
    #endif 

#ifndef EVENT_LOG_IN_FILE
static uint32_t events_vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    uint8_t *src_ptr = (uint8_t *)log_buffer;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            // len = list_u16_flatten( &query_list, pos, ptr, len );
            memcpy( ptr, src_ptr + pos, len );

            break;

        case FS_VFILE_OP_SIZE:
            len = sizeof(log_buffer);
            break;

        default:
            len = 0;
            break;
    }

    return len;
}
#endif

#endif

void event_v_init( void ){

    #ifdef ENABLE_EVENT_LOG

        // check mode
        if( sys_u8_get_mode() == SYS_MODE_SAFE ){

            // don't run in safe mode
            return;
        }

    #ifdef EVENT_LOG_IN_FILE
        evt_file = fs_f_open_P( PSTR("event_log"),
                                FS_MODE_CREATE_IF_NOT_FOUND |
                                FS_MODE_WRITE_APPEND );

        if( evt_file < 0 ){

            log_v_error_P( PSTR("Unable to create event log") );

            return;
        }

        thread_t_create( event_log_thread,
                         PSTR("event_log"),
                         0,
                         0 );
    #else

    fs_v_create_virtual( PSTR("event_log"), events_vfile );

    #endif

    #endif

    EVENT( EVENT_ID_EVT_LOG_INIT, 0 );
}



#ifdef ENABLE_EVENT_LOG

void event_v_log( catbus_hash_t32 event_id, uint32_t param ){

    ATOMIC;

    #ifdef EVENT_LOG_IN_FILE
    if( buf_size < cnt_of_array(log_buffer) ){
    #endif
        
        uint32_t timestamp = tmr_u32_get_system_time_us();

        log_buffer[ins_idx].event_id    = event_id;
        log_buffer[ins_idx].param       = param;
        log_buffer[ins_idx].timestamp   = timestamp;

        #ifdef EVENT_LOG_IN_FILE
        buf_size++;
        #endif

        ins_idx++;

        if( ins_idx >= cnt_of_array(log_buffer) ){

            ins_idx = 0;
        }

    #ifdef EVENT_LOG_IN_FILE
    }
    else{

        sys_v_set_warnings( SYS_WARN_EVENT_LOG_OVERFLOW );
    }
    #endif

    END_ATOMIC;
}

#ifdef EVENT_LOG_IN_FILE
// flush all events to file.
// this does not align the write to the FS page boundaries, so it
// is slower than the logger thread.
void event_v_flush( void ){

    ATOMIC;

    while( buf_size > 0 ){

        // check file position
        if( fs_i32_tell( evt_file ) >= EVENT_LOG_MAX_SIZE ){

            // seek to beginning of file
            fs_v_seek( evt_file, 0 );
        }

        fs_i16_write( evt_file, (uint8_t *)&log_buffer[ext_idx], sizeof(log_buffer[ext_idx]) );

        buf_size--;

        ext_idx++;

        if( ext_idx >= cnt_of_array(log_buffer) ){

            ext_idx = 0;
        }
    }

    END_ATOMIC;
}

#else
void event_v_flush( void ){

}
#endif

#ifdef EVENT_LOG_IN_FILE
// event logger thread
PT_THREAD( event_log_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        // check file position
        if( fs_i32_tell( evt_file ) >= EVENT_LOG_MAX_SIZE ){

            #ifdef EVENT_LOG_ONE_SHOT
            THREAD_EXIT( pt );
            #else
            // seek to beginning of file
            fs_v_seek( evt_file, 0 );
            #endif
        }

        // We will log a multiple of 8 events at a time.  Each event is 8 bytes,
        // and the file system is most efficient writing 64 bytes
        // at a time.

        THREAD_WAIT_WHILE( pt, buf_size < EVENT_LOG_RECORD_INTERVAL );

        EVENT( EVENT_ID_EVT_LOG_RECORD, 0 );

        event_t temp_buf[EVENT_LOG_RECORD_INTERVAL];

        ATOMIC;

        // extract items from buffer
        for( uint8_t i = 0; i < cnt_of_array(temp_buf); i++ ){

            temp_buf[i] = log_buffer[ext_idx];

            buf_size--;

            ext_idx++;

            if( ext_idx >= cnt_of_array(log_buffer) ){

                ext_idx = 0;
            }
        }

        END_ATOMIC;

        // write temp buffer out to file
        int16_t status = fs_i16_write( evt_file, temp_buf, sizeof(temp_buf) );

        if( status < (int16_t)sizeof(temp_buf) ){

            log_v_error_P( PSTR("Error writing to event log") );
        }

        EVENT( EVENT_ID_EVT_LOG_RECORD, 1 );

        // prevent runaway thread
        THREAD_YIELD( pt );
    }

PT_END( pt );
}
#endif

#endif
