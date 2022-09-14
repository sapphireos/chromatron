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

#include "system.h"

#include "os_irq.h"
#include "timers.h"
#include "power.h"
#include "hal_timers.h"

/*


*/

// number of timer ticks per ms
#define TICKS_PER_MS 2000

// milliseconds per timer IRQ tick (for keeping system time)
#define TIMER_MS_PER_TICK 10

#define TIMER_TOP   ( TICKS_PER_MS * 10 )


static volatile uint64_t microseconds;


void hal_timer_v_init( void ){

    // compare match timing:
    // 16,000,000 / 8 = 2,000,000
    // 2,000,000 / 2000 = 1000 hz.
    OCR1A = TIMER_TOP - 1;
    
    TCCR1A = 0b00000000;
    TCCR1B = 0b00000010; // prescaler / 8
    TCCR1C = 0;
    
    TIMSK1 = 0b00000010; // compare match A interrupt enabled
}

// Return TRUE if any timers on the IO clock are running.
// The async timer is a different clock domain, so it does not count.
bool tmr_b_io_timers_running( void ){

    // timers are always running

    return TRUE;
}

uint64_t tmr_u64_get_system_time_us( void ){

    // full 64 bit system time is:
    // microseconds + ( current timer count * 2 )

    // read timer twice.
    // if second read is lower than the first,
    // the timer has rolled over. in that case,
    // manually increment microseconds by the overflow value.
    

    uint32_t timer1, timer2;
    uint64_t current_microseconds;
    bool overflow;

    /*
    Overflow scenario 1:

    CNT is 65535
    tick
    CNT is 0, overflow flag is set

    first read = 0
    overflow = true
    second read = 0 (or 1, it won't really matter)

    need to add the amount of rollover microseconds to the reading since the overflow
    interrupt has not run yet.

    
    Scenario 2:
    
    CNT is 65535

    first read = 65535
    overflow = false
    tick
    CNT is 0, overflow flag is set
    second read = 0
        
    need to compensate here as well (timer1 > timer2)
    

    Scenario 3:
    
    CNT is 65535
    tick
    CNT is 0, overflow flag is set

    first read = 0
    overflow = true
    second read = 0
    
    */

    ATOMIC;

    // first read from timer
    timer1 = TCNT1;

    // check for overflow
    overflow = ( TIFR1 & _BV(OCF1A) ) != 0;

    // second read from timer
    timer2 = TCNT1;


    // get copy of microsecond counter
    current_microseconds = microseconds;
    
    END_ATOMIC;

    // check for overflow while we read microseconds.
    // if so, we need to manually compensate.
    if( ( timer1 > timer2 ) || overflow ){

        current_microseconds += 10000;
    }

    return current_microseconds + ( timer2 * 2 );
}



// Timer 1 compare match interrupt:
ISR(TIMER1_COMPA_vect){
    
    while( TCNT1 > TIMER_TOP ){

        // increment system time
        microseconds += 10000;
        
        // subtract timer max
        TCNT1 -= TIMER_TOP;
    }
}