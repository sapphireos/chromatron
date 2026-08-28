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
#include "time_ntp.h"
#include "datetime.h"
#include "util.h"
#include "config.h"
#include "vm4.h"

#include "cron.h"

#ifdef ENABLE_TIME_NTP

#if 0

Cron

minutes - hours - day of month - month - day of week

* - all possible values for field
, - list separator (not implemented)
- - range separator (not implemented)
/ - specify step for ranges

Every Minute
    * * * * *
Every Five Minutes
    */5 * * * *
Every 10 Minutes
    */10 * * * *
Every 15 Minutes
    */15 * * * *
Every 30 Minutes
    */30 * * * *
Every Hour
    0 * * * *
Every Two Hours
    0 */2 * * *
Every Six Hours
    0 */6 * * *
Every 12 Hours
    0 */12 * * *
Every day at Midnight
    0 0 * * *
At the Start of Every Month
    0 0 1 * *
On January 1st at Midnight
    0 0 1 1 *

Midnight, on sundays
    0 0 * * sunday

every hour, on sundays
    0 * * * sunday



Noon every day
    0 12 * * *

17:30 every day
    30 17 * * *


#endif

static list_t cron_list;

static datetime_t cron_now;
static uint32_t cron_seconds;


PT_THREAD( cron4_thread( pt_t *pt, void *state ) );


void cron_v_init( void ){

    list_v_init( &cron_list );

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( cron4_thread,
             PSTR("cron"),
             0,
             0 );
}

static bool is_whitespace( char s ){

    if( ( s == ' ' ) || ( s == '\0' ) ){
    
        return true;
    }

    return false;
}

static bool is_digit( char s ){

    if( ( s >= '0' ) && ( s <= '9' ) ){

        return true;
    }

    return false;
}

static uint8_t to_digit( char s ){

    if( ( s < '0' ) || ( s > '9' ) ){

        return 0;
    }

    return (uint8_t)( s - '0' );
}

static bool is_wildcard( char s ){

    return s == '*';
}

static char* strip_whitespace( char* s ){

    while( is_whitespace( *s ) ){

        s++;
    }

    return s;
}

static char* parse_number( char* s, int8_t *n ){

    *n = 0;

    if( is_wildcard( *s ) ){

        *n = -1;

        s++;

        return s;
    }

    while( is_digit( *s ) ){

        uint8_t digit = to_digit( *s );

        *n *= 10;
        *n += digit;

        s++;
    }

    return s;
}

int8_t cron_i8_parse( char* s, cron_job_t *job ){

    s = strip_whitespace( s );
    s = parse_number( s, &job->minutes );

    s = strip_whitespace( s );
    s = parse_number( s, &job->hours );

    s = strip_whitespace( s );
    s = parse_number( s, &job->day_of_month );

    s = strip_whitespace( s );
    s = parse_number( s, &job->month );

    s = strip_whitespace( s );
    s = parse_number( s, &job->day_of_week );

    log_v_debug_P( PSTR("%d %d %d %d %d"), 
        job->minutes,
        job->hours,
        job->day_of_month,
        job->month,
        job->day_of_week );

    return 0;
}

void cron_v_add_job( char *s, uint32_t func_meta, uint8_t vm_id ){

    cron_job_t job = {0};

    cron_i8_parse( s, &job );

    job.func_meta   = func_meta;
    job.vm_id       = vm_id;

    list_node_t ln = list_ln_create_node2( &job, sizeof(job), MEM_TYPE_CRON_JOB );

    if( ln < 0 ){

        return;
    }

    list_v_insert_tail( &cron_list, ln );
}

void cron_v_unload( uint8_t vm_id ){

    list_node_t ln = cron_list.head;
    list_node_t next_ln;

    while( ln > 0 ){

        next_ln = list_ln_next( ln );

        const cron_job_t *job = list_vp_get_data( ln );

        if( job->vm_id == vm_id ){

            list_v_remove( &cron_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }   
}


static bool job_ready( datetime_t *now, cron_job_t *job ){

    if( ( job->minutes >= 0 ) && ( job->minutes != now->minutes ) ){

        return FALSE;
    }

    if( ( job->hours >= 0 ) && ( job->hours != now->hours ) ){

        return FALSE;
    }

    if( ( job->day_of_month >= 0 ) && ( job->day_of_month != now->day ) ){

        return FALSE;
    }

    if( ( job->day_of_week >= 0 ) && ( job->day_of_week != now->weekday ) ){

        return FALSE;
    }

    if( ( job->month >= 0 ) && ( job->month != now->month ) ){

        return FALSE;
    }

    return TRUE;
}

PT_THREAD( cron4_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // cron_job_t job = {0};
    // cron_i8_parse("* 1 2 03 12", &job );

    // cron_i8_parse("* 1 2 03 12", &job );
    
    // while(1){

    //     TMR_WAIT( pt, 1000 );

    //     if( !ntp_b_is_sync() ){

    //         continue;
    //     }

    //     ntp_ts_t ntp_local_now = ntp_t_local_now();
    //     datetime_t cron_now;
    //     datetime_v_seconds_to_datetime( ntp_local_now.seconds, &cron_now );

    //     // log_v_debug_P( PSTR("%02d:%02d:%02d %d %d %d %d"),
    //     //     cron_now.hours,
    //     //     cron_now.minutes,
    //     //     cron_now.seconds,
    //     //     cron_now.day,
    //     //     cron_now.weekday,
    //     //     cron_now.month,
    //     //     cron_now.year );




    // }

    // cron_v_add_job( "* * * * *", 123, 0 );
    // cron_v_add_job( "0 * * * *", 456, 0 );

    while(1){

        // prevent runaway thread
        THREAD_YIELD( pt );

        // wait for sync and for cron jobs to be loaded
        THREAD_WAIT_WHILE( pt, !ntp_b_is_sync() || list_b_is_empty( &cron_list ) );

        // initialize cron clock
        ntp_ts_t ntp_local_now = ntp_t_local_now();
        datetime_v_seconds_to_datetime( ntp_local_now.seconds, &cron_now );
        cron_seconds = ntp_local_now.seconds;

        // init alarm
        thread_v_set_alarm( tmr_u32_get_system_time_ms() );

        while( ntp_b_is_sync() && !list_b_is_empty( &cron_list ) ){

            thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

            // update clock
            ntp_local_now = ntp_t_local_now();

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

                delta--;

                // check if top of the minute
                if( cron_now.seconds != 0 ){

                    continue;
                }

                // char s[32] = {0};
                // datetime_v_to_iso8601(s, sizeof(s), &cron_now);
                // log_v_debug_P(PSTR("%s"), s);

                // run through job list
                list_node_t ln = cron_list.head;
                list_node_t next_ln;

                while( ln > 0 ){

                    next_ln = list_ln_next( ln );

                    cron_job_t *job = list_vp_get_data( ln );

                    if( job_ready( &cron_now, job ) ){

                        log_v_debug_P( PSTR("Running cron job: %u"), job->func_meta );
                        int8_t status = vm4_i8_run_function( job->func_meta, job->vm_id );

                        if( status < 0 ){

                            log_v_warn_P( PSTR("Cron job failed: %d"), status );
                        }
                    }
                    
                    ln = next_ln;
                }   
            }

            // update cron clock
            cron_seconds = ntp_local_now.seconds;
        }
    }

PT_END( pt );
}

#endif