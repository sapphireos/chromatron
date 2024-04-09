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



/*

Thread timing improvements:

Rate: run thread at specific multiple of 10 ms.
The threading system drives a high priority 10 ms clock.
Any thread with the rate flag set will run when the tick hits its
modulo of the current clock.


Add flag for rate control (similar to current alarm).
The alarm field becomes the rate field.

There will need to be an accurate 10 ms clock driving the scheduler.
Rate-driven threads should run at higher priority than others, except signals.

Stats should be collected on rate driven threads: avg and max timings at least.

Rate threads should run in a priority order, with higher rates running first.



The overall idea is to get better control and accuracy of timed tasks and hopefully
improve the amount of time the CPU can go to sleep and minimize wakeups while still maintaining
timing.  The same amount of work needs to be done, but it is more efficient if it is coordinated
to maximize work done on a wake up.  The current system spreads tasks out haphazardly.



*/



#include "cpu.h"

#include "system.h"
#include "keyvalue.h"

#include "timers.h"
#include "threading.h"
#include "memory.h"
#include "list.h"
#include "fs.h"
#include "power.h"
#include "background.h"

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

#include <string.h>

/*
Basic thread control:
Wait & Yield

Wait - Thread waits on a given condition.  The processor is allowed to go to
sleep during the wait, which means the thread might not run again until the
next hardware interrupt.  This is best used for low priority and non time
critical functions.

Yield - Thread yields to allow other threads to run.  This implies the thread
is not finished processing, and as such, will be guaranteed to be run again
before sleeping the processor.


*/

// thread state storage
static list_t thread_list;

// currently running thread
static thread_t current_thread;
#ifdef AVR
static uint16_t current_thread_addr;
#else
static uint32_t current_thread_addr;
#endif
static uint8_t run_cause;

// CPU usage info
static cpu_info_t cpu_info;
static uint32_t task_us;
static uint32_t sleep_us;
static uint16_t loops;

static volatile uint8_t thread_flags;
#define FLAGS_SLEEP         0x02
#define FLAGS_ACTIVE        0x04

static volatile uint16_t signals;
static thread_timed_signal_t timed_signals[THREAD_MAX_TIMED_SIGNALS];
static uint32_t last_timed_signal_check;

#ifdef ENABLE_STACK_LOGGING
static uint16_t last_stack;
#endif


// KV:
static int8_t thread_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

        if( hash == __KV__thread_count ){

            uint8_t a = thread_u16_get_thread_count();
            memcpy( data, &a, sizeof(a) );
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t thread_info_kv[] = {
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  0, thread_i8_kv_handler,       "thread_count" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &cpu_info.max_threads,     0,  "thread_peak" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &cpu_info.run_time,        0,  "thread_run_time" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &cpu_info.task_time,       0,  "thread_task_time" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &cpu_info.sleep_time,      0,  "thread_sleep_time" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &cpu_info.scheduler_loops, 0,  "thread_loops" },
    // { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &signals, 0,  "thread_signals" },
};


PT_THREAD( cpu_stats_thread( pt_t *pt, void *state ) );


static uint32_t vfile( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    uint32_t ret_val = 0;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:

            // iterate over data length and fill file info buffers as needed
            while( len > 0 ){

                uint8_t page = pos / sizeof(thread_info_t);

                // get thread state
                thread_t thread = list_ln_index( &thread_list, page );
                thread_state_t *state = list_vp_get_data( thread );

                // set up info page
                thread_info_t info;
                memset( &info, 0, sizeof(info) );

                strlcpy_P( info.name, state->name, sizeof(info.name) );
                info.flags          = state->flags;

                #ifdef AVR
                info.thread_addr    = (uint32_t)((uintptr_t)state->thread) * 2; //multiply by 2 to get byte address
                #else
                info.thread_addr    = (uint32_t)((uintptr_t)state->thread);
                #endif
                info.data_size      = list_u16_node_size( thread ) - sizeof(thread_state_t);
                info.run_time       = state->run_time;
                info.runs           = state->runs;
                info.line           = state->pt.lc;
                info.alarm          = state->alarm;
                info.max_time       = state->max_time;

                // get offset info page
                uint16_t offset = pos - ( page * sizeof(info) );

                // set copy length
                uint16_t copy_len = sizeof(info) - offset;

                if( copy_len > len ){

                    copy_len = len;
                }

                // copy data
                memcpy( ptr, (void *)&info + offset, copy_len );

                // adjust pointers
                ptr += copy_len;
                len -= copy_len;
                pos += copy_len;
                ret_val += copy_len;
            }

            break;

        case FS_VFILE_OP_SIZE:
            ret_val = thread_u16_get_thread_count() * sizeof(thread_info_t);
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}


