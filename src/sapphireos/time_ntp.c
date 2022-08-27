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

#include "time_ntp.h"
#include "sntp.h"

static socket_t sock;

// master clock:
static uint8_t clock_source;
static ntp_ts_t master_ntp_time;
static uint32_t master_sys_time; // system timestamp that correlates to NTP timestamp

// NOTE!
// tz_offset is in MINUTES, because not all timezones
// are aligned on the hour.
static int16_t tz_offset;


KV_SECTION_META kv_meta_t ntp_time_info_kv[] = {
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, &clock_source,               0, "ntp_master_source" },
    { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY, &master_ntp_time.seconds,    0, "ntp_seconds" },
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_PERSIST,   &tz_offset,                  0, "datetime_tz_offset" },
};

PT_THREAD( ntp_clock_thread( pt_t *pt, void *state ) );
PT_THREAD( ntp_server_thread( pt_t *pt, void *state ) );


void ntp_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    // check if time sync is enabled
    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        return;
    }    

    sock = sock_s_create( SOS_SOCK_DGRAM );

    sock_v_bind( sock, NTP_SERVER_PORT );
    
    thread_t_create( ntp_clock_thread,
                    PSTR("ntp_clock"),
                    0,
                    0 );        
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

static bool is_follower( void ){

    return !is_leader() && services_b_is_available( NTP_ELECTION_SERVICE, 0 );
}

static uint16_t get_priority( void ){

    uint8_t source = get_best_local_source();

    uint16_t priority = source;

    return priority;
}



// static void time_v_set_ntp_master_clock_internal( 
//     ntp_ts_t source_ts, 
//     uint32_t source_net_time,
//     uint32_t local_system_time,
//     uint8_t source,
//     uint16_t delay ){    

//     master_source = source;

//     // adjust local time with delay.
//     // local timestamp now matches when the sync message was transmitted, rather than received.
//     local_system_time -= delay;

//     ntp_ts_t local_ntp_ts = ntp_t_from_system_time( local_system_time );
//     uint32_t local_net_time = time_u32_get_network_time_from_local( local_system_time );

//     // get deltas
//     int64_t delta_ntp_seconds = (int64_t)local_ntp_ts.seconds - (int64_t)source_ts.seconds;
//     int16_t delta_ntp_ms = (int16_t)ntp_u16_get_fraction_as_ms( local_ntp_ts ) - (int16_t)ntp_u16_get_fraction_as_ms( source_ts );
//     int32_t delta_master_ms = (int64_t)local_net_time - (int64_t)source_net_time;

//     // char s[ISO8601_STRING_MIN_LEN_MS];
//     // ntp_v_to_iso8601( s, sizeof(s), local_ntp_ts );
//     // log_v_debug_P( PSTR("Local:    %s"), s );
//     // ntp_v_to_iso8601( s, sizeof(s), source_ts );
//     // log_v_debug_P( PSTR("Remote:   %s"), s );

//     if( ( abs64( delta_master_ms ) > 60000 ) ||
//         ( abs64( delta_ntp_seconds ) > 60 ) ||
//         ( !ntp_valid && ( source > TIME_SOURCE_INTERNAL) ) ||
//         !is_sync ){

//         log_v_debug_P( PSTR("clock synced: prev sync: %d delta_master: %d delta_ntp: %d"), is_sync, (int32_t)delta_master_ms, (int32_t)delta_ntp_seconds );
        
//         // hard sync
//         master_ntp_time         = source_ts;
//         master_net_time         = source_net_time;
//         base_system_time        = local_system_time;
//         last_clock_update       = local_system_time;
//         ntp_sync_difference     = 0;
//         master_sync_difference  = 0;

//         is_sync = TRUE;

//         if( source > TIME_SOURCE_INTERNAL ){
            
//             ntp_valid = TRUE;

//             char time_str[ISO8601_STRING_MIN_LEN_MS];
//             ntp_v_to_iso8601( time_str, sizeof(time_str), time_t_now() );

//             log_v_info_P( PSTR("NTP Time is now: %s"), time_str );
//         }

//         return;
//     }

//     // gradual adjustment

//     // set difference
//     ntp_sync_difference = ( delta_ntp_seconds * 1000 ) + delta_ntp_ms;
//     master_sync_difference = delta_master_ms;

//     // log_v_debug_P( PSTR("ntp_sync_difference: %ld"), ntp_sync_difference );   
//     // log_v_debug_P( PSTR("dly: %3u diff: %5ld"), delay, master_sync_difference );   
// }


void ntp_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time ){

    *system_time = tmr_u32_get_system_time_ms();

    *ntp_now = ntp_t_from_system_time( *system_time );   
}

