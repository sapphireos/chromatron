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

// #define NO_LOGGING
#include "sapphire.h"

#ifdef ENABLE_TIME_SYNC

#include "config.h"
#include "timesync.h"
#include "hash.h"
#include "sntp.h"
#include "util.h"
#include "services.h"

PT_THREAD( time_server_thread( pt_t *pt, void *state ) );
PT_THREAD( time_master_thread( pt_t *pt, void *state ) );
PT_THREAD( time_clock_thread( pt_t *pt, void *state ) );

static socket_t sock;

static ip_addr4_t master_ip;
static uint8_t master_source;
static uint8_t local_source;
static bool is_sync;
static bool ntp_valid;

// master clock
static ntp_ts_t master_ntp_time;
static uint32_t master_net_time;
static uint32_t last_clock_update;
static uint32_t base_system_time;
static int32_t ntp_sync_difference;
static int32_t master_sync_difference;

static bool gps_sync;


static uint8_t sync_id;
static uint32_t rtt_start;
static uint16_t filtered_rtt;
#define RTT_FILTER              32
#define RTT_QUALITY_LIMIT       10
#define RTT_RELAX               4
static uint8_t sync_delay;

static uint32_t good_syncs;
static uint32_t rejected_syncs;

// NOTE!
// tz_offset is in MINUTES, because not all timezones
// are aligned on the hour.
static int16_t tz_offset;




static int8_t ntp_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

        uint32_t elapsed = 0;
        uint32_t seconds = 0;

        if( ntp_valid ){
            
            elapsed = tmr_u32_elapsed_time_ms( base_system_time );
            seconds = master_ntp_time.seconds + ( elapsed / 1000 );
        }

        memcpy( data, &seconds, len );

        return 0;
    }

    return -1;
}


KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,       0, 0,  0,                                 cfg_i8_kv_handler,      "enable_time_sync" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &master_net_time,  0,                      "net_time" },
    { SAPPHIRE_TYPE_IPv4,       0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &master_source,    0,                      "net_time_master_source" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &local_source,     0,                      "net_time_local_source" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &filtered_rtt,     0,                      "net_time_filtered_rtt" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, 0,                 ntp_kv_handler,         "ntp_seconds" },
    { SAPPHIRE_TYPE_INT16,      0, KV_FLAGS_PERSIST,   &tz_offset,        0,                      "datetime_tz_offset" },

    { SAPPHIRE_TYPE_INT32,      0, KV_FLAGS_READ_ONLY, &master_sync_difference,          0,       "net_master_sync_diff" },

    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &good_syncs,       0,                      "net_time_good_syncs" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &rejected_syncs,   0,                      "net_time_rejected_syncs" },
};


void hal_rtc_v_irq( void ){


}

void time_v_init( void ){

    // January 1, 2018, midnight
    master_ntp_time.seconds = 1514786400 + 2208988800 - 21600;

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    // check if time sync is enabled
    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        return;
    }

    sock = sock_s_create( SOS_SOCK_DGRAM );

    sock_v_bind( sock, TIME_SERVER_PORT );


    thread_t_create( time_server_thread,
                    PSTR("time_server"),
                    0,
                    0 );    

    thread_t_create( time_master_thread,
                    PSTR("time_master"),
                    0,
                    0 );    
    
    thread_t_create( time_clock_thread,
                    PSTR("time_clock"),
                    0,
                    0 );    
}

void time_v_handle_shutdown( ip_addr4_t ip ){

    
}

bool time_b_is_local_sync( void ){

    return is_sync;
}

bool time_b_is_ntp_sync( void ){

    return ntp_valid;
}

uint32_t time_u32_get_network_time( void ){
    
    uint32_t elapsed_ms = tmr_u32_elapsed_times( base_system_time, tmr_u32_get_system_time_ms() );

    return master_net_time + elapsed_ms;
}

uint32_t time_u32_get_network_time_from_local( uint32_t local_time ){
    
    uint32_t elapsed_ms = tmr_u32_elapsed_times( base_system_time, local_time );

    return master_net_time + elapsed_ms;
}

// returns 1 if time > network_time,
// -1 if time < network_time,
// 0 if equal
int8_t time_i8_compare_network_time( uint32_t time ){
    
    int32_t distance = ( int32_t )( time - time_u32_get_network_time() );
    
    if( distance < 0 ){
    
        return -1;
    }
    else if( distance > 0 ){
    
        return 1;
    }
    else{
    
        return 0;
    }
}

uint32_t time_u32_get_network_aligned( uint32_t alignment ){

    uint32_t net_time = time_u32_get_network_time();

    return net_time + ( alignment - net_time % alignment );
}

void time_v_set_gps_sync( bool sync ){

    gps_sync = sync;
}