// initialize the thread scheduler
void thread_v_init( void ){

    // init thread list
    list_v_init( &thread_list );
}

// return current number of threads
uint16_t thread_u16_get_thread_count( void ){

	return list_u8_count( &thread_list );
}

thread_t thread_t_get_current_thread( void ){

	return current_thread;
}

void thread_v_get_cpu_info( cpu_info_t *info ){

    *info = cpu_info;
}

uint8_t thread_u8_get_cpu_percent( void ){

    return cpu_info.task_time * 100 / cpu_info.run_time;
}

void thread_v_dump( void ){

    // delete file
    file_t f1 = fs_f_open_P( PSTR("threadinfo.dump"), FS_MODE_WRITE_OVERWRITE );

    if( f1 > 0 ){

        fs_v_delete( f1 );
        fs_f_close( f1 );
    }

    f1 = fs_f_open_P( PSTR("threadinfo.dump"), FS_MODE_CREATE_IF_NOT_FOUND );

    if( f1 < 0 ){

        return;
    }

    file_t f2 = fs_f_open_P( PSTR("threadinfo"), FS_MODE_READ_ONLY );

    if( f2 < 0 ){

        fs_f_close( f1 );

        return;
    }

    uint8_t data[64];
    int16_t bytes_read;

    do{

        bytes_read = fs_i16_read( f2, data, sizeof(data) );

        if( bytes_read > 0 ){

            fs_i16_write( f1, data, bytes_read );
        }

    } while( bytes_read > 0 );


    fs_f_close( f1 );
    fs_f_close( f2 );
}

static thread_t make_thread( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                             PGM_P name,
                             void *initial_data,
                             uint16_t size,
                             uint8_t flags ){

    list_node_t ln = list_ln_create_node2( 0, sizeof(thread_state_t) + size, MEM_TYPE_THREAD );

    // check if node was created
    if( ln < 0 ){

        return -1;
    }

    // get state
    thread_state_t *state = list_vp_get_data( ln );

    // initialize the thread context
    PT_INIT( &state->pt );

    state->thread   = thread;
    state->flags    = flags;
    state->name     = name;
    state->run_time = 0;
    state->runs     = 0;
    state->alarm    = 0;
    state->max_time = 0;

    // copy data (if present)
    if( initial_data != 0 ){

        void *thread_data = state + 1;

        memcpy( thread_data, initial_data, size );
    }

    // check max threads
    if( thread_u16_get_thread_count() > cpu_info.max_threads ){

        cpu_info.max_threads = thread_u16_get_thread_count();
    }

    // add to list
    list_v_insert_tail( &thread_list, ln );

    return ln;
}

// create a thread and return its handle
thread_t thread_t_create( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                          PGM_P name,
                          void *initial_data,
                          uint16_t size ){

    return make_thread( thread, name, initial_data, size, THREAD_FLAGS_YIELDED );
}

// creates a thread and asserts if the thread could not be
// created.  this should only be used for critical system threads.
thread_t thread_t_create_critical( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                                   PGM_P name,
                                   void *initial_data,
                                   uint16_t size ){

    thread_t t = thread_t_create( thread, name, initial_data, size );

    ASSERT( t > 0);

    return t;
}

