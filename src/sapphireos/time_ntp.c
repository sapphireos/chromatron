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

#include "sapphire.h"
#include "ip.h"
#include "logging.h"
#include "netmsg.h"
#include "sockets.h"
#include "timers.h"

#include "config.h"
#include "time_ntp.h"
#include "sntp.h"

#define NO_LOGGING

#ifdef ENABLE_TIME_NTP

/*

NTP notes:

We really don't need NTP to be any better than 1 second or so.
We have a high precision clock already for VM sync.




Check if no source:
random delay
start SNTP
start broadcasting as master clock

If receive better source:
set to follower
stop SNTP
stop broadcasting clock

If manual source:
if source is better than current:
    start broadcasting as master clock

*/


static socket_t sock;

// master clock:
static ip_addr4_t master_ip; 
static uint64_t master_timestamp;
static uint8_t prev_source;
static uint8_t clock_source;
static ntp_ts_t master_ntp_time;
uint64_t master_sys_time_ms; // system timestamp that correlates to NTP timestamp
static int16_t master_sync_delta;

static uint32_t last_sync_time;

static uint32_t ntp_syncs;
static uint32_t ntp_timeouts;

// NOTE!
// tz_offset is in MINUTES, because not all timezones
// are aligned on the hour.
static int16_t tz_offset;

static bool is_time_sync_enabled( void ){

    return cfg_b_get_boolean( __KV__enable_ntp_sync );
}

int8_t _ntp_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        if( hash == __KV__ntp_elapsed_last_sync ){

            uint32_t elapsed = 0;

            if( ntp_b_is_sync() ){

                elapsed = tmr_u32_elapsed_time_ms( last_sync_time );
            }

            STORE32(data, elapsed);
        }
    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__ntp_seconds ){            

            if( !is_time_sync_enabled() ){

                master_ntp_time = ntp_ts_from_u64( 0 );

                return 0;
            }

            // manual sync, this just sets up for seconds

            master_ntp_time.fraction = 0;
            master_sync_delta = 0;
            master_sys_time_ms = tmr_u64_get_system_time_ms();

            clock_source = NTP_SOURCE_MANUAL;
        }
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}


KV_SECTION_META kv_meta_t ntp_time_info_kv[] = {
    { CATBUS_TYPE_BOOL,     0, 0,                  0,                           cfg_i8_kv_handler,  "enable_ntp_sync" },

    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, &clock_source,               0,                  "ntp_master_source" },
    { CATBUS_TYPE_UINT32,   0, 0,                  &master_ntp_time.seconds,    _ntp_kv_handler,    "ntp_seconds" },
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_READ_ONLY, &master_sync_delta,          0,                  "ntp_master_sync_delta" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, 0,                           _ntp_kv_handler,    "ntp_elapsed_last_sync" },

    { CATBUS_TYPE_INT16,    0, KV_FLAGS_PERSIST,   &tz_offset,                  0,                  "datetime_tz_offset" },


    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &ntp_syncs,                  0,                  "ntp_syncs" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &ntp_timeouts,               0,                  "ntp_timeouts" },

    { CATBUS_TYPE_UINT64,   0, KV_FLAGS_READ_ONLY, &master_timestamp,           0,                  "ntp_master_timestamp" },
    { CATBUS_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,                  0,                  "ntp_master_ip" },
};


PT_THREAD( ntp_clock_thread( pt_t *pt, void *state ) );
PT_THREAD( ntp_server_thread( pt_t *pt, void *state ) );


void ntp_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    // check if time sync is enabled
    if( !is_time_sync_enabled() ){

        return;
    }    

    clock_source = NTP_SOURCE_NONE;

    sock = sock_s_create( SOS_SOCK_DGRAM );

    sock_v_bind( sock, NTP_SERVER_PORT );

    thread_t_create( ntp_server_thread,
                    PSTR("ntp_server"),
                    0,
                    0 );
    
    thread_t_create( ntp_clock_thread,
                    PSTR("ntp_clock"),
                    0,
                    0 );        
}

static void reset_clock( void ){

    clock_source = NTP_SOURCE_NONE;

    master_ntp_time     = ntp_ts_from_u64( 0 );
    master_sys_time_ms  = 0;
    master_sync_delta   = 0;
    master_ip           = ip_a_addr(0,0,0,0);
    master_timestamp    = 0;
}

void ntp_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time ){

    *system_time = tmr_u32_get_system_time_ms();

    *ntp_now = ntp_t_from_system_time( *system_time );   
}

