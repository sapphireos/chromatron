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



#ifndef _THREADING_H
#define _THREADING_H

#include "cpu.h"

#include "thread.h"
#include "memory.h"
#include "pt.h"
#include "system.h"

#define THREAD_MAX_NAME_LEN 64

#define THREAD_MAX_SIGNALS  16

#define SIGNAL_SYS_0        0
#define SIGNAL_SYS_1        1
#define SIGNAL_SYS_2        2
#define SIGNAL_SYS_3        3
#define SIGNAL_SYS_4        4
#define SIGNAL_SYS_5        5
#define SIGNAL_SYS_6        6
#define SIGNAL_SYS_7        7
#define SIGNAL_APP_0        8
#define SIGNAL_APP_1        9
#define SIGNAL_APP_2        10
#define SIGNAL_APP_3        11
#define SIGNAL_APP_4        12
#define SIGNAL_APP_5        13
#define SIGNAL_APP_6        14
#define SIGNAL_APP_7        15


// this option will cause the scheduler to check that global interrupts
// are enabled after each thread returns.  it will assert if the thread
// has left the interrupts disabled.  this is to check for malfunctioning
// threads, since leaving interrupts disabled can cause the system to lock
// up.
#ifndef __SIM__
    #define ENABLE_THREAD_DISABLE_INTERRUPTS_CHECK
#endif

// #define ENABLE_STACK_LOGGING

typedef struct pt pt_t;

typedef struct{
	pt_t pt;    // protothread context
	PT_THREAD( ( *thread )( pt_t *pt, void *state ) );
	PGM_P name;
    uint8_t flags;
    uint32_t alarm;
    uint32_t run_time;
    uint32_t runs;
} thread_state_t;

typedef struct __attribute__((packed)){
    char name[THREAD_MAX_NAME_LEN];
    uint16_t flags;
    uint32_t thread_addr;
    uint16_t data_size;
    uint32_t run_time;
    uint32_t runs;
    uint16_t line;
    uint64_t alarm;
    uint8_t reserved[24];
} thread_info_t;

#define THREAD_FLAGS_WAITING		0b00000001
#define THREAD_FLAGS_YIELDED		0b00000010
#define THREAD_FLAGS_SLEEPING		0b00000100
#define THREAD_FLAGS_SIGNAL 		0b00001000
#define THREAD_FLAGS_ALARM          0b00010000

typedef struct{
    uint8_t signal;
    int32_t rate; // microseconds
    int32_t ticks;
} thread_timed_signal_t;

#define THREAD_MAX_TIMED_SIGNALS    4

#define THREAD_CAST( thread ) (PT_THREAD((*)(pt_t *pt, void *state )))thread

typedef struct{
    uint8_t max_threads;
    uint16_t run_time;
    uint16_t task_time;
    uint16_t sleep_time;
    uint16_t scheduler_loops;
} cpu_info_t;


void thread_v_init( void );

uint16_t thread_u16_get_thread_count( void );
thread_t thread_t_get_current_thread( void );

void thread_v_get_cpu_info( cpu_info_t *info );
void thread_v_dump( void );

thread_t thread_t_create( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                          PGM_P name,
                          void *initial_data,
                          uint16_t size );

thread_t thread_t_create_critical( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                                   PGM_P name,
                                   void *initial_data,
                                   uint16_t size );

PT_THREAD( ( *thread_p_get_function( thread_t thread_id ) ) )( pt_t *pt, void *state );
uint32_t thread_u32_get_current_addr( void );
void *thread_vp_get_data( thread_t thread_id );
void thread_v_restart( thread_t thread_id );
void thread_v_kill( thread_t thread_id );
void thread_v_active( void );

void thread_v_signal( uint8_t signum );
void thread_v_clear_signal( uint8_t signum );
bool thread_b_signalled( uint8_t signum );
void thread_v_set_signal_flag( void );
void thread_v_clear_signal_flag( void );
uint16_t thread_u16_get_signals( void );
void thread_v_create_timed_signal( uint8_t signum, uint8_t rate );

uint8_t thread_u8_get_run_cause( void );

void thread_v_set_alarm( uint32_t alarm );
uint32_t thread_u32_get_alarm( void );
void thread_v_clear_alarm( void );
bool thread_b_alarm_set( void );
bool thread_b_alarm( void );
uint32_t thread_u32_get_next_alarm( void );
uint32_t thread_u32_get_next_alarm_delta( void );

#if defined(__SIM__) || defined(ESP8266) || defined(ESP32)
    void thread_start( void );
#else
    void thread_start( void ) __attribute__ ((noreturn));
#endif

#if defined(ESP8266) || defined(ESP32)
void thread_core( void );
#endif


#define THREAD_SLEEP( pt ) \
	PT_SLEEP( pt ); \
	thread_v_active()

#define THREAD_WAIT_WHILE( pt, condition ) \
	PT_WAIT_WHILE( pt, condition ); \
	thread_v_active()

#define THREAD_YIELD( pt ) \
	PT_YIELD( pt ); \
	thread_v_active()

#define THREAD_WAIT_SIGNAL( pt, signum ) \
    thread_v_set_signal_flag(); \
    THREAD_WAIT_WHILE( pt, !thread_b_signalled( signum ) ); \
    thread_v_clear_signal( signum ); \
    thread_v_clear_signal_flag()

#define THREAD_RESTART( pt ) \
	PT_RESTART( pt ); \
	thread_v_active()

#define THREAD_EXIT( pt ) \
	PT_EXIT( pt )

#define THREAD_SPAWN( pt, child_context, child_thread ) \
    PT_INIT( &child_context ); \
    while( child_thread( &child_context, 0 ) < PT_EXITED ){ \
        THREAD_YIELD( pt ); \
    }

#endif