ntp_ts_t time_t_from_system_time( uint32_t end_time ){

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

            log_v_debug_P( PSTR("negative elapsed time: %ld"), elapsed_ms ); 
        }
    }

    

    uint64_t now = ntp_u64_conv_to_u64( master_ntp_time );

    // log_v_debug_P( PSTR("base: %lu now: %lu elapsed: %lu"), base_system_time, end_time, elapsed_ms );

    now += ( ( (int64_t)elapsed_ms << 32 ) / 1000 ); 
    
    return ntp_ts_from_u64( now ); 
}

static bool is_leader( void ){

    return services_b_is_server( TIME_ELECTION_SERVICE, 0 );
}

static bool is_follower( void ){

    return !is_leader() && services_b_is_available( TIME_ELECTION_SERVICE, 0 );
}

static void time_v_set_ntp_master_clock_internal( 
    ntp_ts_t source_ts, 
    uint32_t source_net_time,
    uint32_t local_system_time,
    uint8_t source,
    uint16_t delay ){    

    master_source = source;

    // if source is usable to sync ntp, set ntp valid.
    if( source > TIME_SOURCE_INTERNAL ){

        if( !ntp_valid ){

            // log_v_debug_P( PSTR("NTP valid") );
        }

        ntp_valid = TRUE;
    }

    // adjust local time with delay.
    // local timestamp now matches when the sync message was transmitted, rather than received.
    local_system_time -= delay;

    ntp_ts_t local_ntp_ts = time_t_from_system_time( local_system_time );
    uint32_t local_net_time = time_u32_get_network_time_from_local( local_system_time );

    // get deltas
    int64_t delta_ntp_seconds = (int64_t)local_ntp_ts.seconds - (int64_t)source_ts.seconds;
    int16_t delta_ntp_ms = (int16_t)ntp_u16_get_fraction_as_ms( local_ntp_ts ) - (int16_t)ntp_u16_get_fraction_as_ms( source_ts );
    int32_t delta_master_ms = (int64_t)local_net_time - (int64_t)source_net_time;

    // char s[ISO8601_STRING_MIN_LEN_MS];
    // ntp_v_to_iso8601( s, sizeof(s), local_ntp_ts );
    // log_v_debug_P( PSTR("Local:    %s"), s );
    // ntp_v_to_iso8601( s, sizeof(s), source_ts );
    // log_v_debug_P( PSTR("Remote:   %s"), s );

    if( ( abs64( delta_master_ms ) > 60000 ) ||
        ( abs64( delta_ntp_seconds ) > 60 ) ||
        !is_sync ){

        // log_v_debug_P( PSTR("HARD SYNC") );
        
        // hard sync
        master_ntp_time         = source_ts;
        master_net_time         = source_net_time;
        base_system_time        = local_system_time;
        last_clock_update       = local_system_time;
        ntp_sync_difference     = 0;
        master_sync_difference  = 0;

        is_sync = TRUE;

        return;
    }

    // gradual adjustment

    // set difference
    ntp_sync_difference = ( delta_ntp_seconds * 1000 ) + delta_ntp_ms;
    master_sync_difference = delta_master_ms;

    // log_v_debug_P( PSTR("ntp_sync_difference: %ld"), ntp_sync_difference );   
    // log_v_debug_P( PSTR("dly: %3u diff: %5ld"), delay, master_sync_difference );   
}

void time_v_set_ntp_master_clock( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source ){

    if( !is_leader() ){

        // our source isn't as good as the master, don't do anything.
        if( source <= master_source ){

            return;
        }
    }

    time_v_set_ntp_master_clock_internal( source_ts, local_system_time, local_system_time, source, 0 );
}

ntp_ts_t time_t_now( void ){

    return time_t_from_system_time( tmr_u32_get_system_time_ms() );
}

// get NTP time with timezone correction
ntp_ts_t time_t_local_now( void ){

    // get NTP time
    ntp_ts_t ntp = time_t_now();  

    // adjust seconds by timezone offset
    // tz_offset is in minutes, so also convert to seconds
    int32_t tz_seconds = tz_offset * 60;
    ntp.seconds += tz_seconds;

    return ntp;
}

static uint8_t get_best_local_source( void ){

    if( gps_sync ){

        return TIME_SOURCE_GPS;
    }

    if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

        return TIME_SOURCE_NTP;
    }

    if( is_sync && ntp_valid ){

        return TIME_SOURCE_INTERNAL_NTP_SYNC;
    }

    if( is_sync ){

        return TIME_SOURCE_INTERNAL;
    }

    return TIME_SOURCE_NONE;
}


static void transmit_ping( void ){

    time_msg_ping_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_PING;
    
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = master_ip;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}