PT_THREAD( ( *thread_p_get_function( thread_t thread_id ) ) )( pt_t *pt, void *state ){

    thread_state_t *state = list_vp_get_data( thread_id );

	return state->thread;
}

uint32_t thread_u32_get_current_addr( void ){

    #ifdef AVR
    // multiply the address by 2 to get the byte address.
    // this makes it easy to look up the thread function in the .lss file.
    return current_thread_addr << 1;
    #else
    return current_thread_addr;
    #endif
}

void *thread_vp_get_data( thread_t thread_id ){

    thread_state_t *state = list_vp_get_data( thread_id );

    return state + 1;
}

// restart a thread
void thread_v_restart( thread_t thread_id ){

    thread_state_t *state = list_vp_get_data( thread_id );

	state->flags = THREAD_FLAGS_YIELDED;

	PT_INIT( &state->pt );
}

// kill a thread
void thread_v_kill( thread_t thread_id ){

    // remove from list
    list_v_remove( &thread_list, thread_id );

    // release node
    list_v_release_node( thread_id );
}


// active mode, set sleep to false
void thread_v_active( void ){

	thread_flags |= FLAGS_ACTIVE;
	thread_flags &= ~FLAGS_SLEEP;

    thread_v_clear_alarm();
}

void thread_v_signal( uint8_t signum ){

    ASSERT( signum < THREAD_MAX_SIGNALS );

    ATOMIC;

    signals |= ( (uint16_t)1 << signum );

    END_ATOMIC;

    EVENT( EVENT_ID_SIGNAL, signum );
}

void thread_v_clear_signal( uint8_t signum ){

    ASSERT( signum < THREAD_MAX_SIGNALS );

    ATOMIC;

    signals &= ~( (uint16_t)1 << signum );

    END_ATOMIC;
}

bool thread_b_signalled( uint8_t signum ){

    ASSERT( signum < THREAD_MAX_SIGNALS );

    uint16_t sig_copy;

    ATOMIC;

    sig_copy = signals;

    END_ATOMIC;

    return ( sig_copy & ( (uint16_t)1 << signum ) ) != 0;
}

void thread_v_set_signal_flag( void ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );

	state->flags |= THREAD_FLAGS_SIGNAL;
}

void thread_v_clear_signal_flag( void ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );

	state->flags &= ~THREAD_FLAGS_SIGNAL;
}

uint16_t thread_u16_get_signals( void ){

    uint16_t sig_copy;

    ATOMIC;

    sig_copy = signals;

    END_ATOMIC;

    return sig_copy;
}

void thread_v_create_timed_signal( uint8_t signum, uint8_t rate ){

    // cannot create timed signals in safe mode
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    // check if signal is already registered
    for( uint8_t i = 0; i < cnt_of_array(timed_signals); i++ ){

        if( timed_signals[i].signal == signum ){

            // update rate
            timed_signals[i].rate = rate * 1000; // convert to microseconds
            timed_signals[i].ticks = timed_signals[i].rate;

            return;
        }
    }

    for( uint8_t i = 0; i < cnt_of_array(timed_signals); i++ ){

        if( timed_signals[i].rate == 0 ){

            // add signal

            timed_signals[i].signal = signum;
            timed_signals[i].rate = rate * 1000; // convert to microseconds
            timed_signals[i].ticks = timed_signals[i].rate;

            return;
        }
    }

    // no signals available
    ASSERT( FALSE );
}

void thread_v_destroy_timed_signal( uint8_t signum ){

    // cannot use timed signals in safe mode
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    for( uint8_t i = 0; i < cnt_of_array(timed_signals); i++ ){

        if( timed_signals[i].signal == signum ){

            timed_signals[i].signal = 0;
            timed_signals[i].rate = 0;

            return;
        }
    }
}

uint8_t thread_u8_get_run_cause( void ){

    return run_cause;
}

void thread_v_set_alarm( uint32_t alarm ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );

    state->alarm = alarm;
    state->flags |= THREAD_FLAGS_ALARM;
}

