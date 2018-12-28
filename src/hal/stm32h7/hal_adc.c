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
#include "adc.h"
#include "hal_io.h"
#include "hal_adc.h"
#include "keyvalue.h"
#include "logging.h"
#include "timers.h"
#include "config.h"

static ADC_HandleTypeDef hadc1;

static uint16_t vref = 3300;

static int8_t hal_adc_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

       if( hash == __KV__vcc ){

            uint16_t mv = adc_u16_read_vcc();
            memcpy( data, &mv, sizeof(mv) );
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t hal_adc_kv[] = {
    { SAPPHIRE_TYPE_UINT16,      0, KV_FLAGS_READ_ONLY, 0, hal_adc_kv_handler,   "vcc" },
};


PT_THREAD( hal_adc_thread( pt_t *pt, void *state ) );

void adc_v_init( void ){

	__HAL_RCC_ADC12_CLK_ENABLE();
  	
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode 		= GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull 		= GPIO_NOPULL;

    if( cfg_u16_get_board_type() == BOARD_TYPE_NUCLEAR ){
    
    	GPIO_InitStruct.Pin 		= VMON_Pin;	
    	HAL_GPIO_Init(VMON_GPIO_Port, &GPIO_InitStruct);
	}


	// hadc1.Instance = ADC1;
	// hadc1.Init.ClockPrescaler 			= ADC_CLOCK_SYNC_PCLK_DIV4;
	// hadc1.Init.Resolution 				= ADC_RESOLUTION_12B;
	// hadc1.Init.ScanConvMode 			= DISABLE;
	// hadc1.Init.ContinuousConvMode 		= DISABLE;
	// hadc1.Init.DiscontinuousConvMode 	= DISABLE;
	// hadc1.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_NONE;
	// hadc1.Init.ExternalTrigConv 		= ADC_SOFTWARE_START;
	// hadc1.Init.DataAlign 				= ADC_DATAALIGN_RIGHT;
	// hadc1.Init.NbrOfConversion 			= 1;
	// hadc1.Init.DMAContinuousRequests 	= DISABLE;
	// hadc1.Init.EOCSelection 			= ADC_EOC_SINGLE_CONV;
	// if (HAL_ADC_Init(&hadc1) != HAL_OK)
	// {
	// 	_Error_Handler(__FILE__, __LINE__);
	// }

	thread_t_create( hal_adc_thread,
                     PSTR("hal_adc"),
                     0,
                     0 );
}

PT_THREAD( hal_adc_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	TMR_WAIT( pt, 1000 );

	log_v_debug_P( PSTR("VCC: %u Vrefint: %u"), adc_u16_read_vcc(), adc_u16_read_raw( ADC_CHANNEL_REF ) );
    
    while(1){

        // vref calibration

        #define SAMPLES 8
        uint32_t accum = 0;

        for( uint32_t i = 0; i <SAMPLES; i++ ){

            accum += adc_u16_read_vcc();
            _delay_us(20);
        }

        vref = accum / SAMPLES;


        TMR_WAIT( pt, 4000 );
    }

PT_END( pt );
}


static int16_t _adc_i16_internal_read( uint8_t channel ){

    uint32_t internal_channel = 0;

    switch( channel ){
        case ADC_CHANNEL_VSUPPLY:
            internal_channel = ADC_CHANNEL_10;
            break;

        case ADC_CHANNEL_REF:
            internal_channel = ADC_CHANNEL_17;
            break;

        default:
        	return 0;
            break;
    }

	// /**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
	// */
	// ADC_ChannelConfTypeDef sConfig;
	// sConfig.Channel = internal_channel;
	// sConfig.Rank = ADC_REGULAR_RANK_1;
	// sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
	// sConfig.Offset = 0;
	// if (HAL_ADC_ConfigChannel( &hadc1, &sConfig ) != HAL_OK)
	// {
	// 	_Error_Handler( __FILE__, __LINE__ );
	// }

	// HAL_ADC_Start( &hadc1 );        
 //    HAL_ADC_PollForConversion( &hadc1, 5 );

 //    uint32_t value = HAL_ADC_GetValue( &hadc1 );
 	
 //    return (int16_t)value;
    return 0;
}

void adc_v_shutdown( void ){

	// __HAL_RCC_ADC1_CLK_DISABLE();
	
	HAL_GPIO_DeInit(VMON_GPIO_Port, VMON_Pin);
}

void adc_v_set_ref( uint8_t ref ){


} 

uint16_t adc_u16_read_raw( uint8_t channel ){

    return _adc_i16_internal_read( channel );
}

uint16_t adc_u16_read_supply_voltage( void ){

	uint16_t raw = adc_u16_read_raw( ADC_CHANNEL_VSUPPLY );
	uint16_t mv = adc_u16_convert_to_millivolts( raw );

    return mv * 2;
}

uint16_t adc_u16_read_vcc( void ){

	uint16_t vref_int = adc_u16_read_raw( ADC_CHANNEL_REF );
	// this is nominally 1.2V

    return ( 1200 * 4095 ) / vref_int;
}

uint16_t adc_u16_convert_to_millivolts( uint16_t raw_value ){

	uint32_t millivolts = 0;

	// assuming 3300 mv VREF at 12 bits (4095 codes)

	millivolts = (uint32_t)raw_value * vref;
	millivolts /= 4096;

	return millivolts;
}


