// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

 SNTP Client

 Implements an RFC 4330 compliant SNTP client.

 Only unicast mode is supported.


 Notes:
 As written, the compiled implementation of this code is huge, around 10,000 bytes.
 This is because of all of the math with 64 bit variables, and with far too many
 to fit in the register set, so a lot of time is spent moving variables to/from
 memory.


*/

#include "system.h"


#include "datetime.h"
#include "dns.h"
#include "config.h"
#include "fs.h"
#include "timers.h"
#include "threading.h"
#include "memory.h"
#include "sockets.h"
#include "keyvalue.h"

#include "sntp.h"
#include "timesync.h"

// #define NO_LOGGING
#include "logging.h"

#ifdef ENABLE_TIME_SYNC

// offset and delay in ms from last sync
static int16_t last_offset;
static uint16_t last_delay;

static sntp_status_t8 status = SNTP_STATUS_DISABLED;

static socket_t sock = -1;
static thread_t thread = -1;


KV_SECTION_META kv_meta_t sntp_cfg_kv[] = {
    { CATBUS_TYPE_STRING64,     0, 0,  0,                  cfg_i8_kv_handler,  "sntp_server" },
    { CATBUS_TYPE_INT8,         0, KV_FLAGS_READ_ONLY,  &status, 0,            "sntp_status" },
};



/*

Notes:

Network Time: This is the current time synchronized to the NTP server.
System Time: This is the current value of the 32 bit millisecond system timer.


Setting the clock:

We set network time from the calculation result of the SNTP request.
Base system time is the millisecond system timer value corresponding
to when we received the reply packet.  This allows us to correlate network
time with system time.

We drive network time at a 1 second rate from the system clock.
An API call to get network time with greater than 1 second of precision
will read the current system time, subtract the base system time, and
add those milliseconds to the network time.

Adjusting the system clock frequency:

Once we have a base system time set (after the initial network time sync),
we can calculate our local clock drift next time we sync to the server.
We calculate what the server says our local time should be and compare
that to the 1 millisecond precision local network time and the difference
tells us how many milliseconds we're off.  We can then calculate how much
to adjust the system clock frequency based on how much offset we need to
correct within the next polling interval.

Example:
Polling at 60 seconds
Initial offset is 0
Drifting at +0.1%

Assume our initial time is 0.

At our next sync, we have 60 seconds.  However, since we drifted +0.1%,
we're running too fast and we have 60.06 seconds.  We get 60 back from
the server, so we know we drift +0.06 seconds per polling interval.

Note we don't need to try to correct the offset itself, since we can
calculate our offset from the server.  We just want to make sure the offset
changes as little as possible between updates.

We increase our clock rate by 0.1%.
Next sync, we check our last offset and the new offset.  Say our new offset
is 0.07 seconds, we have difference of 0.01.  Still a little fast, so we
correct by (0.01 / 60) = 0.016%.

Note our accuracy is affected by our thread scheduling and network queuing delays.

It might be interesting to track statistics on the delays and offsets we get from the server.

*/


PT_THREAD( sntp_client_thread( pt_t *pt, void *state ) );


void sntp_v_init( void ){

}

void sntp_v_start( void ){

    if( status != SNTP_STATUS_DISABLED ){

        return;
    }

    ASSERT( thread <= 0 );
    
    log_v_debug_P( PSTR("starting SNTP client") );

    status = SNTP_STATUS_NO_SYNC;

    thread = thread_t_create( sntp_client_thread,
                              PSTR("sntp_client"),
                              0,
                              0 );
}

void sntp_v_stop( void ){

    if( sock > 0 ){

        sock_v_release( sock );
        sock = -1;
    }

    if( thread > 0 ){

        log_v_debug_P( PSTR("stopping SNTP client") );

        thread_v_kill( thread );
        thread = -1;
    }

    status = SNTP_STATUS_DISABLED;
}

sntp_status_t8 sntp_u8_get_status( void ){

    return status;
}

int16_t sntp_i16_get_offset( void ){

    return last_offset;
}

