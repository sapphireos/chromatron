// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "esp8266.h"

PT_THREAD( time_server_thread( pt_t *pt, void *state ) );
PT_THREAD( time_master_thread( pt_t *pt, void *state ) );
PT_THREAD( time_clock_thread( pt_t *pt, void *state ) );

static socket_t sock;

static uint32_t local_time;
static uint32_t last_net_time;
static uint32_t net_time;
static ip_addr_t master_ip;
static uint64_t master_uptime;
static uint8_t master_source;
static bool is_sync;
static bool ntp_valid;

// master clock
static ntp_ts_t ntp_master_time;
static uint32_t last_clock_update;
static uint32_t base_system_time;
static int32_t ntp_sync_difference;

static bool gps_sync;

static uint8_t sync_state;
#define STATE_WAIT              0
#define STATE_MASTER            1
#define STATE_SLAVE             2


#define DRIFT_FILTER            32
static uint8_t drift_init;
static int16_t filtered_drift;

static uint32_t rtt_start;
static uint16_t filtered_rtt;
#define RTT_FILTER              32
#define RTT_QUALITY_LIMIT       10
#define RTT_RELAX               4

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

        uint32_t elapsed = tmr_u32_elapsed_time_ms( base_system_time );
        uint32_t seconds = ntp_master_time.seconds + ( elapsed / 1000 );

        memcpy( data, &seconds, len );

        return 0;
    }

    return -1;
}


KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,       0, 0,  0,                                 cfg_i8_kv_handler,      "enable_time_sync" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &net_time,         0,                      "net_time" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &sync_state,       0,                      "net_time_state" },
    { SAPPHIRE_TYPE_IPv4,       0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &master_source,    0,                      "net_time_master_source" },
    { SAPPHIRE_TYPE_UINT64,     0, KV_FLAGS_READ_ONLY, &master_uptime,    0,                      "net_time_master_uptime" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, 0,                 ntp_kv_handler,         "ntp_seconds" },
    { SAPPHIRE_TYPE_INT16,      0, KV_FLAGS_PERSIST,   &tz_offset,        0,                      "datetime_tz_offset" },
};


void hal_rtc_v_irq( void ){


}

void time_v_init( void ){

    // January 1, 2018, midnight
    ntp_master_time.seconds = 1514786400 + 2208988800 - 21600;

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    // check if time sync is enabled
    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        return;
    }

    sync_state = STATE_WAIT;

    sock = sock_s_create( SOCK_DGRAM );

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

bool time_b_is_sync( void ){

    return is_sync;
}

uint32_t time_u32_get_network_time( void ){

    int32_t elapsed = tmr_u32_elapsed_time_ms( local_time );

    // now adjust for drift
    int32_t drift = ( filtered_drift * elapsed ) / 65536;

    uint32_t adjusted_net_time = net_time + ( elapsed - drift );

    return adjusted_net_time;
}

void time_v_set_gps_sync( bool sync ){

    gps_sync = sync;
}


ntp_ts_t time_t_from_system_time( uint32_t end_time ){

    if( !ntp_valid ){

        return ntp_ts_from_u64( 0 );    
    }

    uint32_t elapsed_ms = tmr_u32_elapsed_times( base_system_time, end_time );

    // ASSERT( elapsed_ms < 4000000000 );
    if( elapsed_ms > 4000000000 ){

        log_v_debug_P( PSTR("elapsed time out of range: %lu"), elapsed_ms ); 
    }

    uint64_t now = ntp_u64_conv_to_u64( ntp_master_time );

    // log_v_debug_P( PSTR("base: %lu now: %lu elapsed: %lu"), base_system_time, end_time, elapsed_ms );

    now += ( ( (uint64_t)elapsed_ms << 32 ) / 1000 ); 
    
    return ntp_ts_from_u64( now ); 
}

static void time_v_set_master_clock_internal( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source ){

    is_sync = TRUE;

    master_source = source;

    // if source is usable to sync ntp, set ntp valid.
    if( source > TIME_SOURCE_INTERNAL ){

        if( !ntp_valid ){

            log_v_debug_P( PSTR("NTP valid") );
        }

        ntp_valid = TRUE;
    }

    ntp_ts_t local_ts = time_t_from_system_time( local_system_time );

    // get deltas
    int64_t delta_seconds = (int64_t)local_ts.seconds - (int64_t)source_ts.seconds;
    int16_t delta_ms = (int16_t)ntp_u16_get_fraction_as_ms( local_ts ) - (int16_t)ntp_u16_get_fraction_as_ms( source_ts );

    // char s[ISO8601_STRING_MIN_LEN_MS];
    // ntp_v_to_iso8601( s, sizeof(s), local_ts );
    // log_v_debug_P( PSTR("Local:    %s"), s );
    // ntp_v_to_iso8601( s, sizeof(s), source_ts );
    // log_v_debug_P( PSTR("Remote:   %s"), s );

    if( abs64( delta_seconds ) > 60 ){

        log_v_debug_P( PSTR("HARD SYNC") );
        
        // hard sync
        ntp_master_time     = source_ts;
        base_system_time    = local_system_time;
        last_clock_update   = local_system_time;
        ntp_sync_difference = 0;

        return;
    }

    // gradual adjustment

    // set difference
    ntp_sync_difference = ( delta_seconds * 1000 ) + delta_ms;

    // log_v_debug_P( PSTR("ntp_sync_difference: %ld"), ntp_sync_difference );   
}

