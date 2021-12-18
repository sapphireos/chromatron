/* 
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
 */


#include "cpu.h"

#include "system.h"

#include "config.h"
#include "keyvalue.h"

#include "threading.h"
#include "target.h"

#include "hal_timers.h"
#include "timers.h"

#include "util.h"

//#define NO_LOGGING
#include "logging.h"

// #define NO_EVENT_LOGGING
#include "event_log.h"



static int8_t tmr_i8_kv_handler( 
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){
        
        if( hash == __KV__sys_time ){

            uint32_t time = tmr_u32_get_system_time_ms();
            
            memcpy( data, &time, sizeof(time) );
        }
        else if( hash == __KV__sys_time_us ){

            uint64_t time = tmr_u64_get_system_time_us();
            
            memcpy( data, &time, sizeof(time) );
        }
    }

    return 0;
}


KV_SECTION_META kv_meta_t tmr_info_kv[] = {
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  0, tmr_i8_kv_handler,  "sys_time" },
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  0, tmr_i8_kv_handler,  "sys_time_us" },
};

void tmr_v_init( void ){
	
    hal_timer_v_init();
}

// returns the current system time, accurate to the nearest millisecond.
// this uses the current system time and the current timer count
uint32_t tmr_u32_get_system_time_ms( void ){
	
    return tmr_u64_get_system_time_ms();
}

uint64_t tmr_u64_get_system_time_ms( void ){

    return tmr_u64_get_system_time_us() / 1000;
}

// returns the current system time, accurate to the nearest microsecond.
// this uses the current system time and the current timer count
uint32_t tmr_u32_get_system_time_us( void ){
	
    return tmr_u64_get_system_time_us();
}

// returns the current value of the system timer.
uint32_t tmr_u32_get_system_time( void ){
	
	return tmr_u32_get_system_time_ms();
}

// compute elapsed ticks since start_ticks
uint32_t tmr_u32_elapsed_time_ms( uint32_t start_time ){
	
	uint32_t end_time = tmr_u32_get_system_time_ms();
	
    return tmr_u32_elapsed_times( start_time, end_time );
}

uint32_t tmr_u32_elapsed_time_us( uint32_t start_time ){

    uint32_t end_time = tmr_u32_get_system_time_us();

    return tmr_u32_elapsed_times( start_time, end_time );
}


// compute elapsed ticks between start_time and end_time
uint32_t tmr_u32_elapsed_times( uint32_t start_time, uint32_t end_time ){
        
    if( end_time >= start_time ){
        
        return end_time - start_time;
    }
    else{
        
        return UINT32_MAX - ( start_time - end_time );
    }
}


// returns 1 if time > system_time,
// -1 if time < system_time,
// 0 if equal
int8_t tmr_i8_compare_time( uint32_t time ){
	
	int32_t distance = ( int32_t )( time - tmr_u32_get_system_time_ms() );
	
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

// returns 1 if time1 > time2,
// -1 if time1 < time2,
// 0 if equal
int8_t tmr_i8_compare_times( uint32_t time1, uint32_t time2 ){
		
	int32_t distance = ( int32_t )( time1 - time2 );
	
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