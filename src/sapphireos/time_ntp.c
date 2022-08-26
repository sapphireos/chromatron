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


static void time_v_set_ntp_master_clock_internal( 
    ntp_ts_t source_ts, 
    uint32_t source_net_time,
    uint32_t local_system_time,
    uint8_t source,
    uint16_t delay ){    

    master_source = source;

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
        ( !ntp_valid && ( source > TIME_SOURCE_INTERNAL) ) ||
        !is_sync ){

        log_v_debug_P( PSTR("clock synced: prev sync: %d delta_master: %d delta_ntp: %d"), is_sync, (int32_t)delta_master_ms, (int32_t)delta_ntp_seconds );
        
        // hard sync
        master_ntp_time         = source_ts;
        master_net_time         = source_net_time;
        base_system_time        = local_system_time;
        last_clock_update       = local_system_time;
        ntp_sync_difference     = 0;
        master_sync_difference  = 0;

        is_sync = TRUE;

        if( source > TIME_SOURCE_INTERNAL ){
            
            ntp_valid = TRUE;

            char time_str[ISO8601_STRING_MIN_LEN_MS];
            ntp_v_to_iso8601( time_str, sizeof(time_str), time_t_now() );

            log_v_info_P( PSTR("NTP Time is now: %s"), time_str );
        }

        return;
    }

    // gradual adjustment

    // set difference
    ntp_sync_difference = ( delta_ntp_seconds * 1000 ) + delta_ntp_ms;
    master_sync_difference = delta_master_ms;

    // log_v_debug_P( PSTR("ntp_sync_difference: %ld"), ntp_sync_difference );   
    // log_v_debug_P( PSTR("dly: %3u diff: %5ld"), delay, master_sync_difference );   
}


void time_v_get_timestamp( ntp_ts_t *ntp_now, uint32_t *system_time ){

    *system_time = tmr_u32_get_system_time_ms();

    *ntp_now = time_t_from_system_time( *system_time );   
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

            // log_v_debug_P( PSTR("negative elapsed time: %ld"), elapsed_ms ); 
        }
    }

    

    uint64_t now = ntp_u64_conv_to_u64( master_ntp_time );

    // log_v_debug_P( PSTR("base: %lu now: %lu elapsed: %lu"), base_system_time, end_time, elapsed_ms );

    now += ( ( (int64_t)elapsed_ms << 32 ) / 1000 ); 
    
    return ntp_ts_from_u64( now ); 
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


#endif