void time_v_set_master_clock( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source ){

    if( sync_state == STATE_SLAVE ){

        // our source isn't as good as the master, don't do anything.
        if( source <= master_source ){

            return;
        }

        // our source is better, we are master now
        sync_state = STATE_MASTER;
        master_ip = ip_a_addr(0,0,0,0);
        log_v_debug_P( PSTR("we are master (local source master update) %d"), source );
    }

    time_v_set_master_clock_internal( source_ts, local_system_time, source );
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

// return TRUE if 1 is better than 2
static bool is_master_better( uint64_t uptime1, uint8_t source1, uint64_t uptime2, uint8_t source2 ){

    // check if sources are the same
    if( source1 == source2 ){

        // priority by uptime
        if( uptime1 >= uptime2 ){

            return TRUE;
        }
    }
    else if( source1 > source2 ){

        return TRUE;
    }

    return FALSE;
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

PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // set timeout (in seconds)
    sock_v_set_timeout( sock, 32 );
    
    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check if data received
        if( sock_i16_get_bytes_read( sock ) > 0 ){

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

            if( *type == TIME_MSG_MASTER ){

                time_msg_master_t *msg = (time_msg_master_t *)magic;

                if( sync_state == STATE_WAIT ){

                    // select master
                    master_ip = raddr.ipaddr;
                    master_uptime = msg->uptime;
                    master_source = msg->source;

                    filtered_drift = 0;
                    drift_init = 0;
                    filtered_rtt = 0;

                    sync_state = STATE_SLAVE;

                    log_v_debug_P( PSTR("assigning master: %d.%d.%d.%d"), 
                        master_ip.ip3, 
                        master_ip.ip2, 
                        master_ip.ip1, 
                        master_ip.ip0 );                    
                }
                else if( sync_state == STATE_MASTER ){

                    log_v_debug_P( PSTR("rx sync while master") );

                    // update our master uptime
                    master_uptime = tmr_u64_get_system_time_us();

                    // check if this master is better
                    if( is_master_better( msg->uptime, msg->source, master_uptime, master_source ) ){

                        // select master
                        master_ip = raddr.ipaddr;
                        master_uptime = msg->uptime;
                        master_source = msg->source;

                        filtered_drift = 0;
                        drift_init = 0;
                        filtered_rtt = 0;
                    
                        sync_state = STATE_SLAVE;

                        log_v_debug_P( PSTR("assigning new master: %d.%d.%d.%d"), 
                            master_ip.ip3, 
                            master_ip.ip2, 
                            master_ip.ip1, 
                            master_ip.ip0 );                    

                        // stop sntp
                        sntp_v_stop();
                    }
                } 
            }
            else if( *type == TIME_MSG_NOT_MASTER ){

                // check if that was master.
                if( ip_b_addr_compare( raddr.ipaddr, master_ip ) ){

                    // ruh roh.  master rebooted or something.
                    sync_state = STATE_WAIT;

                    log_v_debug_P( PSTR("lost master, resetting state") );

                    // stop sntp
                    sntp_v_stop();
                }
            }
            else if( *type == TIME_MSG_REQUEST_SYNC ){

                if( sync_state != STATE_MASTER ){

                    continue;
                }

                net_time = tmr_u32_get_system_time_ms();
                local_time = net_time;

                time_msg_sync_t msg;
                msg.magic           = TIME_PROTOCOL_MAGIC;
                msg.version         = TIME_PROTOCOL_VERSION;
                msg.type            = TIME_MSG_SYNC;
                msg.net_time        = net_time;
                msg.uptime          = master_uptime;
                msg.flags           = 0;
                msg.ntp_time        = time_t_now();
                msg.source          = master_source;

                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
            else if( *type == TIME_MSG_SYNC ){

                if( sync_state == STATE_MASTER ){

                    continue;
                }

                time_msg_sync_t *msg = (time_msg_sync_t *)magic;

                master_uptime = msg->uptime;

                uint32_t now = tmr_u32_get_system_time_ms();
                uint32_t est_net_time = time_u32_get_network_time();
                
                int32_t elapsed_rtt = tmr_u32_elapsed_times( rtt_start, now );
                if( elapsed_rtt > 500 ){

                    // a 0.5 second RTT is clearly ridiculous.
                    log_v_debug_P( PSTR("bad: RTT: %u"), elapsed_rtt );

                    continue;
                }
                // check quality of RTT sample
                else if( ( filtered_rtt > 0 ) && 
                         ( elapsed_rtt > ( filtered_rtt + RTT_QUALITY_LIMIT ) ) ){

                    // log_v_debug_P( PSTR("poor quality: RTT: %u"), elapsed_rtt );


                    // although we are not using this sync message for our clock,
                    // we will bump the filtered RTT up a bit in case the overall RTT is
                    // drifting upwards.  this prevents us from losing sync on a busy network.
                    // also - only do this if RTT has been initiatized.
                    if( filtered_rtt > 0 ){
                        
                        filtered_rtt += RTT_RELAX;
                    }

                    continue;
                }

                // check sync
                if( msg->source != TIME_SOURCE_NONE ){
                    
                    // does not compensate for transmission time, so there will be a fraction of a
                    // second offset in NTP time.
                    // this is probably OK, we don't usually need better than second precision
                    // on the NTP clock for most use cases.

                    time_v_set_master_clock_internal( msg->ntp_time, now, msg->source );
                }

                int32_t elapsed_local = tmr_u32_elapsed_times( local_time, now );  
                local_time = now;

                // adjust network timestamp from server with RTT measurement
                uint32_t adjusted_net_time = msg->net_time + ( elapsed_rtt / 2 );

                int32_t elapsed_remote_net = tmr_u32_elapsed_times( last_net_time, adjusted_net_time );
                last_net_time = adjusted_net_time;

                // compute drift
                int32_t clock_diff = elapsed_local - elapsed_remote_net;
                int16_t drift = ( clock_diff * 65536 ) / elapsed_remote_net;

                if( drift_init < 1 ){

                    drift_init++;
                }
                else if( drift_init == 1 ){

                    drift_init++;
                    filtered_drift = drift;
                    filtered_rtt = elapsed_rtt;
                }
                else{

                    filtered_drift = util_i16_ewma( drift, filtered_drift, DRIFT_FILTER );
                    filtered_rtt = util_u16_ewma( elapsed_rtt, filtered_rtt, RTT_FILTER );

                    if( drift_init < 255 ){

                        drift_init++;
                    }
                }

                // compute offset between actual network time and what our internal estimate was
                int16_t clock_offset = (int64_t)est_net_time - (int64_t)adjusted_net_time;


                // how bad is our offset?
                if( abs16( clock_offset ) > 500 ){
                    // worse than 0.5 seconds, do a hard jump

                    net_time = adjusted_net_time;

                    // reset drift filter
                    drift = 0;
                    drift_init = 0;
                    filtered_drift = 0;

                    filtered_rtt = 0;
                
                    log_v_debug_P( PSTR("hard jump: %ld"), clock_offset );
                }
                else{

                    net_time += elapsed_remote_net;

                    // log_v_debug_P( PSTR("rtt: %lu filt: %u offset %d drift: %d"), 
                    //     elapsed_rtt, filtered_rtt, clock_offset, filtered_drift );
                    log_v_debug_P( PSTR("offset %4d diff: %6ld"), clock_offset, clock_diff );
                }
            }
        }
        // socket timeout
        else{

            if( sync_state != STATE_MASTER ){

                log_v_debug_P( PSTR("timed out, resetting state") );

                master_source = 0;
                sync_state = STATE_WAIT;
            }
        }
    }


PT_END( pt );
}

static void send_master( void ){

    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    net_time = tmr_u32_get_system_time_ms();
    local_time = net_time;

    time_msg_master_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_MASTER;
    msg.net_time        = net_time;
    msg.uptime          = master_uptime;
    msg.flags           = 0;
    msg.source          = master_source;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );   
}

