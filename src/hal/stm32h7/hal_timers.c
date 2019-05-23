// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#include "hal_cpu.h"
#include "os_irq.h"
#include "timers.h"
#include "power.h"
#include "hal_timers.h"
#include "hal_watchdog.h"

#define OVERFLOW_MICROSECONDS ( 65536 * 1 )

static volatile uint64_t microseconds;

static TIM_HandleTypeDef system_timer;

void hal_timer_v_preinit( void ){

	// enable clock
	__HAL_RCC_TIM2_CLK_ENABLE();

    // Timer 2 is on APB1 (100 MHz nominally)
	system_timer.Instance = HAL_SYS_TIMER;

	system_timer.Init.Prescaler 		 = ( HAL_RCC_GetPCLK1Freq() * 2 ) / 1000000;
    system_timer.Init.Period             = 65535;
	system_timer.Init.CounterMode 		 = TIM_COUNTERMODE_UP;
	system_timer.Init.AutoReloadPreload  = TIM_AUTORELOAD_PRELOAD_ENABLE;
	system_timer.Init.ClockDivision      = TIM_CLOCKDIVISION_DIV1;
	system_timer.Init.RepetitionCounter  = 0;

	HAL_TIM_Base_Init( &system_timer );
}

void hal_timer_v_init( void ){
    
    HAL_NVIC_SetPriority( TIM2_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( TIM2_IRQn );

    HAL_TIM_Base_Start_IT( &system_timer );
}

bool tmr_b_io_timers_running( void ){

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
    timer1 = __HAL_TIM_GET_COUNTER( &system_timer );

    // check for overflow
    overflow = __HAL_TIM_GET_FLAG( &system_timer, TIM_IT_UPDATE ) != 0;

    // second read from timer
    timer2 = __HAL_TIM_GET_COUNTER( &system_timer );


    // get copy of microsecond counter
    current_microseconds = microseconds;
    
    END_ATOMIC;

    // check for overflow while we read microseconds.
    // if so, we need to manually compensate.
    if( ( timer1 > timer2 ) || overflow ){

        current_microseconds += OVERFLOW_MICROSECONDS;
    }

    return current_microseconds + timer2;
}


int8_t tmr_i8_set_alarm_microseconds( int64_t alarm ){

    return 0;
}

void tmr_v_cancel_alarm( void ){

    
}

// return true if alarm is armed
bool tmr_b_alarm_armed( void ){

    return 0;
}


ISR(TIM2_IRQHandler){
    	
    if( !__HAL_TIM_GET_FLAG( &system_timer, TIM_IT_UPDATE ) ){

    	return;
    }

    // clear flag
    __HAL_TIM_CLEAR_FLAG( &system_timer, TIM_IT_UPDATE );

	microseconds += OVERFLOW_MICROSECONDS;

    // // check if we need to arm the alarm
    // if( alarm_microseconds > 0 ){

    //     if( alarm_microseconds >= OVERFLOW_MICROSECONDS ){

    //         alarm_microseconds -= OVERFLOW_MICROSECONDS;

    //         // check if alarm is very very soon
    //         if( alarm_microseconds < 32 ){

    //             // alarm now
    //             alarm();
    //         }
    //         else{

    //             // arm timer compare for alarm
    //             TCC1.CCA = TCC1.CNT + MICROSECONDS_TO_TIMER_TICKS( alarm_microseconds );

    //             arm_alarm();    
    //         }
    //     }

    // }

    hal_wdg_v_kick();
}

