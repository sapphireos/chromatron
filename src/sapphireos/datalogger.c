/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "system.h"
#include "memory.h"
#include "timers.h"
#include "msgflow.h"
#include "fs.h"
#include "threading.h"
#include "logging.h"
#include "hash.h"
#include "keyvalue.h"
#include "datalogger.h"

#ifdef ENABLE_MSGFLOW

typedef struct{
    catbus_hash_t32 hash;
    uint16_t tick_rate;
    uint16_t ticks;
} datalog_entry_t;

static mem_handle_t datalog_handle = -1;

PT_THREAD( datalog_config_thread( pt_t *pt, void *state ) );
PT_THREAD( datalog_thread( pt_t *pt, void *state ) );


void datalog_v_init( void ){

    thread_t_create( datalog_thread,
                     PSTR("datalogger"),
                     0,
                     0 );

    thread_t_create( datalog_config_thread,
                     PSTR("datalogger_config"),
                     0,
                     0 );

}

PT_THREAD( datalog_config_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint32_t file_hash;
    file_hash = 0;

    while(1){

        file_t f = fs_f_open_P( PSTR("datalog_config"), FS_MODE_READ_ONLY );

        if( f < 0 ){

            goto done;
        }

        // get file hash
        uint32_t hash = 0;
        int16_t read_len = 0;
        do{

            uint8_t buf[256];

            read_len = fs_i16_read( f, buf, sizeof(buf) );

            if( read_len > 0 ){

                hash = hash_u32_partial( hash, buf, read_len );    
            }

        } while( read_len > 0 );

        if( hash == file_hash ){

            // file has not changed

            goto done;
        }

        log_v_debug_P( PSTR("Loading new datalogger config") );
        file_hash = hash;

        fs_v_seek( f, 0 );

        datalog_file_entry_t entry;
        uint16_t entry_count = fs_i32_get_size( f ) / sizeof(entry);

        if( datalog_handle > 0 ){

            mem2_v_free( datalog_handle );

            datalog_handle = -1;
        }

        datalog_handle = mem2_h_alloc( entry_count * sizeof(datalog_entry_t) );

        if( datalog_handle < 0 ){

            goto done;
        }

        datalog_entry_t *ptr = (datalog_entry_t *)mem2_vp_get_ptr( datalog_handle );        

        while( fs_i16_read( f, &entry, sizeof(entry) ) == sizeof(entry) ){

            // error check
            if( entry.rate < DATALOG_TICK_RATE ){

                entry.rate = DATALOG_TICK_RATE;
            }

            ptr->hash = entry.hash;
            ptr->tick_rate = entry.rate / DATALOG_TICK_RATE;
            ptr->ticks = ptr->tick_rate;

            ptr++;
        }       

    done:
        if( f > 0 ){
            
            f = fs_f_close( f );
        }
        
        TMR_WAIT( pt, 10000 );
    }
        
PT_END( pt );
}


PT_THREAD( datalog_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // wait until we have a valid config
    THREAD_WAIT_WHILE( pt, datalog_handle < 0 );
    
    static msgflow_t msgflow;

    msgflow = msgflow_m_listen( __KV__datalogger, MSGFLOW_CODE_ANY, 128 );

    // msgflow creation failed
    if( msgflow <= 0 ){

        THREAD_EXIT( pt );
    }

    while(1){

        THREAD_WAIT_WHILE( pt, !msgflow_b_connected( msgflow ) || ( datalog_handle < 0 ) );

        log_v_debug_P( PSTR("Datalogger connected") );

        thread_v_set_alarm( tmr_u32_get_system_time_ms() );

        while( msgflow_b_connected( msgflow ) && ( datalog_handle > 0 ) ){

            uint16_t entries = mem2_u16_get_size( datalog_handle ) / sizeof(datalog_entry_t);

            datalog_entry_t *entry_ptr = (datalog_entry_t *)mem2_vp_get_ptr( datalog_handle );

            while( entries > 0 ){

                entries--;
                
                // check timeout
                entry_ptr->ticks--;
                if( entry_ptr->ticks > 0 ){

                    goto done;
                }

                entry_ptr->ticks = entry_ptr->tick_rate;

                #define MAX_DATA_LEN 128

                uint8_t buf[sizeof(datalog_data_t) + MAX_DATA_LEN];
                datalog_data_t *data_msg = (datalog_data_t *)buf;
                memset( data_msg->reserved, 0, sizeof(data_msg->reserved) );
                uint8_t *data = &data_msg->data.data;

                uint16_t msglen = ( sizeof(datalog_data_t) - 1 ) + kv_i16_len( entry_ptr->hash );

                if( ( kv_i8_get( entry_ptr->hash, data, MAX_DATA_LEN ) == KV_ERR_STATUS_OK ) &&
                    ( kv_i8_get_catbus_meta( entry_ptr->hash, &data_msg->data.meta ) == KV_ERR_STATUS_OK ) ){

                    // transmit!
                    msgflow_b_send( msgflow, buf, msglen );
                }

            done:
                entry_ptr++;
            }

            thread_v_set_alarm( thread_u32_get_alarm() + DATALOG_TICK_RATE );
            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );
        }

        log_v_debug_P( PSTR("Datalogger disconnected") );
    }
        
PT_END( pt );
}


#endif