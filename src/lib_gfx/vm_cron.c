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

#include "system.h"
#include "threading.h"
#include "logging.h"
#include "timers.h"
#include "list.h"
#include "keyvalue.h"
#include "fs.h"
#include "timesync.h"
#include "datetime.h"
#include "util.h"

#ifdef VM_TARGET_ESP
#include "esp8266.h"
#endif

#include "vm_core.h"
#include "vm_cron.h"
#include "vm_wifi_cmd.h"

#ifdef ENABLE_TIME_SYNC

static datetime_t cron_now;
static uint32_t cron_seconds;
static list_t cron_list;

extern int8_t vm_i8_run_vm( uint8_t vm_id, uint8_t data_id, uint16_t func_addr, wifi_msg_vm_info_t *info );


PT_THREAD( cron_thread( pt_t *pt, void *state ) );


static bool job_ready( datetime_t *now, cron_job_t *job ){

    bool match = TRUE;

    if( ( job->cron.seconds >= 0 ) && ( job->cron.seconds != now->seconds ) ){

        match = FALSE;
    }

    if( ( job->cron.minutes >= 0 ) && ( job->cron.minutes != now->minutes ) ){

        match = FALSE;
    }

    if( ( job->cron.hours >= 0 ) && ( job->cron.hours != now->hours ) ){

        match = FALSE;
    }

    if( ( job->cron.day_of_month >= 0 ) && ( job->cron.day_of_month != now->day ) ){

        match = FALSE;
    }

    if( ( job->cron.day_of_week >= 0 ) && ( job->cron.day_of_week != now->weekday ) ){

        match = FALSE;
    }

    if( ( job->cron.month >= 0 ) && ( job->cron.month != now->month ) ){

        match = FALSE;
    }

    return match;
}

void vm_cron_v_init( void ){

    list_v_init( &cron_list );

    thread_t_create( cron_thread,
             PSTR("cron"),
             0,
             0 );
}

void vm_cron_v_load( uint8_t vm_id, vm_state_t *state, file_t f ){

    // make sure this vm's cron jobs are unloaded first
    vm_cron_v_unload( vm_id );

    fs_v_seek( f, sizeof(uint32_t) + state->cron_start );

    cron_job_t cron_job;
    cron_job.vm_id = vm_id;

    for( uint8_t i = 0; i < state->cron_count; i++ ){

        fs_i16_read( f, (uint8_t *)&cron_job.cron, sizeof(cron_job.cron) );

        list_node_t ln = list_ln_create_node2( &cron_job, sizeof(cron_job), MEM_TYPE_CRON_JOB );

        if( ln < 0 ){

            return;
        }

        list_v_insert_tail( &cron_list, ln );
    }
}

void vm_cron_v_unload( uint8_t vm_id ){

    list_node_t ln = cron_list.head;
    list_node_t next_ln;

    while( ln > 0 ){

        next_ln = list_ln_next( ln );

        cron_job_t *entry = list_vp_get_data( ln );

        if( entry->vm_id == vm_id ){

            list_v_remove( &cron_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }   
}



PT_THREAD( cron_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        // prevent runaway thread
        THREAD_YIELD( pt );

        // wait for sync and for cron jobs to be loaded
        THREAD_WAIT_WHILE( pt, !time_b_is_ntp_sync() || list_b_is_empty( &cron_list ) );

        // initialize cron clock
        ntp_ts_t ntp_local_now = time_t_local_now();
        datetime_v_seconds_to_datetime( ntp_local_now.seconds, &cron_now );
        cron_seconds = ntp_local_now.seconds;

        while( time_b_is_ntp_sync() && !list_b_is_empty( &cron_list ) ){

            TMR_WAIT( pt, 1000 );
            
            // update clock
            ntp_ts_t ntp_local_now = time_t_local_now();

            int32_t delta = (int64_t)ntp_local_now.seconds - (int64_t)cron_seconds;

            // log_v_debug_P( PSTR("Cron delta: %d"), delta );

            if( abs32( delta ) > 10 ){

                // our clock is pretty far off for some reason.
                // restart cron.

                log_v_debug_P( PSTR("Cron resynchronizing clock") );

                THREAD_RESTART( pt );
            }   

            // step through seconds while local clock is ahead of cron's clock
            while( delta > 0 ){

                datetime_v_increment_seconds( &cron_now );

                // run through job list
                list_node_t ln = cron_list.head;
                list_node_t next_ln;

                while( ln > 0 ){

                    next_ln = list_ln_next( ln );

                    cron_job_t *entry = list_vp_get_data( ln );

                    if( job_ready( &cron_now, entry ) ){

                        #ifdef VM_TARGET_ESP
                        wifi_msg_vm_info_t info;
                        int8_t status = vm_i8_run_vm( entry->vm_id, WIFI_DATA_ID_VM_RUN_FUNC, entry->cron.func_addr, &info );
                        #else

                        int8_t status = vm_cron_i8_run_func( entry->vm_id, entry->cron.func_addr );                   
                        #endif

                        log_v_debug_P( PSTR("Running cron job: %u for vm: %d status: %d"), entry->cron.func_addr, entry->vm_id, status );
                    }

                    ln = next_ln;
                }   

                delta--;
            }

            // update cron clock
            cron_seconds = ntp_local_now.seconds;
        }
    }

PT_END( pt );
}

#endif



// this code is unfinished
// static void calc_deadline( uint32_t local_seconds, datetime_t *now, cron_job_t *job ){

//     datetime_t datetime_cron = *now;
    
//     if( job->cron.month >= 0 ){

//         datetime_cron.month = job->cron.month;
//     }

//     if( job->cron.day_of_week >= 0 ){

//         datetime_cron.weekday = job->cron.day_of_week;
//     }

//     if( job->cron.day_of_month >= 0 ){

//         datetime_cron.day = job->cron.day_of_month;
//     }

//     if( job->cron.hours >= 0 ){

//         datetime_cron.hours = job->cron.hours;
//     }

//     if( job->cron.minutes >= 0 ){

//         datetime_cron.minutes = job->cron.minutes;
//     }

//     if( job->cron.seconds >= 0 ){

//         datetime_cron.seconds = job->cron.seconds;
//     }

//     uint32_t seconds = datetime_u32_datetime_to_seconds( &datetime_cron );

//     int32_t delta = (int64_t)seconds - (int64_t)local_seconds;
//     log_v_debug_P( PSTR("Next event: %lu delta: %ld"), seconds, delta );
// }