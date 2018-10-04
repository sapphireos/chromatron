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
#include "hal_rtc.h"

static uint16_t current_period;

ISR(RTC_OVF_vect){

	hal_rtc_v_irq();

	// set next period
	RTC.PER = current_period;
}
	
// default handler
 __attribute__((weak)) void hal_rtc_v_irq( void ){


}

void hal_rtc_v_init( void ){

	// set RTC to run on 32.768 khz crystal oscillator
	// and enable RTC
	CLK.RTCCTRL = CLK_RTCSRC_TOSC32_gc | CLK_RTCEN_bm;

	// set overflow period to one second
	RTC.PER = 32767;

	// enable overflow interrupt
	// set for low priority
	RTC.INTCTRL = RTC_OVFINTLVL_LO_gc;
}

uint16_t hal_rtc_u16_get_period( void ){

	return current_period;
}

void hal_rtc_v_set_period( uint16_t period ){

	ATOMIC;
	current_period = period;
	END_ATOMIC;
}

