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

#include "system.h"

#include "hal_cpu.h"
#include "os_irq.h"
#include "timers.h"
#include "power.h"
#include "hal_timers.h"

volatile uint32_t meow;

TIM_TypeDef *timer2;

void hal_timer_v_init( void ){

	__HAL_RCC_TIM2_CLK_ENABLE();

	timer2 = TIM2;

	LL_TIM_InitTypeDef init;
	LL_TIM_StructInit( &init );

	init.Prescaler 			= 108;
	init.CounterMode 		= LL_TIM_COUNTERMODE_UP;
	init.Autoreload			= 0;
	init.ClockDivision      = LL_TIM_CLOCKDIVISION_DIV1;
	init.RepetitionCounter  = 0;

	LL_TIM_Init( HAL_SYS_TIMER, &init );

	LL_TIM_SetUpdateSource( HAL_SYS_TIMER, LL_TIM_UPDATESOURCE_COUNTER );
	LL_TIM_EnableUpdateEvent( HAL_SYS_TIMER ); 
	LL_TIM_EnableIT_UPDATE( HAL_SYS_TIMER );
    
    HAL_NVIC_SetPriority( TIM2_IRQn, 0, 0 );
    HAL_NVIC_EnableIRQ( TIM2_IRQn );

    LL_TIM_EnableCounter( HAL_SYS_TIMER );

    while(1){

    	meow = TIM2->CNT;
    }
}

bool tmr_b_io_timers_running( void ){

    return FALSE;
}


uint64_t tmr_u64_get_system_time_us( void ){

    return 0;
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


void TIM2_IRQHandler(void) {
    
    trace_printf("meow\n");
}
 