uint32_t thread_u32_get_alarm( void ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );
    
    return state->alarm;
}

void thread_v_clear_alarm( void ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );

    state->flags &= ~THREAD_FLAGS_ALARM;
}

bool thread_b_alarm_set( void ){

    thread_state_t *state = list_vp_get_data( thread_t_get_current_thread() );

    return ( state->flags & THREAD_FLAGS_ALARM ) != 0;
}

bool thread_b_alarm( void ){

    return ( run_cause & THREAD_FLAGS_ALARM ) != 0;
}

uint32_t thread_u32_get_next_alarm( void ){

    uint32_t next_alarm = 0;
    bool init = FALSE;

    // iterate through thread list
    list_node_t ln = thread_list.head;

    while( ln >= 0 ){

        list_node_state_t *ln_state = mem2_vp_get_ptr_fast( ln );
        thread_state_t *state = (thread_state_t *)&ln_state->data;

        if( ( state->flags & THREAD_FLAGS_ALARM ) != 0 ){

            if( ( !init ) || tmr_i8_compare_times( state->alarm, next_alarm ) ){

                next_alarm = state->alarm;                
                init = TRUE;
            }
        }

        ln = ln_state->next;
    }

    return next_alarm;
}

uint32_t thread_u32_get_next_alarm_delta( void ){

    uint32_t next_alarm = thread_u32_get_next_alarm();
    uint32_t now = tmr_u32_get_system_time_ms();

    if( tmr_i8_compare_times( now, next_alarm ) >= 0 ){

        return 0;
    }

    uint32_t delta = tmr_u32_elapsed_times( now, next_alarm );

    return delta;
}


void run_thread( thread_t thread, thread_state_t *state ){

    uint32_t thread_us = tmr_u32_get_system_time_us();

	// set current thread
	current_thread = thread;
    #ifdef AVR
    current_thread_addr = (uint16_t)state->thread;
    #else
    current_thread_addr = (uint32_t)state->thread;
    #endif

    // clear active flag
    thread_flags &= ~FLAGS_ACTIVE;

    // get pointer to state memory
    void *ptr = state + 1;

    // stack check
    // this is in case a thread overflows the stack.
    // this variable will (probably) get stomped on first,
    volatile uint32_t stack_check = 0x12345678;

    // *******************************************
    // Run the thread
    char status = state->thread( &state->pt, ptr );
    // *******************************************

    // .... and then we'll assert after the thread
    // smashes the stack.
    ASSERT( stack_check == 0x12345678 );

    #ifdef ENABLE_STACK_LOGGING

    uint16_t stack_usage = mem2_u16_stack_count();

    if( stack_usage > last_stack ){

        last_stack = stack_usage;

        char thread_name[THREAD_MAX_NAME_LEN];
        strlcpy_P( thread_name, state->name, sizeof(thread_name) );
        log_v_debug_P( PSTR("Thread: %s Stack: %d"), thread_name, stack_usage );
    }

    #endif

    // compute run time
    if( thread_flags & FLAGS_ACTIVE ){

        uint32_t last_run_time = state->run_time;
        uint32_t elapsed_us = tmr_u32_elapsed_time_us( thread_us );
        task_us += elapsed_us;
        state->run_time += elapsed_us;

        // check for overflow
        if( state->run_time < last_run_time ){

            state->run_time = 0xffffffff;
        }

        // check for overflow
        if( state->runs < 0xffffffff ){

            state->runs++;
        }

        if( elapsed_us > state->max_time ){

            state->max_time = elapsed_us;
        }
    }

    // check returned thread state
    switch( status ){

        // thread is waiting.
        // generally, this is used if the thread is waiting on
        // some resource which is indirectly tied to hardware.
        // as such, we won't increment the ready threads count
        // for this case, so the processor can eventually go to
        // sleep.
        // only use this when its ok if the processor goes to
        // sleep during the wait.  If it is imperative to keep
        // the processor awake, use the yield instead.
        case PT_WAITING:

            state->flags |= THREAD_FLAGS_WAITING;

            break;

        // thread yielded, it has more processing to do
        case PT_YIELDED:

            state->flags |= THREAD_FLAGS_YIELDED;

            break;

        // thread has gone to sleep
        case PT_SLEEPING:

            state->flags |= THREAD_FLAGS_SLEEPING;

            break;

        // if the thread has completed,
        case PT_EXITED:
        case PT_ENDED:

            // remove the thread
            thread_v_kill( current_thread );

            break;

        default:
            break;
    }

    run_cause = 0;

    #ifdef ENABLE_THREAD_DISABLE_INTERRUPTS_CHECK
        #ifdef AVR
        // check if the global interrupts are still enabled
        ASSERT_MSG( ( SREG & 0x80 ) != 0, "Global interrupts disabled!" );
        #endif
    #endif
}


