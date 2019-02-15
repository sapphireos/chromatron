/*
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
 */


#include "system.h"

#include "pwm.h"
#include "hal_pwm.h"

#include "sapphire.h"


static TIM_HandleTypeDef pwm_timer;

static uint32_t get_channel( uint8_t channel ){

	if( channel == IO_PIN_T0 ){

		return 3;
	}
	else if( channel == IO_PIN_T1 ){

		return 4;
	}

	ASSERT( 0 );
}


void pwm_v_init( void ){

	pwm_timer.Instance = TIM4;

	HAL_TIM_PWM_Init( &pwm_timer );
}

void pwm_v_enable( uint8_t channel ){

	ASSERT( channel < N_PWM_CHANNELS );
		
	uint32_t timer_channel = get_channel( channel );

	HAL_TIM_PWM_Start( &pwm_timer, channel );
}

void pwm_v_disable( uint8_t channel ){

	ASSERT( channel < N_PWM_CHANNELS );

	uint32_t timer_channel = get_channel( channel );

	HAL_TIM_PWM_Stop( &pwm_timer, channel );
}

void pwm_v_write( uint8_t channel, uint16_t value ){

	ASSERT( channel < N_PWM_CHANNELS );
   
   	uint32_t timer_channel = get_channel( channel );
	
}

void pwm_v_init_channel( uint8_t channel, uint16_t freq ){

	ASSERT( channel < N_PWM_CHANNELS );

	uint32_t timer_channel = get_channel( channel );

	TIM_OC_InitTypeDef config;
	config.OCMode 		= TIM_OCMODE_PWM1;
	config.Pulse 		= 0;
	config.OCPolarity 	= TIM_OCPOLARITY_LOW;
	config.OCNPolarity 	= TIM_OCNPOLARITY_LOW;
	config.OCIdleState 	= TIM_OCIDLESTATE_SET;
	config.OCNIdleState = TIM_OCNIDLESTATE_SET;
	
	HAL_TIM_PWM_ConfigChannel( &pwm_timer, &config, timer_channel );
}