static void send_not_master( void ){

    time_msg_not_master_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_NOT_MASTER;
    
    // set up broadcast address
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = ip_a_addr(255,255,255,255);

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  
}

static void request_sync( void ){

    time_msg_request_sync_t msg;
    msg.magic           = TIME_PROTOCOL_MAGIC;
    msg.version         = TIME_PROTOCOL_VERSION;
    msg.type            = TIME_MSG_REQUEST_SYNC;
    
    sock_addr_t raddr;
    raddr.port = TIME_SERVER_PORT;
    raddr.ipaddr = master_ip;

    sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), &raddr );  

    rtt_start = tmr_u32_get_system_time_ms();
}


PT_THREAD( time_master_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    master_ip = ip_a_addr(0,0,0,0);

    THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

    // random delay, see if other masters show up
    if( sync_state == STATE_WAIT ){

        // send, broadcast a message staying we are not a master.
        // this way, if we *were* the master and got reset, then when we come back up
        // we'll tell everyone else we aren't the master, so someone else will jump in
        // (with the current network time).

        send_not_master();
        TMR_WAIT( pt, 200 );
        send_not_master();
        TMR_WAIT( pt, 200 );
        send_not_master();

        thread_v_set_alarm( thread_u32_get_alarm() + 2000 + ( rnd_u16_get_int() >> 3 ) );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( sync_state == STATE_WAIT ) );
    }

    if( sync_state == STATE_WAIT ){

        // check if we have a clock source
        if( get_best_local_source() != TIME_SOURCE_NONE ){

            // elect ourselves as master
            sync_state = STATE_MASTER;
            master_uptime = 0;
            master_ip = ip_a_addr(0,0,0,0);

            log_v_debug_P( PSTR("we are master") );
        }
        else{

            // no clock source available, but try to start SNTP 
            // and maybe we'll get a sync from that.
            sntp_v_start();
        }

    }

    while( sync_state == STATE_MASTER ){

        // reset drift filter in master mode, because the
        // master does not have drift, by definition.
        filtered_drift = 0;


        // default sync source to internal clock
        master_source = TIME_SOURCE_INTERNAL;

        // check sync sources
        if( gps_sync ){

            sntp_v_stop();
            master_source = TIME_SOURCE_GPS;
        }
        else{

            // start SNTP (ignored if already running)
            sntp_v_start();

            // check if synchronized
            if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

                master_source = TIME_SOURCE_NTP;
            }
        }

        master_uptime = tmr_u64_get_system_time_us();

        send_master();

        thread_v_set_alarm( tmr_u32_get_system_time_ms() +  TIME_MASTER_SYNC_RATE * 1000 );

        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && ( !sys_b_shutdown() ) );

        if( sys_b_shutdown() ){

            send_not_master();
            TMR_WAIT( pt, 200 );
            send_not_master();
            TMR_WAIT( pt, 200 );
            send_not_master();   

            THREAD_EXIT( pt );
        }
    }

    if( sync_state == STATE_SLAVE ){

        TMR_WAIT( pt, rnd_u16_get_int() >> 5 ); // random delay we don't dogpile the time master
        request_sync();
    }

    while( sync_state == STATE_SLAVE ){

        // check if local source is better than or same as NTP.  if so, stop our client.
        if( master_source >= TIME_SOURCE_NTP ){
            
            sntp_v_stop();
        }

        // random delay
        uint16_t delay;

        if( drift_init > 16 ){

            delay = ( TIME_SLAVE_SYNC_RATE_MAX * 1000 ) + ( rnd_u16_get_int() >> 3 );
        }
        else{

            delay = ( TIME_SLAVE_SYNC_RATE_BASE * 1000 ) + ( rnd_u16_get_int() >> 3 );
        }

        TMR_WAIT( pt, delay );

        if( get_best_local_source() > master_source ){

            // elect ourselves as master
            sync_state = STATE_MASTER;
            master_uptime = 0;

            log_v_debug_P( PSTR("we are master (local source)") );
            master_ip = ip_a_addr(0,0,0,0);
        } 


        // check state
        if( sync_state != STATE_SLAVE ){

            // no longer master
            log_v_debug_P( PSTR("no longer slave") );

            break;
        }

        // check if local source is worse than NTP.  if so, try to start our NTP client
        if( master_source < TIME_SOURCE_NTP ){

            sntp_v_start();
        }
        
        request_sync();
    }

    // restart if we get here
    THREAD_RESTART( pt );

PT_END( pt );
}


PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( TRUE ){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        uint32_t now = tmr_u32_get_system_time_ms();

        uint32_t elapsed = tmr_u32_elapsed_times( last_clock_update, now );

        uint64_t master = ntp_u64_conv_to_u64( ntp_master_time );

        int16_t clock_adjust = 0;

        if( ntp_sync_difference > 50 ){

            // our clock is ahead, so we need to slow down
            clock_adjust = 10;
        }
        else if( ntp_sync_difference > 2 ){

            // our clock is ahead, so we need to slow down
            clock_adjust = 1;
        }
        else if( ntp_sync_difference < -50 ){

            // our clock is behind, so we need to speed up
            clock_adjust = -10;
        }
        else if( ntp_sync_difference < -2 ){

            // our clock is behind, so we need to speed up
            clock_adjust = -1;
        }

        ntp_sync_difference -= clock_adjust;

        master += ( ( (uint64_t)( elapsed - clock_adjust ) << 32 ) / 1000 );

        ntp_master_time = ntp_ts_from_u64( master );

        base_system_time += elapsed;  
        last_clock_update = now;
    }

PT_END( pt );
}



#endif