// return true if given clock is better than current master
static bool compare_clock( ip_addr4_t source_ip, uint64_t source_timestamp, uint8_t source ){

    // check if better source:
    if( source > clock_source ){

        return TRUE;
    }

    // check if worse source:
    else if( source < clock_source ){

        return FALSE; // cannot be a match
    }

    // sources match

    // check if older timestamp
    if( source_timestamp > master_timestamp ){

        return TRUE;
    }

    return FALSE;
}

static bool is_master( void ){
    
    if( !ntp_b_is_sync() ){

        return FALSE;
    }

    if( ip_b_is_zeroes( master_ip ) ){

        return FALSE;
    }    

    ip_addr4_t local_ip = cfg_ip_get_ipaddr();

    return ip_b_addr_compare( local_ip, master_ip );
}

void ntp_v_set_master_clock( 
    ntp_ts_t source_ntp, 
    ip_addr4_t source_ip,
    uint64_t source_timestamp,
    uint8_t source ){

    if( ip_b_is_zeroes( source_ip ) ){

        source_ip = cfg_ip_get_ipaddr();
    }

    // check if this is the current local master
    if( !ip_b_addr_compare( source_ip, master_ip ) ){

        // NOT local master, check comparison!
        if( !compare_clock( source_ip, source_timestamp, source ) ){

            return;
        }
    }

    uint64_t local_system_time_ms = tmr_u64_get_system_time_ms();

    // get current NTP timestamp from the given system timestamp:
    ntp_ts_t local_ntp = ntp_t_from_system_time( local_system_time_ms );
    
    // ok, now we have what time we *think* it is
    // let's get a delta from the actual time being given to us
    // from whoever called this function:

    // since the complete NTP timestamp is really a u64, we don't have an easy way to do 
    // an integer diff (that would need 128 bit signed integers), so we'll do it piecemeal
    // on the seconds and then the fraction:
    int64_t delta_ntp_seconds = (int64_t)local_ntp.seconds - 
                                (int64_t)source_ntp.seconds;

    int16_t delta_ntp_fraction_ms = (int16_t)ntp_u16_get_fraction_as_ms( local_ntp ) - 
                                    (int16_t)ntp_u16_get_fraction_as_ms( source_ntp );
    
    // reassemble to a 64 bit millisecond integer:
    int64_t delta_ms = delta_ntp_seconds * 1000 + delta_ntp_fraction_ms;


    // check if delta exceeds the hard sync threshold,
    // or the clock has not been previously set:

    // delta is local - source
    // therefore:
    // if delta is positive, our local clock is ahead of the source clock
    // if delta is negative, our local clock is behind the source clock

    // assign clock source:
    if( clock_source != source ){

        log_v_debug_P( PSTR("NTP source changed from %d to %d"), clock_source, source );

        clock_source = source;
    }

    if( ( clock_source <= NTP_SOURCE_NONE ) || ( abs64( delta_ms ) >= NTP_HARD_SYNC_THRESHOLD_MS ) ){

        // hard sync: just jolt the clock into sync

        master_ntp_time = source_ntp;
        master_sys_time_ms = local_system_time_ms;
        master_sync_delta = 0;

        char time_str[ISO8601_STRING_MIN_LEN_MS];
        ntp_v_to_iso8601( time_str, sizeof(time_str), ntp_t_now() );

    
        // log a message for hard syncs if we were previously synced
        // we can skip the initial clock setting, the main clock
        // thread will log it already.
        log_v_info_P( PSTR("NTP Time is now: %s [hard sync]"), time_str );    
    }
    else{

        // soft sync: slowly slew the clock into the correct position

        master_sync_delta = delta_ms;

        // log_v_debug_P( PSTR("NTP sync diff: %ld [soft sync]"), delta_ms );
    }
    
    last_sync_time = tmr_u32_get_system_time_ms();

    // set master
    master_ip = source_ip;
    master_timestamp = source_timestamp;
}

