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



/*

Time Sync Version 8:

Separate time synchonization between the millisecond accurate "network time"
and a less accurate, fraction of a second NTP time.

The two use cases are pretty different:

the millisecond clock is mostly used to synchronize the frame clocks on 
VMs across the network.  it doesn't care about the absolute time of day or date,
it just needs to do relative timing with millisecond precision.

The NTP clock is generally used for either logging or for scheduled time of day
and calendar date based events.  Achieving a high accuracy sync with a remote
NTP server across a wireless network and public Internet is difficult to achieve
even with high end equipment, thus, nobody using the NTP subsystem is planning
on millisecond precision anyway - anything within a few seconds is usable, within
1 - 2 seconds quite acceptable, and a fraction of a second phenomenal,

Since both systems have different input requirements:

ms net time: 
    - a high precision internal clock source
    - local wifi connection
    - no internet needed
    - can tolerate high bandwidth usage
    - relatively high bandwidth tolerance can eliminate need for drift compensation
    - no need for GPS at all (no PPS!) -  we don't care about NTP time and we have no need of sync
        signal accurate to atomic clocks, we only need a millisecond clock that every device shares.
    - prefer to avoid hard syncs after initial sync to minimize VM frame skipping

ntp time:
    - internet access
    - ntp server access (DNS)
    - "low quality" sync (1 second level) is trivial
    - remote NTP sync system must be low bandwidth to minimize load on external NTP servers
    - local NTP sync can use much higher contact rates
    - can operate with no drift compensation
    - "hard syncs" - just jolting the clock forward or backward,
        is fair game.
    - soft GPS sync - timestamp on message receive, no PPS interrupt



Additionally, both systems can be internally separated almost entirely into different
OS modules.  The existing high level time sync API could be maintained to 
minimize impact to downstream software (which themselves are complex modules).




*/




/*

Millisecond-precision network time service


32-bit millisecond clock

Leaders drive network clock from their internal oscillators (so there is no concept of clock source quality)

Followers periodically sync while tracking round trip time.



*/







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
PT_THREAD( time_clock_thread( pt_t *pt, void *state ) );

static socket_t sock;

static int16_t sync_delta;
static uint32_t base_sys_time;
static uint32_t master_net_time;
static bool is_sync;

static ip_addr4_t master_ip;



static int8_t net_time_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

        uint32_t timestamp = time_u32_get_network_time();

        memcpy( data, &timestamp, len );

        return 0;
    }

    return -1;
}


KV_SECTION_META kv_meta_t time_info_kv[] = {   
    { CATBUS_TYPE_BOOL,       0, 0,  0,                            cfg_i8_kv_handler,      "enable_time_sync" },

    { CATBUS_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, 0,            net_time_kv_handler,    "net_time" },
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_READ_ONLY, &is_sync,     0,                      "net_time_is_sync" },
    { CATBUS_TYPE_IPv4,       0, KV_FLAGS_READ_ONLY, &master_ip,   0,                      "net_time_master_ip" },

    { CATBUS_TYPE_INT16,      0, KV_FLAGS_READ_ONLY, &sync_delta,  0,                      "net_master_sync_diff" },
};


static bool is_time_sync_enabled( void ){

    return cfg_b_get_boolean( __KV__enable_time_sync );
}

void time_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }
    
    // check if time sync is enabled
    if( !is_time_sync_enabled() ){

        return;
    }

    sock = sock_s_create( SOS_SOCK_DGRAM );

    // sock_v_bind( sock, TIME_SERVER_PORT );


    thread_t_create( time_server_thread,
                    PSTR("time_server"),
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
    
    if( !time_b_is_sync() ){
        
        return 0;        
    }

    uint32_t elapsed_ms = tmr_u32_elapsed_time_ms( base_sys_time );

    return master_net_time + elapsed_ms;
}