void ntp_v_set_master_clock( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source ){




    // if( !is_leader() ){

    //     // our source isn't as good as the master, don't do anything.
    //     if( source <= master_source ){

    //         return;
    //     }
    // }

    // time_v_set_ntp_master_clock_internal( source_ts, local_system_time, local_system_time, source, 0 );
}

ntp_ts_t ntp_t_from_system_time( uint32_t end_time ){

    if( !ntp_valid ){

        return ntp_ts_from_u64( 0 );    
    }

    int32_t elapsed_ms = 0;

    if( end_time > base_system_time ){

        elapsed_ms = tmr_u32_elapsed_times( base_system_time, end_time );
    }
    else{

        // end_time is BEFORE base_system_time.
        // this can happen if the base_system_time increments in the clock thread
        // and we are computing from a timestamp with a negative offset applied (such as an RTT)
        // in this case, we can allow negative values for elapsed time.
        uint32_t temp = tmr_u32_elapsed_times( base_system_time, end_time );

        // value is, or should be, negative
        if( temp > ( UINT32_MAX / 2 ) ){

            elapsed_ms = ( UINT32_MAX - temp ) * -1;

            // log_v_debug_P( PSTR("negative elapsed time: %ld"), elapsed_ms ); 
        }
    }

    

    uint64_t now = ntp_u64_conv_to_u64( master_ntp_time );

    // log_v_debug_P( PSTR("base: %lu now: %lu elapsed: %lu"), base_system_time, end_time, elapsed_ms );

    now += ( ( (int64_t)elapsed_ms << 32 ) / 1000 ); 
    
    return ntp_ts_from_u64( now ); 
}

ntp_ts_t ntp_t_now( void ){

    return ntp_t_from_system_time( tmr_u32_get_system_time_ms() );
}

// get NTP time with timezone correction
ntp_ts_t ntp_t_local_now( void ){

    // get NTP time
    ntp_ts_t ntp = time_t_now();  

    // adjust seconds by timezone offset
    // tz_offset is in minutes, so also convert to seconds
    int32_t tz_seconds = tz_offset * 60;
    ntp.seconds += tz_seconds;

    return ntp;
}



PT_THREAD( ntp_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // wait for network
    THREAD_WAIT_WHILE( pt, !wifi_b_connected() );
    
    services_v_join_team( NTP_ELECTION_SERVICE, 0, get_priority(), NTP_SERVER_PORT );

    // wait until we resolve the election
    THREAD_WAIT_WHILE( pt, !services_b_is_available( NTP_ELECTION_SERVICE, 0 ) );

    // last_clock_update = tmr_u32_get_system_time_ms();
    // thread_v_set_alarm( last_clock_update );

    // while( 1 ){

    //     master_ip = services_a_get_ip( TIME_ELECTION_SERVICE, 0 );
    //     local_source = get_best_local_source();
    
    //     // update election parameters (in case our source changes)        
    //     services_v_join_team( TIME_ELECTION_SERVICE, 0, get_priority(), TIME_SERVER_PORT );

    //     thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
    //     THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

    //     uint32_t now = tmr_u32_get_system_time_ms();
    //     uint32_t elapsed_ms = tmr_u32_elapsed_times( last_clock_update, now );

    //     int16_t master_clock_adjust = 0;
    //     int16_t ntp_clock_adjust = 0;

    //     if( is_sync ){

    //         if( master_sync_difference > 50 ){

    //             // our clock is ahead, so we need to slow down
    //             master_clock_adjust = 10;
    //         }
    //         else if( master_sync_difference > 2 ){

    //             // our clock is ahead, so we need to slow down
    //             master_clock_adjust = 1;
    //         }
    //         else if( master_sync_difference < -50 ){

    //             // our clock is behind, so we need to speed up
    //             master_clock_adjust = -10;
    //         }
    //         else if( master_sync_difference < -2 ){

    //             // our clock is behind, so we need to speed up
    //             master_clock_adjust = -1;
    //         }

    //         master_sync_difference -= master_clock_adjust;

    //         // update master clocks
    //         master_net_time += elapsed_ms - master_clock_adjust;

    //         // update local reference
    //         base_system_time += elapsed_ms;  
    //     }

    //     if( ntp_valid ){

    //         uint64_t temp_ntp_master_u64 = ntp_u64_conv_to_u64( master_ntp_time );

    //         if( ntp_sync_difference > 50 ){

    //             // our clock is ahead, so we need to slow down
    //             ntp_clock_adjust = 10;
    //         }
    //         else if( ntp_sync_difference > 2 ){

    //             // our clock is ahead, so we need to slow down
    //             ntp_clock_adjust = 1;
    //         }
    //         else if( ntp_sync_difference < -50 ){

    //             // our clock is behind, so we need to speed up
    //             ntp_clock_adjust = -10;
    //         }
    //         else if( ntp_sync_difference < -2 ){

    //             // our clock is behind, so we need to speed up
    //             ntp_clock_adjust = -1;
    //         }

    //         ntp_sync_difference -= ntp_clock_adjust;            

    //         // update master clocks
    //         temp_ntp_master_u64 += ( ( (uint64_t)( elapsed_ms - ntp_clock_adjust ) << 32 ) / 1000 );
    //         master_ntp_time = ntp_ts_from_u64( temp_ntp_master_u64 );
    //     }

    //     last_clock_update = now;

    // }

PT_END( pt );
}


