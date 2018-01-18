/*
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
 */

#include "cpu.h"

#include "io.h"
#include "system.h"
#include "timers.h"
#include "keyvalue.h"
#include "config.h"
#include "adc.h"
#include "hal_cpu.h"

#include "power.h"

// #define NO_LOGGING
#include "logging.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

#ifdef ENABLE_POWER

static volatile pwr_wake_source_t8 wake_source;

static uint64_t sleep_time;
static uint16_t sleep_percent;

KV_SECTION_META kv_meta_t pwr_info_kv[] = {
    { SAPPHIRE_TYPE_UINT64,   KV_FLAGS_READ_ONLY,  &sleep_time, 0,     "sleep_time" },
    { SAPPHIRE_TYPE_UINT16,   KV_FLAGS_READ_ONLY,  &sleep_percent, 0,  "sleep_percent" },
};

/*

Simplified sleep scheduler?

ATOMIC

get next alarm
get now

int32_t time_to_alarm = next - now (seq arith)

if time_to_alarm < 250:
    OCR2A = TCNT2 + ( time_to_alarm )
    arm interrupt

else if time_to_alarm <= 0:
    disarm interrupt
    wake up scheduler

else:
    wait for next timer overflow

END_ATOMIC


If we set an arbitrary alarm on wakeup, we can remove the alarm_set logic.
We could use a long timeout, 10 minutes or more, and any thread timers will
override it.  This would need to be done on wake up, BEFORE the scheduler runs.


possible scheduler loop:


while( 1 ){

    ATOMIC

    get next alarm
    get now

    int32_t time_to_alarm = next - now (seq arith)

    if time_to_alarm <= 0:
        break

    else if time_to_alarm < 250:

        timer sync

        ocr2a_val = TCNT2 + ( time_to_alarm )

        OCR2A = ocr2a_val

        timer sync

        arm interrupt

        timer sync

        // safety check OCR2A is set ahead of TCNT2
        if( ocr2a_val == TCNT2 ){

            // re-run scheduler, deadline was too soon

            break
        }

    else:
        wait for next timer overflow

    END_ATOMIC

    sleep()

    if( threads signalled ){

        break;
    }
}

disarm interrupt


exit sleep handler (returns to scheduler)



Timer module should abstract register access and timer sync




6-30-2014

Power scheduler updates:

tmr_b_check_thread_wake() should set the symbol counter alarm.  We should
then have an alarm check API that gets the amount of time remaining to the
next alarm.  If it is negative or below a threshold, we don't go to sleep.
If it is beyond the threshold, we do go to sleep.

The sym count compare 3 IRQ should set the pwr wake event.  We can remove the
while loop.



7-10-2014

Current alarm scheduler still has problems. If we try to set an alarm that is within 50 microseconds,
the alarm will not be set.  However, the check thread wake API checks that the alarm is expired,
so there is a 50 microsecond window where the thread will not set and alarm, but will also not execute.
If no other threads execute, the scheduler goes to the power module to put the CPU to sleep.  And then
the watchdog wakes us up.

Maybe fixing the check wake API will fix the problem.  Or allowing the timer alarm API to set an alarm
that has already expired, or is about to.

This scheme has proved to be very brittle and difficult to debug.  We have threads setting information,
and then that information gets overwritten by another thread.  The original thread needs to be called
again to get its alarm information back.  We also can't have an API (like TMR_WAIT) where the only
thing that will wake a thread is the timer, and the thread will ONLY be called when the timer expires.
Right now, we have to call it every time the scheduler runs.

If we track timers for all threads, we can easily scan all threads for the soonest alarm.  An alarm
flag can indicate the alarm is set.  Use the wait flag for other wait APIs, so TMR_WAIT only sets
the alarm bit.  The bit clears when we run the thread.

Internally (in the thread state), we store the 64 bit microsecond system time when the thread will wake.
We can calculate the correct symbol counter alarm based on that and the current system time.

When checking alarms, we'll check the actual thread alarm against the current system time.



*/




void pwr_v_init( void ){

}

void pwr_v_wake_up( pwr_wake_source_t8 source ){

    ATOMIC;

    wake_source = source;

    END_ATOMIC;
}

bool pwr_b_is_wake_event_set( void ){

    bool temp;

    ATOMIC;
    temp = ( wake_source >= 0 );
    END_ATOMIC;

    return temp;
}

pwr_wake_source_t8 pwr_i8_get_wake_source( void ){

    pwr_wake_source_t8 temp;

    ATOMIC;
    temp = wake_source;
    END_ATOMIC;

    return temp;
}