uint32_t time_u32_get_network_time_from_local( uint32_t local_time ){
    
    uint32_t elapsed_ms = tmr_u32_elapsed_times( base_sys_time, local_time );

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

static bool is_leader( void ){

    return services_b_is_server( TIME_ELECTION_SERVICE, 0 );
}

static bool is_service_avilable( void ){

    return services_b_is_available( TIME_ELECTION_SERVICE, 0 );
}

static bool is_follower( void ){

    return !is_leader() && is_service_avilable();
}

static uint16_t get_priority( void ){

    // TODO
    // if doze mode works out, check for it here
    // and lower priority?

    #ifdef ESP32
    if( batt_b_enabled() ){

        return 0;
    }

    return 10;
    #endif

    return 1;
}

static uint8_t *decode_msg( uint8_t *msg ){

    uint32_t *magic = (uint32_t *)msg;

    if( *magic != TIME_PROTOCOL_MAGIC ){

        return 0;
    }

    uint8_t *version = (uint8_t *)(magic + 1);

    if( *version != TIME_PROTOCOL_VERSION ){

        return 0;
    }

    uint8_t *type = version + 1;    

    return type;
}

PT_THREAD( time_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint16_t backoff;
    backoff = TIME_SYNC_RATE_BASE;

    // wait for network
    THREAD_WAIT_WHILE( pt, !wifi_b_connected() );
    
    services_v_join_team( TIME_ELECTION_SERVICE, 0, get_priority(), sock_u16_get_lport( sock ) );

    // wait until we resolve the election
    THREAD_WAIT_WHILE( pt, !is_service_avilable() );


    while( is_leader() ){

        if( !is_sync ){

            sync_delta = 0;
            master_net_time = tmr_u32_get_system_time_ms();
            base_sys_time = master_net_time;            

            is_sync = TRUE;
        }

        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_leader() );

        if( !is_leader() ){

            continue;
        }

        // check if data received
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        uint32_t now = tmr_u32_get_system_time_ms();

        uint8_t *data = sock_vp_get_data( sock );
        uint8_t *type = decode_msg( data );

        if( type == 0 ){

            continue;
        }

        sock_addr_t raddr;
        sock_v_get_raddr( sock, &raddr );

        if( *type == TIME_MSG_REQUEST_SYNC ){

            time_msg_request_sync_t *req = (time_msg_request_sync_t *)data;

            time_msg_sync_t sync = {
                TIME_PROTOCOL_MAGIC,
                TIME_PROTOCOL_VERSION,
                TIME_MSG_SYNC,
                req->transmit_time,
                time_u32_get_network_time_from_local( now )
            };

            sock_i16_sendto( sock, (uint8_t *)&sync, sizeof(sync), 0 );  
        }
        else if( *type == TIME_MSG_PING ){

            time_msg_ping_response_t reply = {
                TIME_PROTOCOL_MAGIC,
                TIME_PROTOCOL_VERSION,
                TIME_MSG_PING_RESPONSE,
            };
    
            sock_i16_sendto( sock, (uint8_t *)&reply, sizeof(reply), 0 );  
        }
    }

    while( is_follower() ){

        sock_v_flush( sock );

        // random delay to prevent overloading the server
        TMR_WAIT( pt, 1000 + ( rnd_u16_get_int() >> 4 ) ); // 1 to 5 seconds        

        // send ping to warm up ARP
        time_msg_ping_t ping = {
            TIME_PROTOCOL_MAGIC,
            TIME_PROTOCOL_VERSION,
            TIME_MSG_PING,
        };

        sock_addr_t send_raddr = services_a_get( TIME_ELECTION_SERVICE, 0 );

        sock_v_flush( sock );
        
        sock_i16_sendto( sock, (uint8_t *)&ping, sizeof(ping), &send_raddr );  

        sock_v_set_timeout( sock, 2 );

        // wait for reply or timeout
        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_follower() );

        // check if service changed
        if( !is_follower() ){

            THREAD_RESTART( pt );
        }

        // check for timeout
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            TMR_WAIT( pt, 10000 );

            continue;
        }   

        uint8_t *type = decode_msg( sock_vp_get_data( sock ) );

        if( type == 0 ){

            continue;
        }

        if( *type != TIME_MSG_PING_RESPONSE ){

            continue;
        }

        // send sync request
        time_msg_request_sync_t req = {
            TIME_PROTOCOL_MAGIC,
            TIME_PROTOCOL_VERSION,
            TIME_MSG_REQUEST_SYNC,
            tmr_u32_get_system_time_ms()   
        };

        sock_addr_t send_raddr2 = services_a_get( TIME_ELECTION_SERVICE, 0 );
        
        sock_i16_sendto( sock, (uint8_t *)&req, sizeof(req), &send_raddr2 );  

        // wait for reply or timeout
        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( sock ) < 0 ) && is_follower() );

        uint32_t now = tmr_u32_get_system_time_ms();

        // check if service changed
        if( !is_follower() ){

            THREAD_RESTART( pt );
        }

        // check for timeout
        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            TMR_WAIT( pt, 10000 );

            continue;
        }   

        uint8_t *data = sock_vp_get_data( sock );
        uint8_t *type2 = decode_msg( data );

        if( type2 == 0 ){

            continue;
        }

        if( *type2 != TIME_MSG_SYNC ){

            continue;
        }        

        time_msg_sync_t *sync = ( time_msg_sync_t * )data;

        // compute elasped time
        uint32_t elapsed_ms = tmr_u32_elapsed_times( sync->origin_time, now );

        // check for obviously bad RTTs
        if( elapsed_ms > TIME_RTT_THRESHOLD ){

            log_v_debug_P( PSTR("bad RTT: %u origin: %u now: %u"), elapsed_ms, sync->origin_time, now );

            TMR_WAIT( pt, 10000 );

            continue;
        }

        // assuming link is symmetrical, compute offset
        uint32_t clock_offset = elapsed_ms / 2;

        // adjust source timestamp for offset
        sync->net_time += clock_offset;
        
        if( is_sync ){

            uint32_t net_time = time_u32_get_network_time_from_local( now );

            // compute sync delta
            sync_delta = (int64_t)net_time - (int64_t)sync->net_time;

            // log_v_info_P( PSTR("sync delta: %d"), sync_delta );
        }

        // if not synced or sync is too far off, we can immediately jolt the clock into position
        if( !is_sync || ( abs16( sync_delta ) > 200 ) ){

            master_net_time = sync->net_time;
            base_sys_time = now;

            is_sync = TRUE;

            log_v_info_P( PSTR("Net time hard sync delta: %d"), sync_delta );

            sync_delta = 0;

            // reset backoff
            backoff = TIME_SYNC_RATE_BASE;
        }


        // change to backoff after we verify everything works
        TMR_WAIT( pt, (uint32_t)backoff * 1000 );

        // increment backoff
        if( backoff < TIME_SYNC_RATE_MAX ){

            backoff *= 2;
        }
    }

    THREAD_RESTART( pt );


