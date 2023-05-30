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

#ifdef ENABLE_TIME_SYNC

#include "config.h"
#include "time_ntp.h"
#include "sntp.h"

/*

Track long term sync delay and elapsed differences.
Try to average to a long term average sync accuracy.

It would be cool to see the difference in oscillator
speeds between devices, which over a long enough
averaging period, should be discernible.



*/


static socket_t sock;

// master clock:
static uint8_t prev_source;
static uint8_t clock_source;
static ntp_ts_t master_ntp_time;
uint64_t master_sys_time_ms; // system timestamp that correlates to NTP timestamp
static int16_t master_sync_delta;

static uint32_t last_sync_time;

static ip_addr4_t master_ip;

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
    { CATBUS_TYPE_IPv4,     0, KV_FLAGS_READ_ONLY, &master_ip,                  0,                  "ntp_master_ip" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, 0,                           _ntp_kv_handler,    "ntp_elapsed_last_sync" },

    { CATBUS_TYPE_INT16,    0, KV_FLAGS_PERSIST,   &tz_offset,                  0,                  "datetime_tz_offset" },
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

    // sock_v_bind( sock, NTP_SERVER_PORT );

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

    master_ntp_time = ntp_ts_from_u64( 0 );
    master_sys_time_ms = 0;
    master_sync_delta = 0;
}

static bool is_leader( void ){

    return services_b_is_server( NTP_ELECTION_SERVICE, 0 );
}

static bool is_service_avilable( void ){

    return services_b_is_available( NTP_ELECTION_SERVICE, 0 );
}

static bool is_follower( void ){

    return !is_leader() && is_service_avilable();
}

static uint16_t get_priority( void ){

    return clock_source;
}

void ntp_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time ){

    *system_time = tmr_u32_get_system_time_ms();

    *ntp_now = ntp_t_from_system_time( *system_time );   
}

