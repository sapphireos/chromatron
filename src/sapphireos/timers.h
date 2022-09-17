/*
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
 */



#ifndef _TIMERS_H
#define _TIMERS_H

#include "system.h"
#include "threading.h"
#include "hal_timers.h"


// function prototypes:
void tmr_v_init( void );

bool tmr_b_io_timers_running( void );

uint32_t tmr_u32_get_system_time_ms( void );
uint64_t tmr_u64_get_system_time_ms( void );
uint32_t tmr_u32_get_system_time_us( void );
uint64_t tmr_u64_get_system_time_us( void );
uint32_t tmr_u32_get_system_time( void );

uint32_t tmr_u32_elapsed_time_ms( uint32_t start_time );
uint32_t tmr_u32_elapsed_times( uint32_t start_time, uint32_t end_time );
uint32_t tmr_u32_elapsed_time_us( uint32_t start_time );
int8_t tmr_i8_compare_time( uint32_t time );
int8_t tmr_i8_compare_times( uint32_t time1, uint32_t time2 );

#define TMR_WAIT( pt, time ) \
    thread_v_set_alarm( ((uint32_t)time) + tmr_u32_get_system_time_ms() ); \
    THREAD_SLEEP( pt );


#define BUSY_WAIT(expr) {\
        while(expr){}\
        }
    
#define SAFE_BUSY_WAIT(expr) {\
		uint32_t ___timeout___ = tmr_u32_get_system_time_us(); \
    	while( expr ){ \
    		if( tmr_u32_elapsed_time_us( ___timeout___ ) > 1000000 ){ \
    			ASSERT( FALSE ); \
    		}\
	    } \
	}

#define BUSY_WAIT_TIMEOUT(expr, timeout) {\
		uint32_t ___timeout___ = tmr_u32_get_system_time_us(); \
    	while( expr ){ \
    		if( tmr_u32_elapsed_time_us( ___timeout___ ) > timeout ){ \
    			break; \
    		}\
	    } \
	}

#endif