static uint32_t process_timed_signals( void ){

    uint32_t now = tmr_u32_get_system_time_us();
    uint32_t elapsed = tmr_u32_elapsed_times( last_timed_signal_check, now );

    uint32_t min_time_remaining = 0xffffffff;

    for( uint8_t i = 0; i < cnt_of_array(timed_signals); i++ ){

        if( timed_signals[i].rate <= 0 ){

            continue;
        }

        timed_signals[i].ticks -= elapsed;

        if( timed_signals[i].ticks <= 0 ){

            timed_signals[i].ticks += timed_signals[i].rate;

            signals |= ( 1 << timed_signals[i].signal );
        }
        
        if( timed_signals[i].ticks < (int32_t)min_time_remaining ){

            // track minimum time left on any timed signal

            min_time_remaining = timed_signals[i].ticks;
        }
    }

    last_timed_signal_check = now;

    return min_time_remaining / 1000; // convert to milliseconds
}

static void process_signalled_threads( void ){

    uint8_t count = 0;

    while( ( thread_u16_get_signals() != 0 ) && ( count < 16 ) ){

        count++;

        // iterate through thread list
        list_node_t ln = thread_list.head;

        while( ln >= 0 ){

            list_node_state_t *ln_state = mem2_vp_get_ptr_fast( ln );
            thread_state_t *state = (thread_state_t *)&ln_state->data;

            if( ( state->flags & THREAD_FLAGS_SIGNAL ) != 0 ){

                run_cause = THREAD_FLAGS_SIGNAL;

                // clear wait flags
                state->flags &= ~THREAD_FLAGS_WAITING;
                state->flags &= ~THREAD_FLAGS_YIELDED;

                run_thread( ln, state );
            }

            ln = ln_state->next;
        }
    }
}


// void process_alarm_threads( void ){

//     // iterate through thread list
//     list_node_t ln = thread_list.head;

//     while( ln >= 0 ){

//         list_node_state_t *ln_state = mem2_vp_get_ptr_fast( ln );
//         thread_state_t *state = (thread_state_t *)&ln_state->data;

//         if( ( ( state->flags & THREAD_FLAGS_ALARM ) != 0 ) &&
//             ( tmr_i8_compare_time( state->alarm ) < 0 ) ){

//             // clear flags
//             state->flags &= ~THREAD_FLAGS_ALARM;
//             state->flags &= ~THREAD_FLAGS_SLEEPING;

//             run_thread( ln, state );
//         }

//         ln = ln_state->next;
//     }
// }

