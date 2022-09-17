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
#include "logging.h"
#include "timers.h"
#include "list.h"
#include "keyvalue.h"
#include "fs.h"
#include "timesync.h"
#include "datetime.h"
#include "util.h"
#include "config.h"

#include "vm_core.h"
#include "vm_cron.h"


#ifdef ENABLE_TIME_SYNC

static datetime_t cron_now;
static uint32_t cron_seconds;
static list_t cron_list;


PT_THREAD( cron_thread( pt_t *pt, void *state ) );


/*

Cron mods:

Add libcall function to get current time in NTP seconds.
Compile has a built in conversion of an NTP 8601 time string
to a 32 bit NTP seconds value.  ntptime() or somesuch.

Also have a function to check for at a specific date and time, 
with repeating granularity down to the second (like cron is now),
and also for range checks, for instance, between 2 times of day.
The times compute to whenever the next alarm is, and these 2 values
are compared against the current time.

@condition(fx_expression)

expressions are the comparison part of an if statement:

time() < '2021-08-19T03:45:59' # this will trigger once on this time and then never again
time() < Cron(hour=3) # run every day at 3:00a (24 hour time)
time() < Cron(day_of_week='Monday' hour=3) # run at 3 am on mondays

or:

assign to var:
c = Cron()

time() in between(cron1(), cron2())

These can compile in any way, including generating smaller units
of FX code to implement.  For instance, doing the time
calculations with 32 bit integers.  I'd prefer not to have
to implement variables longer than 32 bits for now.


Need to implement the syntax in the compiler, then look
at the timing system.
Also check why the current cron doesn't sync within 1-2 seconds.



*/


static bool job_ready( datetime_t *now, cron_job_t *job ){

    if( ( job->cron.seconds >= 0 ) && ( job->cron.seconds != now->seconds ) ){

        return FALSE;
    }

    if( ( job->cron.minutes >= 0 ) && ( job->cron.minutes != now->minutes ) ){

        return FALSE;
    }

    if( ( job->cron.hours >= 0 ) && ( job->cron.hours != now->hours ) ){

        return FALSE;
    }

    if( ( job->cron.day_of_month >= 0 ) && ( job->cron.day_of_month != now->day ) ){

        return FALSE;
    }

    if( ( job->cron.day_of_week >= 0 ) && ( job->cron.day_of_week != now->weekday ) ){

        return FALSE;
    }

    if( ( job->cron.month >= 0 ) && ( job->cron.month != now->month ) ){

        return FALSE;
    }

    return TRUE;
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

        // init alarm
        thread_v_set_alarm( tmr_u32_get_system_time_ms() );

        while( time_b_is_ntp_sync() && !list_b_is_empty( &cron_list ) ){

            thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

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

                        int8_t status = vm_cron_i8_run_func( entry->vm_id, entry->cron.func_addr );                   
                       
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

typedef struct{
    uint8_t vm_id;
    uint32_t cron_seconds;
    datetime_t cron_time;
} replay_state_t;

PT_THREAD( cron_replay_thread( pt_t *pt, replay_state_t *state ) )
{
PT_BEGIN( pt ); 

    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        THREAD_EXIT( pt );
    }

    // wait for time sync
    THREAD_WAIT_WHILE( pt, !time_b_is_ntp_sync() );

    // init clock to 24 hours ago
    ntp_ts_t ntp_now = time_t_local_now();
    state->cron_seconds = ntp_now.seconds - ( 24 * 60 * 60 );
    datetime_v_seconds_to_datetime( state->cron_seconds, &state->cron_time );

    
    while(1){

        ntp_ts_t ntp_local_now = time_t_local_now();

        for( uint16_t i = 0; i < 600; i++ ){

            datetime_v_increment_seconds( &state->cron_time );
            state->cron_seconds++;

            // run through job list
            list_node_t ln = cron_list.head;
            list_node_t next_ln;

            while( ln > 0 ){

                next_ln = list_ln_next( ln );

                cron_job_t *entry = list_vp_get_data( ln );

                if( entry->vm_id != state->vm_id ){

                    goto next;
                }

                if( job_ready( &state->cron_time, entry ) ){

                    int8_t status = vm_cron_i8_run_func( entry->vm_id, entry->cron.func_addr );                   
                   
                    log_v_debug_P( PSTR("Replaying cron job: %u for vm: %d status: %d"), entry->cron.func_addr, entry->vm_id, status );
                }

next:
                ln = next_ln;
            }   

            if( state->cron_seconds >= ntp_local_now.seconds ){

                THREAD_EXIT( pt );
            }
        }
        
        TMR_WAIT( pt, 20 );
    }

PT_END( pt );
}

#endif


void vm_cron_v_init( void ){

    #ifdef ENABLE_TIME_SYNC

    list_v_init( &cron_list );

    thread_t_create( cron_thread,
             PSTR("cron"),
             0,
             0 );

    #endif
}

void vm_cron_v_load( uint8_t vm_id, vm_state_t *state, file_t f ){

    #ifdef ENABLE_TIME_SYNC

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

    replay_state_t replay_state;
    replay_state.vm_id = vm_id;

    thread_t_create( THREAD_CAST(cron_replay_thread),
             PSTR("cron_replay"),
             &replay_state,
             sizeof(replay_state) );

    #endif
}
