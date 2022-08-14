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

Base clock 32 MHz
Prescaler /64
Timer: 500 KHz (2 uS resolution)

Overflow rate: 7.63 Hz


On overflow: increment microseconds counter by 131072.


*/

#define OVERFLOW_MICROSECONDS ( 65536 * 2 )

static volatile uint64_t microseconds;


void hal_timer_v_init( void ){

    // enable overflow interrupt and set priority level to high
    // TCC1.INTCTRLA |= TC_OVFINTLVL_HI_gc;

    // start timer, prescaler /64
    // TCC1.CTRLA |= TC_CLKSEL_DIV64_gc;
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
    

    // uint32_t timer1, timer2;
    // uint64_t current_microseconds;
    // bool overflow;

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

    // ATOMIC;

    // // first read from timer
    // timer1 = TCC1.CNT;

    // // check for overflow
    // overflow = ( TCC1.INTFLAGS & TC1_OVFIF_bm ) != 0;

    // // second read from timer
    // timer2 = TCC1.CNT;


    // // get copy of microsecond counter
    // current_microseconds = microseconds;
    
    // END_ATOMIC;

    // // check for overflow while we read microseconds.
    // // if so, we need to manually compensate.
    // if( ( timer1 > timer2 ) || overflow ){

    //     current_microseconds += OVERFLOW_MICROSECONDS;
    // }

    // return current_microseconds + ( timer2 * 2 );
}


// ISR(TCC1_OVF_vect){
// OS_IRQ_BEGIN(TCC1_OVF_vect);
// ^^^ Do not use the OS_IRQ macros in this interrupt.
// This is because the IRQ timing will call into
// the system time functions, and the timer rollover
// will yield an incorrect timing update.

    // microseconds += OVERFLOW_MICROSECONDS;

// OS_IRQ_END();
// }

