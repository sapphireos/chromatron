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

#include "system.h"
#include "memory.h"
#include "timers.h"
#include "msgflow.h"
#include "fs.h"
#include "threading.h"
#include "logging.h"
#include "hash.h"
#include "keyvalue.h"
#include "time_ntp.h"
#include "datalogger.h"

#ifdef ENABLE_MSGFLOW

typedef struct{
    catbus_hash_t32 hash;
    catbus_meta_t meta;
    uint16_t tick_rate;
    uint16_t ticks;
} datalog_entry_t;


#define DATALOG_FLUSH_TICKS ( DATALOG_FLUSH_RATE / DATALOG_TICK_RATE )


static mem_handle_t datalog_handle = -1;
static mem_handle_t datalog_buffer_handle = -1;
static msgflow_t msgflow = -1;
static ntp_ts_t ntp_base;
static uint32_t systime_base;
static uint16_t buffer_offset;

static bool refresh_config;

PT_THREAD( datalog_config_thread( pt_t *pt, void *state ) );
PT_THREAD( datalog_thread( pt_t *pt, void *state ) );


void datalog_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( datalog_thread,
                     PSTR("datalogger"),
                     0,
                     0 );

    thread_t_create( datalog_config_thread,
                     PSTR("datalogger_config"),
                     0,
                     0 );
}

static void reset_config( void ){

    if( datalog_handle > 0 ){

        mem2_v_free( datalog_handle );

        datalog_handle = -1;
    }
}