// this will compute the current NTP time from the current clock
// information even if the clock has not be synchronized
ntp_ts_t ntp_t_from_system_time( uint64_t sys_time_ms ){

    // we are using 64 bit millisecond timestamps for the system time,
    // driven by the hardware system timer.
    // this timer will never roll over (500+ million years)
    // and is not reset during runtime.

    // however, we still convert to a signed int64, since
    // we might request an NTP timestamp from before the current
    // master system time setting.

    int64_t elapsed_ms = (int64_t)sys_time_ms - (int64_t)master_sys_time_ms;

    // add elapsed time to NTP timestamp - this is easier done by converting
    // the NTP timestamp to a 64 bit integer
    uint64_t now = ntp_u64_conv_to_u64( master_ntp_time );


    if( elapsed_ms > 0 ){

        now += ( ( (uint64_t)elapsed_ms << 32 ) / 1000 ); 
    }
    else if( elapsed_ms < 0 ){

        now -= ( ( (uint64_t)( -1 * elapsed_ms ) << 32 ) / 1000 ); 
    }

    return ntp_ts_from_u64( now );
}

ntp_ts_t ntp_t_now( void ){

    // check if syncrhonized
    if( clock_source == NTP_SOURCE_NONE ){

        return ntp_ts_from_u64( 0 );    
    }

    return ntp_t_from_system_time( tmr_u64_get_system_time_ms() );
}

// get NTP time with timezone correction
ntp_ts_t ntp_t_local_now( void ){

    // check if syncrhonized
    if( clock_source == NTP_SOURCE_NONE ){

        return ntp_ts_from_u64( 0 );    
    }

    // get NTP time
    ntp_ts_t ntp = ntp_t_now();  

    // adjust seconds by timezone offset
    // tz_offset is in minutes, so also convert to seconds
    int32_t tz_seconds = tz_offset * 60;
    ntp.seconds += tz_seconds;

    return ntp;
}

bool ntp_b_is_sync( void ){

    return clock_source > NTP_SOURCE_NONE;
}

void ntp_v_transmit( ntp_ts_t source_ntp, uint8_t source ){

    ntp_msg_clock_t msg = {
        .magic = NTP_PROTOCOL_MAGIC,
        .version = NTP_PROTOCOL_VERSION,
        .type = NTP_MSG_CLOCK,
        .source = source,
        .origin_timestamp = tmr_u64_get_system_time_us(),
        .ntp_timestamp = source_ntp,
    };

    sock_addr_t raddr = {
        {255, 255, 255, 255},
        NTP_SERVER_PORT
    };
    
    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}

static bool ntp_b_clock_source_timed_out( void ){

    uint32_t delta_ms = tmr_u32_elapsed_time_ms( last_sync_time );

    return delta_ms > ( NTP_MASTER_CLOCK_TIMEOUT * 1000 );
}