PT_THREAD( ntp_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

            uint32_t now = tmr_u32_get_system_time_ms();

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

        //     if( *type == TIME_MSG_REQUEST_SYNC ){

        //         if( !is_leader() ){

        //             continue;
        //         }

        //         time_msg_request_sync_t *req = (time_msg_request_sync_t *)magic;

        //         if( sock_i16_get_bytes_read( sock ) != sizeof(time_msg_request_sync_t) ){

        //             log_v_debug_P( PSTR("invalid len") );
        //         }

        //         time_msg_sync_t msg;
        //         msg.magic           = TIME_PROTOCOL_MAGIC;
        //         msg.version         = TIME_PROTOCOL_VERSION;
        //         msg.type            = TIME_MSG_SYNC;
        //         msg.net_time        = time_u32_get_network_time_from_local( now );
        //         msg.flags           = 0;
        //         msg.ntp_time        = time_t_from_system_time( now );
        //         msg.source          = master_source;
        //         msg.id              = req->id;

        //         sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
        //     }
        //     else if( *type == TIME_MSG_PING ){

        //         if( !is_leader() ){

        //             continue;
        //         }

        //         time_msg_ping_response_t msg;
        //         msg.magic           = TIME_PROTOCOL_MAGIC;
        //         msg.version         = TIME_PROTOCOL_VERSION;
        //         msg.type            = TIME_MSG_PING_RESPONSE;
                
        //         sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
        //     }
        //     else if( *type == TIME_MSG_PING_RESPONSE ){

        //         if( is_leader() ){

        //             continue;
        //         }

        //         transmit_sync_request();
        //     }
        //     else if( *type == TIME_MSG_SYNC ){

        //         if( is_leader() ){

        //             continue;
        //         }

        //         time_msg_sync_t *msg = (time_msg_sync_t *)magic;

        //         if( sock_i16_get_bytes_read( sock ) != sizeof(time_msg_sync_t) ){

        //             log_v_debug_P( PSTR("invalid len") );
        //         }

        //         // check id
        //         if( msg->id != sync_id ){

        //             // log_v_debug_P( PSTR("bad sync id: %u != %u"), msg->id, sync_id );

        //             rejected_syncs++;
        //             continue;
        //         }

        //         // uint32_t est_net_time = time_u32_get_network_time();
                
        //         uint32_t elapsed_rtt = tmr_u32_elapsed_times( rtt_start, now );

        //         // init filtered RTT if needed
        //         if( filtered_rtt == 0 ){

        //             filtered_rtt = elapsed_rtt;
        //         }

        //         if( elapsed_rtt > 500 ){

        //             // a 0.5 second RTT is clearly ridiculous.
        //             // log_v_debug_P( PSTR("bad: RTT: %u"), elapsed_rtt );

        //             rejected_syncs++;

        //             continue;
        //         }
        //         // check quality of RTT sample
        //         else if( elapsed_rtt > ( filtered_rtt + (uint32_t)RTT_QUALITY_LIMIT ) ){

        //             // log_v_debug_P( PSTR("poor quality: RTT: %u"), elapsed_rtt );

        //             // although we are not using this sync message for our clock,
        //             // we will bump the filtered RTT up a bit in case the overall RTT is
        //             // drifting upwards.  this prevents us from losing sync on a busy network.
                        
        //             filtered_rtt += RTT_RELAX;

        //             rejected_syncs++;

        //             continue;
        //         }
        //         else{

        //             // filter rtt
        //             filtered_rtt = util_u16_ewma( elapsed_rtt, filtered_rtt, RTT_FILTER );
        //         }

        //         good_syncs++;

        //         // check sync
        //         if( msg->source != TIME_SOURCE_NONE ){
                    
        //             // log_v_debug_P( PSTR("%4ld %4u"), elapsed_rtt, filtered_rtt );

        //             uint16_t delay = elapsed_rtt / 2;

        //             time_v_set_ntp_master_clock_internal( msg->ntp_time, msg->net_time, now, msg->source, delay );

        //             // each time we get a valid sync, we bump the sync delay up until the max
        //             if( is_sync && ( sync_delay < TIME_SYNC_RATE_MAX ) ){
                
        //                sync_delay++;
        //             }
        //         }
        //     }
        // }
    }


PT_END( pt );
}




#endif
