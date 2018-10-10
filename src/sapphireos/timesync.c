// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

// master clock
static ntp_ts_t master_time;
static uint32_t base_system_time;
static int32_t sync_difference;
static int16_t clock_adjust;

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


KV_SECTION_META kv_meta_t time_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,       0, 0,  0,                                 cfg_i8_kv_handler,      "enable_time_sync" },
    { SAPPHIRE_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &net_time,         0,                      "net_time" },
    { SAPPHIRE_TYPE_IPv4,       0, KV_FLAGS_READ_ONLY, &master_ip,        0,                      "net_time_master_ip" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_READ_ONLY, &master_source,    0,                      "net_time_master_source" },
    { SAPPHIRE_TYPE_UINT64,     0, KV_FLAGS_READ_ONLY, &master_uptime,    0,                      "net_time_master_uptime" },
};


void hal_rtc_v_irq( void ){


}

void time_v_init( void ){

    // January 1, 2018, midnight
    master_time.seconds = 1514786400 + 2208988800 - 21600;

    // check if time sync is enabled
    if( !cfg_b_get_boolean( __KV__enable_time_sync ) ){

        return;
    }

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

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

    return master_source != 0;
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

void time_v_set_master_clock( ntp_ts_t t, uint32_t base_time, uint8_t source ){

    master_source = source;

    // log_v_debug_P( PSTR("set master clock source: %u"), source );

    // base time is the millisecond system timer that corresponds
    // with the given ntp timestamp


    // get master time at base time
    // ntp_ts_t master = ntp_ts_from_u64( 
    //                     ntp_u64_conv_to_u64( master_time ) + 
    //                     ntp_u64_conv_to_u64( ntp_ts_from_ms( tmr_u32_elapsed_time_ms( base_time ) ) ) );
    
    uint32_t elapsed_ms = tmr_u32_elapsed_times( base_system_time, base_time );

    ntp_ts_t master = master_time;
    uint64_t fraction = master.fraction + ( ( (uint64_t)elapsed_ms << 32 ) / 1000 );

    master.seconds += ( fraction >> 32 );
    master.fraction += ( fraction & 0xffffffff );


    // get deltas
    int64_t delta_seconds = (int64_t)master.seconds - (int64_t)t.seconds;
    int16_t delta_ms = (int16_t)ntp_u16_get_fraction_as_ms( master ) - (int16_t)ntp_u16_get_fraction_as_ms( t );

    char s[ISO8601_STRING_MIN_LEN_MS];
    ntp_v_to_iso8601( s, sizeof(s), master_time );
    log_v_debug_P( PSTR("Master:   %s"), s );
    ntp_v_to_iso8601( s, sizeof(s), t );
    log_v_debug_P( PSTR("Remote:   %s"), s );

    if( abs64( delta_seconds ) > 60 ){

        log_v_debug_P( PSTR("HARD SYNC") );

        // hard sync
        master_time = t;
        base_system_time = base_time;

        return;
    }

    log_v_debug_P( PSTR("delta seconds: %ld ms: %d"), (int32_t)delta_seconds, delta_ms );

    // gradual adjustment

    // set difference
    sync_difference = ( delta_seconds * 1000 ) + delta_ms;

    // if( sync_difference > 0){

    //     // our clock is ahead, so we need to slow down
    //     clock_adjust = 10;
    // }
    // else if( sync_difference < 0){

    //     // our clock is behind, so we need to speed up
    //     clock_adjust = -10;
    // }
    // else{

    //     clock_adjust = 0;   
    // }

    log_v_debug_P( PSTR("sync_difference: %ld adjust: %d"), sync_difference, clock_adjust );
}

ntp_ts_t time_t_now( void ){

    // uint16_t rtc_time = hal_rtc_u16_get_time();
    ntp_ts_t now = master_time;

    // get time elapsed since base time was set
    uint32_t elapsed_ms = tmr_u32_elapsed_time_ms( base_system_time );

    // now.seconds += ( elapsed_ms / 1000 );
    // elapsed_ms %= 1000;

    uint64_t fraction = now.fraction + ( ( (uint64_t)elapsed_ms << 32 ) / 1000 );

    now.seconds += ( fraction >> 32 );
    now.fraction += ( fraction & 0xffffffff );

    // now.fraction += ( ( (uint64_t)elapsed_ms << 32 ) / 1000 );

    // ntp_ts_t elapsed = ntp_ts_from_ms( elapsed_ms );

    // now = ntp_ts_from_u64( ntp_u64_conv_to_u64( now ) + ntp_u64_conv_to_u64( elapsed ) );
    
    return now;
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

        return TIME_FLAGS_SOURCE_GPS;
    }

    if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

        return TIME_FLAGS_SOURCE_NTP;
    }

    return 0;
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
                    }
                } 
            }
            else if( *type == TIME_MSG_NOT_MASTER ){

                // check if that was master.
                if( ip_b_addr_compare( raddr.ipaddr, master_ip ) ){

                    // ruh roh.  master rebooted or something.
                    sync_state = STATE_WAIT;

                    log_v_debug_P( PSTR("lost master, resetting state") );
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
                // msg.ntp_time        = sntp_t_now();
                msg.source          = master_source;

                sock_i16_sendto( sock, (uint8_t *)&msg, sizeof(msg), 0 );
            }
            else if( *type == TIME_MSG_SYNC ){

                if( sync_state == STATE_MASTER ){

                    continue;
                }

                time_msg_sync_t *msg = (time_msg_sync_t *)magic;

                master_uptime = msg->uptime;

                // check sync
                if( msg->source > 0 ){
                    
                    // does not compensate for transmission time, so there will be a fraction of a
                    // second offset in NTP time.
                    // this is probably OK, we don't usually need better than second precision
                    // on the NTP clock for most use cases.

                    time_v_set_master_clock( msg->ntp_time, tmr_u32_get_system_time_ms(), msg->source );
                }
                
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

                    log_v_debug_P( PSTR("poor quality: RTT: %u"), elapsed_rtt );


                    // although we are not using this sync message for our clock,
                    // we will bump the filtered RTT up a bit in case the overall RTT is
                    // drifting upwards.  this prevents us from losing sync on a busy network.
                    // also - only do this if RTT has been initiatized.
                    if( filtered_rtt > 0 ){
                        
                        filtered_rtt += RTT_RELAX;
                    }

                    continue;
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
                
                    // log_v_debug_P( PSTR("hard jump: %ld"), clock_offset );
                }
                else{

                    net_time += elapsed_remote_net;
                }

                // log_v_debug_P( PSTR("rtt: %lu filt: %u offset %d drift: %d"), 
                    // elapsed_rtt, filtered_rtt, clock_offset, filtered_drift );
            }
        }
        // socket timeout
        else{

            if( sync_state != STATE_MASTER ){

                log_v_debug_P( PSTR("timed out, resetting state") );

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

        // TMR_WAIT( pt, 2000 + ( rnd_u16_get_int() >> 3 ) );
        TMR_WAIT( pt, rnd_u16_get_int() >> 5 );
    }

    if( sync_state == STATE_WAIT ){

        // check if we have a clock source
        if( get_best_local_source() > 0 ){

            // elect ourselves as master
            sync_state = STATE_MASTER;
            master_uptime = 0;

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

        // check state
        if( sync_state != STATE_MASTER ){

            // no longer master
            log_v_debug_P( PSTR("no longer master") );

            sntp_v_stop();

            break;
        }

        // check sync sources
        if( gps_sync ){

            sntp_v_stop();
            master_source = TIME_FLAGS_SOURCE_GPS;
        }
        else{

            // start SNTP (ignored if already running)
            sntp_v_start();

            // check if synchronized
            if( sntp_u8_get_status() == SNTP_STATUS_SYNCHRONIZED ){

                master_source = TIME_FLAGS_SOURCE_NTP;
            }
        }

        master_uptime = tmr_u64_get_system_time_us();

        send_master();

        TMR_WAIT( pt, TIME_MASTER_SYNC_RATE * 1000 );
    }

    while( sync_state == STATE_SLAVE ){

        sntp_v_stop();

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
        } 


        // check state
        if( sync_state != STATE_SLAVE ){

            // no longer master
            log_v_debug_P( PSTR("no longer slave") );

            break;
        }
        
        rtt_start = tmr_u32_get_system_time_ms();

        request_sync();
    }

    // restart if we get here
    THREAD_RESTART( pt );

PT_END( pt );
}


PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint16_t clock_rate;

    while( TRUE ){

        sync_difference -= clock_adjust;

        clock_rate = 1000 + clock_adjust;

        thread_v_set_alarm( thread_u32_get_alarm() + clock_rate );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // increment master clock by 1 second
        master_time.seconds++;
        base_system_time += clock_rate; // base system time needs to track actual elapsed time

        if( sync_difference < 0 ){
            
            sync_difference += 1000;
        }
        else if( sync_difference > 0 ){
            
            sync_difference -= 1000;
        }
    }

PT_END( pt );
}



#endif