void ntp_v_set_master_clock( 
    ntp_ts_t source_ntp, 
    uint64_t local_system_time_ms,
    uint8_t source ){

    // check if incoming source is better than SNTP:
    if( source > NTP_SOURCE_SNTP ){

        // make sure sntp service is stopped
        sntp_v_stop();
    }

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

    if( ( clock_source <= NTP_SOURCE_NONE ) || ( abs64( delta_ms ) >= NTP_HARD_SYNC_THRESHOLD_MS ) ){

        // hard sync: just jolt the clock into sync

        master_ntp_time = source_ntp;
        master_sys_time_ms = local_system_time_ms;
        master_sync_delta = 0;

        // assign clock source:
        clock_source = source;

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

    // assign clock source:
    clock_source = source;

    last_sync_time = tmr_u32_get_system_time_ms();
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


PT_THREAD( ntp_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while( TRUE ){

        THREAD_WAIT_WHILE( pt, !ntp_b_is_sync() );

        prev_source = clock_source;

        char time_str[ISO8601_STRING_MIN_LEN_MS];
        ntp_v_to_iso8601( time_str, sizeof(time_str), ntp_t_now() );
        log_v_info_P( PSTR("NTP Time is now: %s source: %u"), time_str, clock_source );

        thread_v_set_alarm( tmr_u32_get_system_time_ms() );

        while( ntp_b_is_sync() ){

            thread_v_set_alarm( thread_u32_get_alarm() + 1000 );

            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( clock_source == prev_source ) );

            master_ip = services_a_get_ip( NTP_ELECTION_SERVICE, 0 );

            // check for master clock timeout
            // this is a slow process
            if( ntp_b_is_sync() ){

                uint32_t delta_ms = tmr_u32_elapsed_time_ms( last_sync_time );

                if( ( clock_source > NTP_SOURCE_INTERNAL ) && ( delta_ms > ( NTP_MASTER_CLOCK_TIMEOUT * 1000 ) ) ){

                    log_v_info_P( PSTR("NTP master clock desync, changing source to internal: %u"), delta_ms );

                    clock_source = NTP_SOURCE_INTERNAL;           
                }
            }

            // update service priorities
            services_v_join_team( NTP_ELECTION_SERVICE, 0, get_priority(), sock_u16_get_lport( sock ) );

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

    // stop SNTP
    sntp_v_stop();

    // wait for network
    THREAD_WAIT_WHILE( pt, !wifi_b_connected() );
    
    services_v_join_team( NTP_ELECTION_SERVICE, 0, get_priority(), sock_u16_get_lport( sock ) );

    // wait until we resolve the election
    THREAD_WAIT_WHILE( pt, !is_service_avilable() );


    // check if we are the leader
    if( is_leader() ){

        // check if we should enable SNTP
        if( clock_source < NTP_SOURCE_SNTP ){

            // start SNTP
            sntp_v_start();    
        }

        // wait for NTP sync: we can't run the leader server without a sync
        // we also set a timeout, if for some reason we can't get an NTP sync
        // we will reset the thread
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + ( NTP_INITIAL_SNTP_SYNC_TIMEOUT * 1000 ) );
        THREAD_WAIT_WHILE( pt, !ntp_b_is_sync() && thread_b_alarm_set() );

        // check if we got a sync:
        if( !ntp_b_is_sync() ){

            log_v_warn_P( PSTR("SNTP initial sync timed out") );

            THREAD_RESTART( pt );
        }
    }
    else if( is_follower() ){

        // wait until leader has a clock source:
        THREAD_WAIT_WHILE( pt, is_follower() && services_u16_get_leader_priority( NTP_ELECTION_SERVICE, 0 ) <= NTP_SOURCE_NONE );
    }

    // service is available at this point
    // if a leader, we should have an NTP sync by now
    // if a follower, we might not.

    // leader loop: run server
    while( is_leader() ){

        // enable timeout
        sock_v_set_timeout( sock, 1 );

        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_leader() );

        if( !is_leader() ){

            THREAD_RESTART( pt );
        }

        // check if we should enable SNTP
        if( clock_source < NTP_SOURCE_SNTP ){

            // start SNTP
            sntp_v_start();    
        }

        // check if data received
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        uint32_t *magic = sock_vp_get_data( sock );

        if( *magic != NTP_PROTOCOL_MAGIC ){

            continue;
        }

        uint8_t *version = (uint8_t *)(magic + 1);

        if( *version != NTP_PROTOCOL_VERSION ){

            continue;
        }

        uint8_t *type = version + 1;

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( *type != NTP_MSG_REQUEST_SYNC ){

            // invalid message
                
            log_v_error_P( PSTR("invalid msg") );

            goto server_done;                
        }

        ntp_msg_request_sync_t *req = (ntp_msg_request_sync_t *)magic;


        ntp_msg_reply_sync_t reply = {
            NTP_PROTOCOL_MAGIC,
            NTP_PROTOCOL_VERSION,
            NTP_MSG_REPLY_SYNC,
            clock_source,
            req->origin_system_time_ms,
            ntp_t_now()
        };

        sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );  

server_done:
        THREAD_YIELD( pt );

    } // /leader


    sntp_v_stop();


    // follower, this runs a client:
    while( is_follower() && ( services_u16_get_leader_priority( NTP_ELECTION_SERVICE, 0 ) > NTP_SOURCE_NONE ) ){

        // send sync request

        ntp_msg_request_sync_t sync = {
            NTP_PROTOCOL_MAGIC,
            NTP_PROTOCOL_VERSION,
            NTP_MSG_REQUEST_SYNC,
            tmr_u64_get_system_time_ms(),
        };

        sock_v_flush( sock );

        sock_addr_t send_raddr = services_a_get( NTP_ELECTION_SERVICE, 0 );
        
        sock_i16_sendto( sock, (uint8_t *)&sync, sizeof(sync), &send_raddr );  

        sock_v_set_timeout( sock, 2 );

        // wait for reply or timeout
        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_follower() );

        // get local receive timestamp
        uint64_t reply_recv_timestamp = tmr_u64_get_system_time_ms();

        // check if service changed
        if( !is_follower() ){

            THREAD_RESTART( pt );
        }

        // check for timeout
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            goto client_done;
        }

        uint32_t *magic = sock_vp_get_data( sock );

        if( *magic != NTP_PROTOCOL_MAGIC ){

            goto client_done;
        }

        uint8_t *version = (uint8_t *)(magic + 1);

        if( *version != NTP_PROTOCOL_VERSION ){

            goto client_done;
        }

        uint8_t *type = version + 1;

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( *type != NTP_MSG_REPLY_SYNC ){

            // invalid message
                
            log_v_error_P( PSTR("invalid msg") );

            goto client_done;
        }

        ntp_msg_reply_sync_t *reply = (ntp_msg_reply_sync_t *)magic;
        
        // process reply
        int16_t packet_rtt = reply_recv_timestamp - reply->origin_system_time_ms;

        if( packet_rtt < 0 ){

            log_v_error_P( PSTR("NTP packet time traveled, giving up") );

            goto client_done;
        }

        uint16_t packet_delay = packet_rtt / 2; // assuming link is symmetrical

        // adjust the transmit NTP timestamp by the packet delay so
        // it correlates with the packet receive timestamp:


        // convert delay milliseconds to NTP fraction:
        uint64_t delay_fraction = ( (uint64_t)packet_delay << 32 ) / 1000;

        // convert source NTP timestamp to u64:
        uint64_t source_timestamp = ntp_u64_conv_to_u64( reply->ntp_timestamp );

        // add delay:
        source_timestamp += delay_fraction;

        // convert back to NTP:
        ntp_ts_t delay_adjusted_ntp = ntp_ts_from_u64( source_timestamp );

        uint8_t source = NTP_SOURCE_INVALID;

        if( reply->source == NTP_SOURCE_SNTP ){

            source = NTP_SOURCE_SNTP_NET;
        }
        else if( reply->source == NTP_SOURCE_GPS ){

            source = NTP_SOURCE_GPS_NET;
        }
        else if( reply->source == NTP_SOURCE_INTERNAL ){

            source = NTP_SOURCE_INTERNAL_NET;
        }
        else if( ( reply->source == NTP_SOURCE_NONE ) ||
                 ( reply->source == NTP_SOURCE_INVALID ) ){

            goto client_done;
        }
        else{

            source = reply->source;
        }

        // set master clock with new timestamps
        ntp_v_set_master_clock( delay_adjusted_ntp, reply_recv_timestamp, source ); 
        

client_done:
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + ( NTP_SYNC_INTERVAL * 1000 ) );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && is_follower() );
    } // /follower


    THREAD_RESTART( pt );

PT_END( pt );
}




#endif