static void transmit_sync_request( void ){

    sync_id++;

    time_msg_request_sync_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_REQUEST_SYNC;
    msg.id              = sync_id;
    
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = master_ip;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  

    rtt_start = tmr_u32_get_system_time_ms();
}

static void request_sync( void ){

    // transmit a ping first.
    // this ensures the server will have an ARP entry ready when
    // when we send the sync request
    transmit_ping();
}

PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

            uint32_t now = tmr_u32_get_system_time_ms();

            uint32_t *magic = sock_vp_get_data( sock );

            if( *magic != TIME_PROTOCOL_MAGIC ){

                continue;
            }

            uint8_t *version = (uint8_t *)(magic + 1);

            if( *version != TIME_PROTOCOL_VERSION ){

                continue;
            }

            uint8_t *type = version + 1;

            sock_addr_t raddr;
            sock_v_get_raddr( sock, &raddr );

            if( *type == TIME_MSG_REQUEST_SYNC ){

                if( !is_leader() ){

                    continue;
                }

                time_msg_request_sync_t *req = (time_msg_request_sync_t *)magic;

                if( sock_i16_get_bytes_read( sock ) != sizeof(time_msg_request_sync_t) ){

                    log_v_debug_P( PSTR("invalid len") );
                }

                time_msg_sync_t msg;
                msg.magic           = TIME_PROTOCOL_MAGIC;
                msg.version         = TIME_PROTOCOL_VERSION;
                msg.type            = TIME_MSG_SYNC;
                msg.net_time        = time_u32_get_network_time_from_local( now );
                msg.flags           = 0;
                msg.ntp_time        = time_t_from_system_time( now );
                msg.source          = master_source;
                msg.id              = req->id;

                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
            else if( *type == TIME_MSG_PING ){

                if( !is_leader() ){

                    continue;
                }

                time_msg_ping_response_t msg;
                msg.magic           = TIME_PROTOCOL_MAGIC;
                msg.version         = TIME_PROTOCOL_VERSION;
                msg.type            = TIME_MSG_PING_RESPONSE;
                
                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
            else if( *type == TIME_MSG_PING_RESPONSE ){

                if( is_leader() ){

                    continue;
                }

                transmit_sync_request();
            }
            else if( *type == TIME_MSG_SYNC ){

                if( is_leader() ){

                    continue;
                }

                time_msg_sync_t *msg = (time_msg_sync_t *)magic;

                if( sock_i16_get_bytes_read( sock ) != sizeof(time_msg_sync_t) ){

                    log_v_debug_P( PSTR("invalid len") );
                }

                // check id
                if( msg->id != sync_id ){

                    log_v_debug_P( PSTR("bad sync id: %u != %u"), msg->id, sync_id );

                    rejected_syncs++;
                    continue;
                }

                // uint32_t est_net_time = time_u32_get_network_time();
                
                uint32_t elapsed_rtt = tmr_u32_elapsed_times( rtt_start, now );

                // init filtered RTT if needed
                if( filtered_rtt == 0 ){

                    filtered_rtt = elapsed_rtt;
                }

                if( elapsed_rtt > 500 ){

                    // a 0.5 second RTT is clearly ridiculous.
                    // log_v_debug_P( PSTR("bad: RTT: %u"), elapsed_rtt );

                    rejected_syncs++;

                    continue;
                }
                // check quality of RTT sample
                else if( elapsed_rtt > ( filtered_rtt + (uint32_t)RTT_QUALITY_LIMIT ) ){

                    // log_v_debug_P( PSTR("poor quality: RTT: %u"), elapsed_rtt );

                    // although we are not using this sync message for our clock,
                    // we will bump the filtered RTT up a bit in case the overall RTT is
                    // drifting upwards.  this prevents us from losing sync on a busy network.
                        
                    filtered_rtt += RTT_RELAX;

                    rejected_syncs++;

                    continue;
                }
                else{

                    // filter rtt
                    filtered_rtt = util_u16_ewma( elapsed_rtt, filtered_rtt, RTT_FILTER );
                }

                good_syncs++;

                // check sync
                if( msg->source != TIME_SOURCE_NONE ){
                    
                    // log_v_debug_P( PSTR("%4ld %4u"), elapsed_rtt, filtered_rtt );

                    uint16_t delay = elapsed_rtt / 2;

                    time_v_set_ntp_master_clock_internal( msg->ntp_time, msg->net_time, now, msg->source, delay );

                    // each time we get a valid sync, we bump the sync delay up until the max
                    if( is_sync && ( sync_delay < TIME_SYNC_RATE_MAX ) ){
                
                       sync_delay++;
                    }
                }
            }
        }
    }


PT_END( pt );
}