PT_END( pt );
}


PT_THREAD( time_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // wait for sync
    THREAD_WAIT_WHILE( pt, !is_sync);

    thread_v_set_alarm( tmr_u32_get_system_time_ms() );

    while( 1 ){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        master_ip = services_a_get_ip( TIME_ELECTION_SERVICE, 0 );

        // get elapsed time
        uint32_t elapsed_ms = tmr_u32_elapsed_time_ms( base_sys_time );

        // update base time
        base_sys_time += elapsed_ms;            
        

        // check sync delta:
        // positive deltas mean our clock is ahead
        // negative deltas mean out clock is behind

        int16_t clock_adjust = 0;

        if( sync_delta > 0 ){

            // local clock is ahead
            // we need to slow down a bit

            if( sync_delta > 500 ){

                clock_adjust = 10;
            }
            else if( sync_delta > 200 ){

                clock_adjust = 5;
            }
            else if( sync_delta > 50 ){

                clock_adjust = 2;
            }
            else{

                clock_adjust = 1;
            }
        }
        else if( sync_delta < 0 ){

            // local clock is behind
            // we need to speed up a bit

            if( sync_delta < -500 ){

                clock_adjust = -10;
            }
            else if( sync_delta < -200 ){

                clock_adjust = -5;
            }
            else if( sync_delta < -50 ){

                clock_adjust = -2;
            }
            else{

                clock_adjust = -1;
            }
        }

        sync_delta -= clock_adjust;

        // adjust elapsed ms by clock adjustment:
        elapsed_ms -= clock_adjust;

        // sanity check elapsed time
        // if we went negative it will rollover
        // this shouldn't happen unless thread timing is very screwed up
        if( elapsed_ms > 2000 ){

            log_v_debug_P( PSTR("Net time sync fail, elapsed: %u"), elapsed_ms );

            continue;
        }  

        // update net time
        master_net_time += elapsed_ms;
    }

PT_END( pt );
}



#endif