int32_t thread_core( void ){

    // set sleep flag
    thread_flags |= FLAGS_SLEEP;

    // process signals first
    uint32_t timed_signals_ms_remaining = process_timed_signals();
    process_signalled_threads();

    // ********************************************************************
    // Process Waiting threads
    //
    // Loop through all waiting threads
    // ********************************************************************
    list_node_t ln = thread_list.head;

    while( ln >= 0 ){

        list_node_state_t *ln_state = mem2_vp_get_ptr_fast( ln );

        thread_state_t *state = (thread_state_t *)&ln_state->data;

        if( ( ( state->flags & THREAD_FLAGS_ALARM ) != 0 ) &&
              ( tmr_i8_compare_time( state->alarm ) < 0 ) ){

            run_cause = THREAD_FLAGS_ALARM;

            // clear flags
            state->flags &= ~THREAD_FLAGS_ALARM;
            state->flags &= ~THREAD_FLAGS_SLEEPING;
            state->flags &= ~THREAD_FLAGS_WAITING;
            state->flags &= ~THREAD_FLAGS_YIELDED;

            run_thread( ln, state );
        }
        else if( ( ( ( state->flags & THREAD_FLAGS_WAITING ) != 0 ) ||
                   ( ( state->flags & THREAD_FLAGS_YIELDED ) != 0 ) ) ){

            // clear wait flags
            state->flags &= ~THREAD_FLAGS_WAITING;
            state->flags &= ~THREAD_FLAGS_YIELDED;
            state->flags &= ~THREAD_FLAGS_SLEEPING;

            // run the thread
            run_thread( ln, state );
        }

        timed_signals_ms_remaining = process_timed_signals();
        process_signalled_threads();

        #ifdef ENABLE_USB
        usb_v_poll();
        #endif

        ln = ln_state->next;
    }

    mem2_v_collect_garbage();        

    if( loops < 0xffff ){

        loops++;
    }

    // ********************************************************************
    // Check for sleep conditions
    //
    // If no IRQ threads, and all threads are asleep, enter sleep mode
    // ********************************************************************
    #ifdef ENABLE_POWER
    #ifdef ESP8266

    if( ( thread_flags & FLAGS_SLEEP ) &&
        ( thread_u16_get_signals() == 0 )  ){

        uint32_t next_alarm_delta = thread_u32_get_next_alarm_delta();

        // check if timed signal is sooner
        if( timed_signals_ms_remaining < next_alarm_delta ){

            next_alarm_delta = timed_signals_ms_remaining;   
        }

        return next_alarm_delta;
    }

    #else

    if( ( thread_flags & FLAGS_SLEEP ) &&
        ( thread_u16_get_signals() == 0 ) ){

        uint32_t sleep_start = tmr_u32_get_system_time_us();

        pwr_v_sleep();

        // zzzzzzzzzzzzz

        sleep_us += tmr_u32_elapsed_time_us( sleep_start );
    }

    #endif
    #endif

    #ifdef __SIM__
    break;
    #endif

    return 0;
}

#ifdef ESP8266
void thread_v_add_sleep_time( uint32_t sleep_time ){

    sleep_us += sleep_time;
}
#endif

// start the thread scheduler
void thread_start( void ){

    #ifdef ENABLE_STACK_LOGGING

    uint16_t stack_size = mem2_u16_stack_count();

    last_stack = stack_size;

    log_v_debug_P( PSTR("Init: Stack: %d"), stack_size );

    #endif

	// start the background threads
    background_v_init();
    thread_t_create( cpu_stats_thread, PSTR("cpu_stats"), 0, 0 );

    // create vfile
    fs_f_create_virtual( PSTR("threadinfo"), vfile );


    #if defined(ESP8266) || defined(ESP32)

    #else
	// infinite loop running the thread scheduler
	while(1){

		thread_core();
	}
    #endif
}

PT_THREAD( cpu_stats_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint16_t timestamp;

    while(1){

        // mark current timestamp
        timestamp = tmr_u32_get_system_time_ms();

        // reset counters
        loops = 0;
        task_us = 0;
        sleep_us = 0;

        TMR_WAIT( pt, 5000 );

        cpu_info.run_time = tmr_u32_elapsed_time_ms( timestamp );
        cpu_info.task_time = task_us / 1000;
        cpu_info.sleep_time = sleep_us / 1000;
        cpu_info.scheduler_loops = loops;

        EVENT( EVENT_ID_DEBUG_0, 0 );
    }

PT_END( pt );
}