uint16_t sntp_u16_get_delay( void ){

    return last_delay;
}

/*
 Timestamp Conversions


From the RFC:

 Timestamp Name          ID   When Generated
      ------------------------------------------------------------
      Originate Timestamp     T1   time request sent by client
      Receive Timestamp       T2   time request received by server
      Transmit Timestamp      T3   time reply sent by server
      Destination Timestamp   T4   time reply received by client

   The roundtrip delay d and system clock offset t are defined as:

      d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.

*/


void process_packet( 
    ntp_packet_t *packet, 
    ntp_ts_t *network_time, 
    uint32_t *base_system_time ){

    // get destination timestamp (our local network time when we receive the packet)
    *base_system_time = tmr_u32_get_system_time_ms();
    ntp_ts_t dest_ts = time_t_from_system_time( *base_system_time );

    uint64_t dest_timestamp = ntp_u64_conv_to_u64( dest_ts );

    // get timestamps from packet, noting the conversion from big endian to little endian
    packet->originate_timestamp.seconds  = HTONL( packet->originate_timestamp.seconds );
    packet->originate_timestamp.fraction = HTONL( packet->originate_timestamp.fraction );

    packet->receive_timestamp.seconds    = HTONL( packet->receive_timestamp.seconds );
    packet->receive_timestamp.fraction   = HTONL( packet->receive_timestamp.fraction );

    packet->transmit_timestamp.seconds   = HTONL( packet->transmit_timestamp.seconds );
    packet->transmit_timestamp.fraction  = HTONL( packet->transmit_timestamp.fraction );

    // char time_str[ISO8601_STRING_MIN_LEN_MS];
    // ntp_v_to_iso8601( time_str, sizeof(time_str), packet->originate_timestamp );
    // log_v_info_P( PSTR("Originate %s"), time_str );
    // ntp_v_to_iso8601( time_str, sizeof(time_str), packet->receive_timestamp );
    // log_v_info_P( PSTR("Receive   %s"), time_str );
    // ntp_v_to_iso8601( time_str, sizeof(time_str), packet->transmit_timestamp );
    // log_v_info_P( PSTR("Transmit  %s"), time_str );
    // ntp_v_to_iso8601( time_str, sizeof(time_str), dest_ts );
    // log_v_info_P( PSTR("Dest      %s"), time_str );


    uint64_t originate_timestamp = ntp_u64_conv_to_u64( packet->originate_timestamp );

    uint64_t receive_timestamp   = ntp_u64_conv_to_u64( packet->receive_timestamp );

    uint64_t transmit_timestamp  = ntp_u64_conv_to_u64( packet->transmit_timestamp );

    int64_t delay = ( dest_timestamp - originate_timestamp ) - ( transmit_timestamp - receive_timestamp );

    int64_t offset = ( (int64_t)( receive_timestamp - originate_timestamp ) + (int64_t)( transmit_timestamp - dest_timestamp ) ) / 2;

    // current network time is current time + offset
    uint64_t current_time = dest_timestamp + offset;

    // set offset and delay variables
    if( ( ( offset >> 32 ) < 32 ) && ( ( offset >> 32 ) > -32 ) ){

        last_offset = ( ( offset >> 32 ) * 1000 ) + ( ( ( offset & 0xffffffff ) * 1000 ) >> 32 );
    }
    else{

        last_offset = 0;
    }

    if( ( delay >> 32 ) < 64 ){

        last_delay = ( ( delay >> 32 ) * 1000 ) + ( ( ( delay & 0xffffffff ) * 1000 ) >> 32);
    }
    else{

        last_delay = 0;
    }

    // set sync status
    status = SNTP_STATUS_SYNCHRONIZED;

    *network_time = ntp_ts_from_u64( current_time );
}




