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
#include "hal_cpu.h"
#include "os_irq.h"
#include "hal_watchdog.h"
#include "watchdog.h"

#include "stm32h7xx_hal_iwdg.h"


static IWDG_HandleTypeDef wdg;

// system timer kicks the iwdg at 65.536 millisecond intervals
#define WDG_TIMEOUT 64 // approx 4.2 second timeout
// #define WDG_TIMEOUT 255

static volatile bool wdg_enabled;
static volatile uint8_t wdg_timer;


void wdg_v_reset( void ){

	ATOMIC;
	if( wdg_enabled ){

		wdg_timer = WDG_TIMEOUT;
	}
	END_ATOMIC;
}

void wdg_v_enable( wdg_timeout_t8 timeout, wdg_flags_t8 flags ){

	// once the IWDG has been started, it cannot be stopped
	wdg.Instance = IWDG1;

	wdg.Init.Prescaler 	= IWDG_PRESCALER_256;
	wdg.Init.Reload 	= 0x0FFF;
	wdg.Init.Window     = 0x0FFF; // disables window function

	HAL_IWDG_Init( &wdg );

	ATOMIC;
	wdg_enabled = TRUE;
	wdg_timer = WDG_TIMEOUT;
	END_ATOMIC;
}

void wdg_v_disable( void ){

	ATOMIC;
	wdg_enabled = FALSE;
	END_ATOMIC;
}

void hal_wdg_v_kick( void ){

	__HAL_IWDG_RELOAD_COUNTER( &wdg );

	if( wdg_enabled ){

		wdg_timer--;

		ASSERT( wdg_timer != 0 );
	}
}

