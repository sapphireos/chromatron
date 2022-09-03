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

static socket_t sock;

// master clock:
static uint8_t prev_source;
static uint8_t clock_source;
static ntp_ts_t master_ntp_time;
static uint64_t master_sys_time_ms; // system timestamp that correlates to NTP timestamp
static int16_t master_sync_delta;

// NOTE!
// tz_offset is in MINUTES, because not all timezones
// are aligned on the hour.
static int16_t tz_offset;

static bool is_time_sync_enabled( void ){

    return cfg_b_get_boolean( __KV__enable_time_sync );
}

int8_t _ntp_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

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
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, &clock_source,               0,               "ntp_master_source" },
    { CATBUS_TYPE_UINT32,   0, 0,                  &master_ntp_time.seconds,    _ntp_kv_handler, "ntp_seconds" },
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_READ_ONLY, &master_sync_delta,          0,               "ntp_master_sync_delta" },
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_PERSIST,   &tz_offset,                  0,               "datetime_tz_offset" },
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

    master_ntp_time = ntp_ts_from_u64( 0 );
    master_sys_time_ms = 0;
    master_sync_delta = 0;
}

static uint8_t get_best_local_source( void ){

    // if( gps_sync ){

    //     return TIME_SOURCE_GPS;
    // }

    // if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

    //     return TIME_SOURCE_NTP;
    // }

    // if( is_sync && ntp_valid ){

    //     return TIME_SOURCE_INTERNAL_NTP_SYNC;
    // }

    // if( is_sync ){

    //     return TIME_SOURCE_INTERNAL;
    // }

    return NTP_SOURCE_NONE;
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

    uint8_t source = get_best_local_source();

    uint16_t priority = source;

    return priority;
}

void ntp_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time ){

    *system_time = tmr_u32_get_system_time_ms();

    *ntp_now = ntp_t_from_system_time( *system_time );   
}

void ntp_v_set_master_clock( 
    ntp_ts_t source_ntp, 
    uint64_t local_system_time_ms,
    uint8_t source ){

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

    if( ( clock_source == NTP_SOURCE_NONE ) || ( abs64( delta_ms ) >= NTP_HARD_SYNC_THRESHOLD_MS ) ){

        // hard sync: just jolt the clock into sync

        master_ntp_time = source_ntp;
        master_sys_time_ms = local_system_time_ms;
        master_sync_delta = 0;

        char time_str[ISO8601_STRING_MIN_LEN_MS];
        ntp_v_to_iso8601( time_str, sizeof(time_str), ntp_t_now() );

        if( ntp_b_is_sync() ){

            // log a message for hard syncs if we were previously synced
            // we can skip the initial clock setting, the main clock
            // thread will log it already.
            log_v_info_P( PSTR("NTP Time is now: %s [hard sync]"), time_str );    
        }
    }
    else{

        // soft sync: slowly slew the clock into the correct position

        master_sync_delta = delta_ms;

        log_v_debug_P( PSTR("NTP sync diff: %ld [soft sync]"), delta_ms );
    }


    // assign clock source:
    clock_source = source;
}

// this will compute the current NTP time from the current clock
// information even if the clock has not be synchronized
ntp_ts_t ntp_t_from_system_time( uint64_t sys_time_ms ){

    // we are using 64 bit millisecond timestamps for the system time,
    // driven by the hardware system timer.
    // this timer will never roll over (500+ million years)
    // and is not reset during runtime.

    // thus, elapsed time is a simple subtraction from the previous base
    // time.

    ASSERT( sys_time_ms >= master_sys_time_ms );

    uint64_t elapsed_ms = sys_time_ms - master_sys_time_ms;

    // add elapsed time to NTP timestamp - this is easier done by converting
    // the NTP timestamp to a 64 bit integer
    uint64_t now = ntp_u64_conv_to_u64( master_ntp_time );
    now += ( ( (int64_t)elapsed_ms << 32 ) / 1000 ); 

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

    return clock_source != NTP_SOURCE_NONE;
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

            // check if clock source changed
            if( prev_source != clock_source ){

                log_v_info_P( PSTR("NTP clock source changed") );

                THREAD_RESTART( pt );
            }


            // get actual current timestamp, since the thread timing
            // won't be exact:

            uint64_t sys_time_ms = tmr_u64_get_system_time_ms();

            // check for bad timestamps, IE, the current system time
            // is somehow lower than the master timestamp:
            if( sys_time_ms < master_sys_time_ms ){

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
    
    services_v_join_team( NTP_ELECTION_SERVICE, 0, get_priority(), NTP_SERVER_PORT );

    // wait until we resolve the election
    THREAD_WAIT_WHILE( pt, !services_b_is_available( NTP_ELECTION_SERVICE, 0 ) );


    // check if we are the leader
    if( is_leader() ){

        // later:
        // check if we have GPS here

        // start SNTP
        sntp_v_start();
    }

    THREAD_WAIT_WHILE( pt, !ntp_b_is_sync() && is_service_avilable() );

    if( !is_service_avilable() ){

        THREAD_RESTART( pt );
    }

    
    // NTP sync achieved here


    // leader loop: run server
    while( is_leader() ){

        // disable timeout
        sock_v_set_timeout( sock, 0 );

        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_leader() );

        if( !is_leader() ){

            THREAD_RESTART( pt );
        }

        // check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

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
                NTP_MSG_REQUEST_SYNC,
                req->origin_system_time_ms,
                ntp_t_now()
            };

            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );  

server_done:
            THREAD_YIELD( pt );
            
        } // /leader


        // follower, this runs a client:
        while( is_follower() ){

            // send sync request

            ntp_msg_request_sync_t sync = {
                NTP_PROTOCOL_MAGIC,
                NTP_PROTOCOL_VERSION,
                NTP_MSG_REQUEST_SYNC,
                tmr_u64_get_system_time_ms(),
            };


            sock_addr_t send_raddr;
            send_raddr.port = NTP_SERVER_PORT;
            send_raddr.ipaddr = services_a_get_ip( NTP_ELECTION_SERVICE, 0 );

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
            uint16_t packet_rtt = reply->origin_system_time_ms - reply_recv_timestamp;
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


            // set master clock with new timestamps
            ntp_v_set_master_clock( delay_adjusted_ntp, reply_recv_timestamp, NTP_SOURCE_SNTP_NET ); 
            




client_done:
            TMR_WAIT( pt, NTP_SYNC_INTERVAL * 1000 );
        } // /follower
    }


PT_END( pt );
}




#endif