PT_THREAD( sntp_client_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    sock = sock_s_create( SOS_SOCK_DGRAM );

    // check socket creation
    if( sock < 0 ){

        // that's too bad, we'll have to skip this cycle and try again later
            
        TMR_WAIT( pt, 8000 );
        THREAD_RESTART( pt );
    }

    TMR_WAIT( pt, 1000 );

	while(1){

        // wait if IP is not configured
        THREAD_WAIT_WHILE( pt, !cfg_b_ip_configured() );

        // check DNS resolver for ip address and set up remote port
        sock_addr_t ntp_server_addr;
        ntp_server_addr.port = SNTP_SERVER_PORT;

        char name[CFG_STR_LEN];
        cfg_i8_get( CFG_PARAM_SNTP_SERVER, name );

        // check if name is empty
        if( strnlen( name, sizeof(name) ) == 0 ){

            strncpy_P( name, PSTR("pool.ntp.org"), sizeof(name) );
            cfg_v_set( CFG_PARAM_SNTP_SERVER, name );
        }

        ntp_server_addr.ipaddr = dns_a_query( name );

        if( ip_b_is_zeroes( ntp_server_addr.ipaddr ) ){

            TMR_WAIT( pt, 1000 );

            // that's too bad, we'll have to skip this cycle and try again later
            continue;
        }
            
        // build sntp packet
        ntp_packet_t pkt;

        // initialize to all 0s
        memset( &pkt, 0, sizeof(pkt) );

        // set version to 4 and mode to client
        pkt.li_vn_mode = SNTP_VERSION_4 | SNTP_MODE_CLIENT | SNTP_LI_ALARM;

        // get our current network time with the maximum available precision
        ntp_ts_t transmit_ts = time_t_now();

        // set transmit timestamp (converting from little endian to big endian)
        pkt.transmit_timestamp.seconds = HTONL(transmit_ts.seconds);
        pkt.transmit_timestamp.fraction = HTONL(transmit_ts.fraction);

        // parse current time to ISO so we can read it in the log file
        // char time_str[ISO8601_STRING_MIN_LEN_MS];
        // ntp_v_to_iso8601( time_str, sizeof(time_str), transmit_ts );
        // log_v_info_P( PSTR("TX Time is now: %s"), time_str );


        // send packet
        // if packet transmission fails, we'll try again on the next polling cycle
        if( sock_i16_sendto( sock, &pkt, sizeof(pkt), &ntp_server_addr ) < 0 ){

            goto retry;
        }

        // log_v_debug_P( PSTR("SNTP sync sent: %d.%d.%d.%d"), 
        //     ntp_server_addr.ipaddr.ip3,
        //     ntp_server_addr.ipaddr.ip2,
        //     ntp_server_addr.ipaddr.ip1,
        //     ntp_server_addr.ipaddr.ip0 );

        // set timeout
        sock_v_set_timeout( sock, SNTP_TIMEOUT );

        // wait for packet
        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // check for timeout (no data received)
        if( sock_i16_get_bytes_read( sock ) < 0 ){

            // log_v_debug_P( PSTR("SNTP sync timed out") );

            continue;
        }

        // log_v_debug_P( PSTR("SNTP sync received") );

        // get data and process it
        // NOTE the original ntp packet local variable we used will
        // be corrupt at this point in the thread, DO NOT USE IT!
        ntp_packet_t *recv_pkt = sock_vp_get_data( sock );

        // process received packet
        ntp_ts_t network_time;
        uint32_t sys_time;
        process_packet( recv_pkt, &network_time, &sys_time );

        // sync master clock
        time_v_set_ntp_master_clock( network_time, sys_time, TIME_SOURCE_NTP );

        // parse current time to ISO so we can read it in the log file
        char time_str2[ISO8601_STRING_MIN_LEN_MS];
        ntp_v_to_iso8601( time_str2, sizeof(time_str2), network_time );
        log_v_info_P( PSTR("NTP Time is now: %s Offset: %d Delay: %d"), time_str2, last_offset, last_delay );
    

        goto clean_up;

retry:
        TMR_WAIT( pt, 8000 );

        continue;

clean_up:
        
        // wait during polling interval
        TMR_WAIT( pt, (uint32_t)SNTP_DEFAULT_POLL_INTERVAL * 1000 );
    }

PT_END( pt );
}

#endif
