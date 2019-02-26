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

	return 0;
}


void pwm_v_init( void ){

	pwm_timer.Instance = TIM4;
	pwm_timer.Init.Prescaler 			= 0;
	pwm_timer.Init.CounterMode 			= TIM_COUNTERMODE_UP;
	pwm_timer.Init.Period 				= 65535; // 16 bits
	pwm_timer.Init.ClockDivision 		= TIM_CLOCKDIVISION_DIV1;
	pwm_timer.Init.RepetitionCounter 	= 0;
	pwm_timer.Init.AutoReloadPreload 	= TIM_AUTORELOAD_PRELOAD_DISABLE;

	HAL_TIM_Base_Init( &pwm_timer );

	HAL_TIM_PWM_Init( &pwm_timer );
}

void pwm_v_enable( uint8_t channel ){

	uint32_t timer_channel = get_channel( channel );

	HAL_TIM_PWM_Start( &pwm_timer, timer_channel );
}

void pwm_v_disable( uint8_t channel ){

	uint32_t timer_channel = get_channel( channel );

	HAL_TIM_PWM_Stop( &pwm_timer, timer_channel );
}

void pwm_v_write( uint8_t channel, uint16_t value ){
   
   	// uint32_t timer_channel = get_channel( channel );
	
}

void pwm_v_init_channel( uint8_t channel, uint16_t freq ){

	GPIO_InitTypeDef GPIO_InitStruct;
	
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    // GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;	

    GPIO_TypeDef *port;
    uint32_t pin;
    hal_io_v_get_port( channel, &port, &pin );

    // init IO
	GPIO_InitStruct.Pin 		= pin;
	GPIO_InitStruct.Alternate 	= GPIO_AF2_TIM4;
	HAL_GPIO_Init( port, &GPIO_InitStruct );
	// HAL_GPIO_WritePin( port, pin, GPIO_PIN_SET );
	

	uint32_t timer_channel = get_channel( channel );

	TIM_OC_InitTypeDef config;
	config.OCMode 		= TIM_OCMODE_PWM1;
	config.Pulse 		= 500;
	config.OCPolarity 	= TIM_OCPOLARITY_HIGH;
	config.OCNPolarity 	= TIM_OCNPOLARITY_HIGH;
	config.OCIdleState 	= TIM_OCIDLESTATE_RESET;
	config.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	
	HAL_TIM_PWM_ConfigChannel( &pwm_timer, &config, timer_channel );
}