void pwr_v_sleep( void ){

    if( ( !cfg_b_get_boolean( CFG_PARAM_ENABLE_CPU_SLEEP ) ) ||
        ( sys_u8_get_mode() == SYS_MODE_SAFE ) ){

        return;
    }

    // if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        // return;
    // }

    uint64_t sleep_at = tmr_u64_get_system_time_us();

    // shut down modules
    adc_v_shutdown();


    // loop so we go back to sleep after an interrupt, unless that
    // interrupt sets the wake event
    while( 1 ){

        int8_t status = pwr_i8_enter_sleep_mode();

        if( status != 0 ){
            // failed to enter sleep mode

            EVENT( EVENT_ID_DEBUG_3, 0 );
            goto end;
        }

        // zzzzzzzzzzzzz

        ATOMIC;
        // check wake up reason.
        if( pwr_i8_get_wake_source() < 0 ){

            // signalled thread
            if( thread_u16_get_signals() != 0 ){

                wake_source = PWR_WAKE_SOURCE_SIGNAL;
            }
        }
        END_ATOMIC;

        if( pwr_b_is_wake_event_set() ){

            break;
        }
    }

    pwr_wake_source_t8 source = pwr_i8_get_wake_source();

    if( source >= 0 ){

        uint64_t wake_at = tmr_u64_get_system_time_us();

        EVENT( EVENT_ID_CPU_WAKE, source );

        // uint64_t elapsed_time = tmr_u64_elapsed_times( sleep_at, wake_at );
        uint64_t elapsed_time = wake_at - sleep_at;

        sleep_time += elapsed_time;
        sleep_percent = ( sleep_time * 1000 ) / tmr_u64_get_system_time_us();
    }

end:
    _delay_us(1);
    // fixes compiler complaint about defining a variable right after a
    // label, which is what happens with the ATOMIC macro.

    ATOMIC;
    wake_source = -1;
    END_ATOMIC;
}

int8_t pwr_i8_enter_sleep_mode( void ){

    // disable interrupts
    DISABLE_INTERRUPTS;

    tmr_i8_set_alarm_microseconds( thread_i64_get_next_alarm() );

    // set alarm and check status
    // also check if any thread signals have occurred
    // or wake up events
    if( ( thread_u16_get_signals() != 0 ) ||
        ( pwr_b_is_wake_event_set() ) ||
        ( !tmr_b_alarm_armed() ) ){
        // ( tmr_i8_set_alarm_microseconds( thread_i64_get_next_alarm() ) < 0 ) ){

        // re-enable interrupts
        ENABLE_INTERRUPTS;

        return -1;
    }

    // set sleep mode
    cpu_v_sleep();

    tmr_v_cancel_alarm();

    return 0;
}


// initiates an immediate shutdown of the entire system
// this sets all IO to inputs with pull ups, forces the transceiver to
// sleep mode, and puts the CPU in power down with interrupts disabled.
// the system cannot wake up from a full shutdown, it must be power cycled.
void pwr_v_shutdown( void ){

    // disable interrupts
    cli();

    log_v_critical_P( PSTR("Full stop shutdown") );

	// disable watchdog timer
	sys_v_disable_watchdog();

    // set all IO to input and pull up
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLUP );
    }

    // turn off LEDs
    io_v_digital_write( IO_PIN_LED0, LOW );
    io_v_digital_write( IO_PIN_LED1, LOW );
    io_v_digital_write( IO_PIN_LED2, LOW );

    // set all LEDs to outputs
    // this minimizes leakage from the pull up through the LED
    // without this, you'll actually see the red LED glowing faintly.
    io_v_set_mode( IO_PIN_LED0, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED1, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED2, IO_MODE_OUTPUT );

    // set CPU to power down
    pwr_i8_enter_sleep_mode();

    for(;;);
}


void pwr_v_shutdown_leds( void ){

    // turn off LEDs
    io_v_digital_write( IO_PIN_LED0, LOW );
    io_v_digital_write( IO_PIN_LED1, LOW );
    io_v_digital_write( IO_PIN_LED2, LOW );

    // set all LEDs to outputs
    // this minimizes leakage from the pull up through the LED
    // without this, you'll actually see the red LED glowing faintly.
    io_v_set_mode( IO_PIN_LED0, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED1, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED2, IO_MODE_OUTPUT );
}

void pwr_v_shutdown_io( void ){

    // set all IO to input and pull up
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLUP );
    }

    pwr_v_shutdown_leds();
}

#endif