PT_THREAD( time_master_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

        log_v_debug_P( PSTR("master clock RESET") );
        master_ip = ip_a_addr(0,0,0,0);

        // wait for service to resolve
        THREAD_WAIT_WHILE( pt, !services_b_is_available( TIME_ELECTION_SERVICE, 0 ) );

        log_v_debug_P( PSTR("master clock found") );
        master_ip = services_a_get_ip( TIME_ELECTION_SERVICE, 0 );

        while( is_leader() ){

            // default sync source to internal clock
            master_source = TIME_SOURCE_INTERNAL;

            // check sync sources
            if( gps_sync ){

                sntp_v_stop();
                master_source = TIME_SOURCE_GPS;
            }
            else{

                if( sntp_u8_get_status() == SNTP_STATUS_DISABLED ){

                    log_v_debug_P( PSTR("master starting sntp") );

                    // start SNTP (ignored if already running)
                    sntp_v_start();
                }
                // check if synchronized
                else if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

                    master_source = TIME_SOURCE_NTP;
                }
            }

            TMR_WAIT( pt, 1000 );
        }

        if( is_follower() ){

            // reset sync
            is_sync = FALSE;

            sntp_v_stop();

            log_v_debug_P( PSTR("follower, reset sync") );

            sync_delay = TIME_SYNC_RATE_BASE;

            TMR_WAIT( pt, rnd_u16_get_int() >> 5 ); // random delay we don't dogpile the time master
        }

        while( is_follower() ){

            request_sync();

            // random delay + base sync delay
            uint16_t delay = ( sync_delay * 1000 ) + ( rnd_u16_get_int() >> 5 );
            TMR_WAIT( pt, delay );
        }
    }
    
PT_END( pt );
}

static uint16_t get_priority( void ){

    uint8_t source = get_best_local_source();

    uint16_t priority = source;

    #ifdef ESP32
    priority += TIME_SOURCE_ESP32_PRIORITY;
    #endif

    return priority;
}


PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    services_v_join_team( TIME_ELECTION_SERVICE, 0, get_priority(), TIME_SERVER_PORT );

    // wait until we resolve the election
    THREAD_WAIT_WHILE( pt, !services_b_is_available( TIME_ELECTION_SERVICE, 0 ) );

    last_clock_update = tmr_u32_get_system_time_ms();
    thread_v_set_alarm( last_clock_update );

    while( 1 ){

        master_ip = services_a_get_ip( TIME_ELECTION_SERVICE, 0 );
        local_source = get_best_local_source();
    
        // update election parameters (in case our source changes)        
        services_v_join_team( TIME_ELECTION_SERVICE, 0, get_priority(), TIME_SERVER_PORT );

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        uint32_t now = tmr_u32_get_system_time_ms();
        uint32_t elapsed_ms = tmr_u32_elapsed_times( last_clock_update, now );

        int16_t master_clock_adjust = 0;
        int16_t ntp_clock_adjust = 0;

        if( is_sync ){

            if( master_sync_difference > 50 ){

                // our clock is ahead, so we need to slow down
                master_clock_adjust = 10;
            }
            else if( master_sync_difference > 2 ){

                // our clock is ahead, so we need to slow down
                master_clock_adjust = 1;
            }
            else if( master_sync_difference < -50 ){

                // our clock is behind, so we need to speed up
                master_clock_adjust = -10;
            }
            else if( master_sync_difference < -2 ){

                // our clock is behind, so we need to speed up
                master_clock_adjust = -1;
            }

            master_sync_difference -= master_clock_adjust;

            // update master clocks
            master_net_time += elapsed_ms - master_clock_adjust;

            // update local reference
            base_system_time += elapsed_ms;  
        }

        if( ntp_valid ){

            uint64_t temp_ntp_master_u64 = ntp_u64_conv_to_u64( master_ntp_time );

            if( ntp_sync_difference > 50 ){

                // our clock is ahead, so we need to slow down
                ntp_clock_adjust = 10;
            }
            else if( ntp_sync_difference > 2 ){

                // our clock is ahead, so we need to slow down
                ntp_clock_adjust = 1;
            }
            else if( ntp_sync_difference < -50 ){

                // our clock is behind, so we need to speed up
                ntp_clock_adjust = -10;
            }
            else if( ntp_sync_difference < -2 ){

                // our clock is behind, so we need to speed up
                ntp_clock_adjust = -1;
            }

            ntp_sync_difference -= ntp_clock_adjust;            

            // update master clocks
            temp_ntp_master_u64 += ( ( (uint64_t)( elapsed_ms - ntp_clock_adjust ) << 32 ) / 1000 );
            master_ntp_time = ntp_ts_from_u64( temp_ntp_master_u64 );
        }

        last_clock_update = now;
    }

PT_END( pt );
}



#endif