PT_THREAD( ntp_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while( TRUE ){

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 4000 + ( rnd_u16_get_int() >> 4 ) );
        THREAD_WAIT_WHILE( pt, !ntp_b_is_sync() && thread_b_alarm_set() );

        if( !ntp_b_is_sync() && ( sntp_u8_get_status() == SNTP_STATUS_DISABLED ) ){

            log_v_info_P( PSTR("Starting local clock") );

            // start local clock
            master_ip = cfg_ip_get_ipaddr();
            master_timestamp = tmr_u64_get_system_time_us();

            // start SNTP
            sntp_v_start();

            continue;
        }

        if( !ntp_b_is_sync() ){

            continue;
        }

        prev_source = clock_source;

        char time_str[ISO8601_STRING_MIN_LEN_MS];
        ntp_v_to_iso8601( time_str, sizeof(time_str), ntp_t_now() );
        log_v_info_P( PSTR("NTP Time is now: %s source: %u"), time_str, clock_source );

        thread_v_set_alarm( tmr_u32_get_system_time_ms() );

        while( ntp_b_is_sync() ){

            thread_v_set_alarm( thread_u32_get_alarm() + 1000 );

            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( clock_source == prev_source ) );

            // check if master clock
            if( is_master() ){

                // check if we should enable SNTP
                if( clock_source <= NTP_SOURCE_SNTP ){

                    // start SNTP
                    sntp_v_start();
                }
                // check if we should disable SNTP
                else if( clock_source > NTP_SOURCE_SNTP ){

                    sntp_v_stop();
                }
            }
            else{

                // not a master clock, no SNTP!
                sntp_v_stop();
            }


            // check for master clock timeout
            // this is a slow process                            
            if( ntp_b_clock_source_timed_out() ){

                // set source to internal
                clock_source = NTP_SOURCE_INTERNAL;
                master_ip = cfg_ip_get_ipaddr();
                master_timestamp = tmr_u64_get_system_time_us();
                log_v_info_P( PSTR("NTP master clock desync, changing source to internal.") );
            }

            if( clock_source == NTP_SOURCE_INTERNAL ){

                last_sync_time = tmr_u32_get_system_time_ms();
            }

            // check if clock source changed
            if( prev_source != clock_source ){

                log_v_info_P( PSTR("NTP clock source changed to: %d from: %d"), clock_source, prev_source );

                THREAD_RESTART( pt );
            }

            prev_source = clock_source;
            
            // get actual current timestamp, since the thread timing
            // won't be exact:

            uint64_t sys_time_ms = tmr_u64_get_system_time_ms();

            // check for bad timestamps, IE, the current system time
            // is somehow lower than the master timestamp:
            if( sys_time_ms < master_sys_time_ms ){

                // something has gone wrong at this point, so log this and then restart
                // the clock

                log_v_error_P( PSTR("Master system time mismatch: %lu != system clock: %lu"), master_sys_time_ms, sys_time_ms );

                reset_clock();
                THREAD_RESTART( pt );            
            }

            uint64_t elapsed_ms = sys_time_ms - master_sys_time_ms;

            // update base system time:
            master_sys_time_ms += elapsed_ms;

            // update master timestamp
            master_timestamp += elapsed_ms * 1000;

            // check sync delta:
            // positive deltas mean our clock is ahead
            // negative deltas mean out clock is behind

            int16_t clock_adjust = 0;

            if( master_sync_delta > 0 ){

                // local clock is ahead
                // we need to slow down a bit

                if( master_sync_delta > 500 ){

                    clock_adjust = 10;
                }
                else if( master_sync_delta > 200 ){

                    clock_adjust = 5;
                }
                else if( master_sync_delta > 50 ){

                    clock_adjust = 2;
                }
                else{

                    clock_adjust = 1;
                }
            }
            else if( master_sync_delta < 0 ){

                // local clock is behind
                // we need to speed up a bit

                if( master_sync_delta < -500 ){

                    clock_adjust = -10;
                }
                else if( master_sync_delta < -200 ){

                    clock_adjust = -5;
                }
                else if( master_sync_delta < -50 ){

                    clock_adjust = -2;
                }
                else{

                    clock_adjust = -1;
                }
            }

            master_sync_delta -= clock_adjust;

            // adjust elapsed ms by clock adjustment:
            elapsed_ms -= clock_adjust;

            // compute updated NTP timestamp:
            uint64_t ntp_now_u64 = ntp_u64_conv_to_u64( master_ntp_time );
            ntp_now_u64 += ( ( (int64_t)elapsed_ms << 32 ) / 1000 ); 

            // update master NTP timestamp:
            master_ntp_time = ntp_ts_from_u64( ntp_now_u64 );


            // check if we are a leader:
            if( is_master() ){

                uint8_t broadcast_source = clock_source;

                // change sources to net versions, if needed
                if( clock_source == NTP_SOURCE_SNTP ){

                    broadcast_source = NTP_SOURCE_SNTP_NET;
                }
                else if( clock_source == NTP_SOURCE_GPS ){

                    broadcast_source = NTP_SOURCE_GPS_NET;
                }
                else if( clock_source == NTP_SOURCE_INTERNAL ){

                    broadcast_source = NTP_SOURCE_INTERNAL_NET;
                }

                ntp_v_transmit( ntp_t_now(), broadcast_source );
            }

        }

        // lost sync

        log_v_warn_P( PSTR("lost NTP sync") );

        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}


PT_THREAD( ntp_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // enable timeout
    sock_v_set_timeout( sock, 1 );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check for received data
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            // timeout

            continue;
        }


        // process received message

        uint32_t *magic = sock_vp_get_data( sock );

        if( *magic != NTP_PROTOCOL_MAGIC ){

            continue;
        }

        uint8_t *version = (uint8_t *)(magic + 1);

        if( *version != NTP_PROTOCOL_VERSION ){

            continue;
        }

        const uint8_t *type = version + 1;

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        // check if message if from US, ignore if so
        if( ip_b_check_dest( raddr.ipaddr ) ){

            continue;
        }

        if( *type == NTP_MSG_CLOCK ){

            const ntp_msg_clock_t *msg = (ntp_msg_clock_t *)magic;
            
            ntp_v_set_master_clock( msg->ntp_timestamp, raddr.ipaddr, msg->origin_timestamp, msg->source );
        }
        else{

            // invalid message
                
            log_v_error_P( PSTR("invalid msg") );

            continue;    
        }
    }
    
PT_END( pt );
}



#endif
