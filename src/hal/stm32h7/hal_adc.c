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

/*

STM32H7

Configured for 12 bit conversions!

*/

static ADC_HandleTypeDef hadc1;
static ADC_HandleTypeDef hadc2;
static ADC_HandleTypeDef hadc3;


typedef struct{
	uint32_t pin;
	GPIO_TypeDef *port;
	ADC_HandleTypeDef *adc;
	uint32_t channel;
} adc_ch_t;

static const adc_ch_t channels_nuclear[] = {
	{VMON_Pin, 		VMON_GPIO_Port, 	&hadc3, ADC_CHANNEL_5},
	{0, 			0, 					&hadc3, ADC_CHANNEL_19},
};

static const adc_ch_t channels_ctx[] = {
	{MEAS1_Pin, 	MEAS1_GPIO_Port, 	&hadc1, ADC_CHANNEL_8},
	{0, 		0, 				     	&hadc3, ADC_CHANNEL_19},
};

static const adc_ch_t *adc_channels;
static uint8_t adc_channel_count;

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
return;
	__HAL_RCC_ADC12_CLK_ENABLE();
	__HAL_RCC_ADC3_CLK_ENABLE();

	if( cfg_u16_get_board_type() == BOARD_TYPE_NUCLEAR ){

		adc_channels = channels_nuclear;
		adc_channel_count = cnt_of_array(channels_nuclear);
	}
	else if( cfg_u16_get_board_type() == BOARD_TYPE_CHROMATRON_X ){

		adc_channels = channels_ctx;
		adc_channel_count = cnt_of_array(channels_ctx);
	}

  	
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Alternate 	= 0;
    GPIO_InitStruct.Mode 		= GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull 		= GPIO_NOPULL;
    GPIO_InitStruct.Pin 		= adc_channels[0].pin;
	HAL_GPIO_Init(adc_channels[0].port, &GPIO_InitStruct);
	

	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler 			= ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution 				= ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode 			= DISABLE;
	hadc1.Init.EOCSelection 			= ADC_EOC_SINGLE_CONV;
	hadc1.Init.LowPowerAutoWait 		= DISABLE;	
	hadc1.Init.ContinuousConvMode 		= DISABLE;
	hadc1.Init.NbrOfConversion 			= 1;
	hadc1.Init.DiscontinuousConvMode 	= DISABLE;
	hadc1.Init.NbrOfDiscConversion 		= 1;
	hadc1.Init.ExternalTrigConv 		= ADC_SOFTWARE_START;
	hadc1.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.ConversionDataManagement	= ADC_CONVERSIONDATA_DR;
	hadc1.Init.Overrun					= ADC_OVR_DATA_OVERWRITTEN;
	hadc1.Init.LeftBitShift				= ADC_LEFTBITSHIFT_NONE;
	hadc1.Init.BoostMode				= DISABLE;
	hadc1.Init.OversamplingMode			= DISABLE;
	hadc1.Init.Oversampling.Ratio 					= 0;
	hadc1.Init.Oversampling.RightBitShift 			= 0;
	hadc1.Init.Oversampling.TriggeredMode 			= 0;
	hadc1.Init.Oversampling.OversamplingStopReset	= 0;
	
	if (HAL_ADC_Init(&hadc1) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}


	hadc2.Instance = ADC2;
	hadc2.Init.ClockPrescaler 			= ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc2.Init.Resolution 				= ADC_RESOLUTION_12B;
	hadc2.Init.ScanConvMode 			= DISABLE;
	hadc2.Init.EOCSelection 			= ADC_EOC_SINGLE_CONV;
	hadc2.Init.LowPowerAutoWait 		= DISABLE;	
	hadc2.Init.ContinuousConvMode 		= DISABLE;
	hadc2.Init.NbrOfConversion 			= 1;
	hadc2.Init.DiscontinuousConvMode 	= DISABLE;
	hadc2.Init.NbrOfDiscConversion 		= 1;
	hadc2.Init.ExternalTrigConv 		= ADC_SOFTWARE_START;
	hadc2.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc2.Init.ConversionDataManagement	= ADC_CONVERSIONDATA_DR;
	hadc2.Init.Overrun					= ADC_OVR_DATA_OVERWRITTEN;
	hadc2.Init.LeftBitShift				= ADC_LEFTBITSHIFT_NONE;
	hadc2.Init.BoostMode				= DISABLE;
	hadc2.Init.OversamplingMode			= DISABLE;
	hadc2.Init.Oversampling.Ratio 					= 0;
	hadc2.Init.Oversampling.RightBitShift 			= 0;
	hadc2.Init.Oversampling.TriggeredMode 			= 0;
	hadc2.Init.Oversampling.OversamplingStopReset	= 0;
	
	if (HAL_ADC_Init(&hadc2) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

	hadc3.Instance = ADC3;
	hadc3.Init.ClockPrescaler 			= ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc3.Init.Resolution 				= ADC_RESOLUTION_12B;
	hadc3.Init.ScanConvMode 			= DISABLE;
	hadc3.Init.EOCSelection 			= ADC_EOC_SINGLE_CONV;
	hadc3.Init.LowPowerAutoWait 		= DISABLE;	
	hadc3.Init.ContinuousConvMode 		= DISABLE;
	hadc3.Init.NbrOfConversion 			= 1;
	hadc3.Init.DiscontinuousConvMode 	= DISABLE;
	hadc3.Init.NbrOfDiscConversion 		= 1;
	hadc3.Init.ExternalTrigConv 		= ADC_SOFTWARE_START;
	hadc3.Init.ExternalTrigConvEdge 	= ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc3.Init.ConversionDataManagement	= ADC_CONVERSIONDATA_DR;
	hadc3.Init.Overrun					= ADC_OVR_DATA_OVERWRITTEN;
	hadc3.Init.LeftBitShift				= ADC_LEFTBITSHIFT_NONE;
	hadc3.Init.BoostMode				= DISABLE;
	hadc3.Init.OversamplingMode			= DISABLE;
	hadc3.Init.Oversampling.Ratio 					= 0;
	hadc3.Init.Oversampling.RightBitShift 			= 0;
	hadc3.Init.Oversampling.TriggeredMode 			= 0;
	hadc3.Init.Oversampling.OversamplingStopReset	= 0;
	
	if (HAL_ADC_Init(&hadc3) != HAL_OK)
	{
		_Error_Handler(__FILE__, __LINE__);
	}

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
return 0;
	ASSERT( channel < adc_channel_count );

    uint32_t internal_channel = adc_channels[channel].channel;

	ADC_ChannelConfTypeDef sConfig;
	sConfig.Channel 				= internal_channel;
	sConfig.Rank 					= ADC_REGULAR_RANK_1;
	sConfig.SamplingTime 			= ADC_SAMPLETIME_64CYCLES_5;
	sConfig.SingleDiff 				= ADC_SINGLE_ENDED;
	sConfig.OffsetNumber 			= ADC_OFFSET_NONE;
	sConfig.Offset 					= 0;
	sConfig.OffsetRightShift 		= DISABLE;
	sConfig.OffsetSignedSaturation 	= DISABLE;

	ADC_HandleTypeDef *adc = adc_channels[channel].adc;

	if (HAL_ADC_ConfigChannel( adc, &sConfig ) != HAL_OK)
	{
		_Error_Handler( __FILE__, __LINE__ );
	}

	HAL_ADC_Start( adc );        
    HAL_ADC_PollForConversion( adc, 5 );

    uint32_t value = HAL_ADC_GetValue( adc );
 	
    return (int16_t)value;
}

void adc_v_shutdown( void ){

	__HAL_RCC_ADC12_CLK_DISABLE();
	__HAL_RCC_ADC3_CLK_DISABLE();
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