PT_THREAD( datalog_config_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint32_t file_hash;
    file_hash = 0;

    while(1){

        file_t f = fs_f_open_P( PSTR("datalog_config"), FS_MODE_READ_ONLY );

        if( f < 0 ){

            reset_config();

            goto done;
        }

        uint32_t hash = 0;

        // if refresh_config is set, we can skip the file check and go straight to reloading
        // the config file
        if( !refresh_config ){

            // get file hash
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
        }

        refresh_config = FALSE;

        log_v_debug_P( PSTR("Loading new datalogger config") );
        file_hash = hash;

        fs_v_seek( f, 0 );

        datalog_file_entry_t file_entry;
        uint16_t entry_count = fs_i32_get_size( f ) / sizeof(file_entry);

        reset_config();

        datalog_handle = mem2_h_alloc( entry_count * sizeof(datalog_entry_t) );

        if( datalog_handle < 0 ){

            reset_config();
            goto done;
        }

        datalog_entry_t *ptr = (datalog_entry_t *)mem2_vp_get_ptr( datalog_handle );        

        while( fs_i16_read( f, &file_entry, sizeof(file_entry) ) == sizeof(file_entry) ){

            memset( ptr, 0, sizeof(datalog_entry_t) );

            if( kv_i8_get_catbus_meta( file_entry.hash, &ptr->meta ) != KV_ERR_STATUS_OK ){

                goto next;
            }

            // error check
            if( file_entry.rate < DATALOG_TICK_RATE ){

                file_entry.rate = DATALOG_TICK_RATE;
            }

            ptr->hash = file_entry.hash;
            ptr->tick_rate = file_entry.rate / DATALOG_TICK_RATE;
            ptr->ticks = ptr->tick_rate;

        next:
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


static int8_t record_data( datalog_entry_t *entry, uint32_t timestamp ){

#if DATALOG_VERSION == 2

    if( entry->hash == 0 ){

        return -1;
    }

    if( datalog_buffer_handle < 0 ){

        return -2;
    }

    if( buffer_offset == 0 ){

        return -3;
    }

    uint8_t *ptr = mem2_vp_get_ptr( datalog_buffer_handle );

    int16_t remaining_space = DATALOG_MAX_BUFFER_SIZE - buffer_offset;

    uint16_t data_size = type_u16_size_meta( &entry->meta );
    uint16_t chunk_size = ( sizeof(datalog_data_v2_t) - 1 ) + data_size;

    if( remaining_space < chunk_size ){

        return 1;
    }

    datalog_data_v2_t *chunk = (datalog_data_v2_t *)&ptr[buffer_offset];

    uint32_t ntp_offset = tmr_u32_elapsed_times( systime_base, timestamp );

    if( ntp_offset > INT32_MAX ){

        chunk->ntp_offset = (int32_t)ntp_offset;
    }
    else{

        chunk->ntp_offset = ntp_offset;
    }

    chunk->data.meta = entry->meta;

    if( kv_i8_get( entry->hash, &chunk->data.data, data_size ) != KV_ERR_STATUS_OK ){

        return -4;
    }

    buffer_offset += chunk_size;

    return 0;

#elif DATALOG_VERSION == 1

    #define MAX_DATA_LEN 128

    uint8_t buf[sizeof(datalog_header_t) + sizeof(datalog_data_v1_t) + MAX_DATA_LEN];
    memset( buf, 0, sizeof(buf) );
    datalog_header_t *header = (datalog_header_t *)buf;

    header->magic = DATALOG_MAGIC;
    header->version = DATALOG_VERSION;

    if( time_b_is_ntp_sync() ){

        header->flags |= DATALOG_FLAGS_NTP_SYNC;
    }

    datalog_data_v1_t *data_msg = (datalog_data_v1_t *)( header + 1 );
    uint8_t *data = &data_msg->data.data;

    uint16_t msglen = ( sizeof(datalog_data_v1_t) - 1 ) + type_u16_size_meta( &entry->meta ) + sizeof(datalog_header_t);

    if( kv_i8_get( entry->hash, data, MAX_DATA_LEN ) == KV_ERR_STATUS_OK ){

        data_msg->data.meta = entry->meta;

        // transmit!
        msgflow_b_send( msgflow, buf, msglen );
    }

    return 0;
#endif
}


static void flush( void ){

    // check for empty buffer
    if( buffer_offset == 0 ){

        return;
    }

    if( datalog_handle <= 0 ){

        return;
    }

    if( datalog_buffer_handle <= 0 ){

        return;
    }

    if( !msgflow_b_connected( msgflow ) ){

        return;
    }

    uint8_t buf[MSGFLOW_MAX_LEN];

    datalog_header_t *header = (datalog_header_t *)buf;
    memset( header, 0, sizeof(datalog_header_t) );
    uint8_t *msg_ptr = (uint8_t *)( header + 1 );
    uint16_t msg_size = sizeof(datalog_header_t);

    header->magic = DATALOG_MAGIC;
    header->version = DATALOG_VERSION;

    if( ntp_b_is_sync() ){

        header->flags |= DATALOG_FLAGS_NTP_SYNC;
    }

    uint8_t *buf_ptr = mem2_vp_get_ptr( datalog_buffer_handle );

    memcpy( msg_ptr, buf_ptr, buffer_offset );
    msg_size += buffer_offset;

    buffer_offset = 0;

    msgflow_b_send( msgflow, buf, msg_size );
}


PT_THREAD( datalog_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint8_t ticks;
    ticks = 0;
    
    // wait until we have a valid config
    THREAD_WAIT_WHILE( pt, datalog_handle < 0 );

    // allocate buffer    
    datalog_buffer_handle = mem2_h_alloc( DATALOG_MAX_BUFFER_SIZE );

    if( datalog_buffer_handle < 0 ){

        THREAD_EXIT( pt );
    }

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

            ticks++;

            if( ticks >= DATALOG_FLUSH_TICKS ){

                ticks = 0;

                flush();
            }

            uint32_t timestamp = tmr_u32_get_system_time_ms();

            uint16_t entries = mem2_u16_get_size( datalog_handle ) / sizeof(datalog_entry_t);

            datalog_entry_t *entry_ptr = (datalog_entry_t *)mem2_vp_get_ptr( datalog_handle );

            while( entries > 0 ){

                entries--;
                
                // check timeout
                entry_ptr->ticks--;
                if( entry_ptr->ticks > 0 ){

                    goto done;
                }

                // item is ready to record

                if( buffer_offset == 0 ){

                    datalog_v2_meta_t *buf_meta_ptr = mem2_vp_get_ptr( datalog_buffer_handle );

                    // memset( buf_meta_ptr, 0, mem2_u16_get_size( datalog_buffer_handle ) );

                    // get NTP time
                    ntp_v_get_timestamp( &ntp_base, &systime_base );

                    buf_meta_ptr->ntp_base = ntp_base;

                    buffer_offset += sizeof(datalog_v2_meta_t);
                }

                entry_ptr->ticks = entry_ptr->tick_rate;

                int8_t status = record_data( entry_ptr, timestamp );

                if( status > 0 ){

                    // queue for transmission
                    flush();

                    if( buffer_offset == 0 ){

                        datalog_v2_meta_t *buf_meta_ptr = mem2_vp_get_ptr( datalog_buffer_handle );

                        // memset( buf_meta_ptr, 0, mem2_u16_get_size( datalog_buffer_handle ) );

                        // get NTP time
                        ntp_v_get_timestamp( &ntp_base, &systime_base );

                        buf_meta_ptr->ntp_base = ntp_base;

                        buffer_offset += sizeof(datalog_v2_meta_t);
                    }

                    record_data( entry_ptr, timestamp );
                }
                else if( status < 0 ){

                    log_v_warn_P( PSTR("datalog failed: %d"), status );
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

void datalogger_v_refresh_config( void ){

    #ifdef ENABLE_MSGFLOW
    refresh_config = TRUE;
    #endif
}