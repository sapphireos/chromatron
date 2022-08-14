/*
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
 */


#include "system.h"
#include "keyvalue.h"
#include "adc.h"
#include "timers.h"
#include "hal_adc.h"
#include "hal_io.h"

/*

The XMEGA has a pretty complex and full featured ADC.
For now though, we're just going to implement simple,
synchronous conversions.

*/

static int8_t adc_kv_handler(
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
    { CATBUS_TYPE_UINT16,      0, KV_FLAGS_READ_ONLY, 0, adc_kv_handler,   "vcc" },
};




static int16_t _adc_i16_internal_read( uint8_t channel ){

    uint8_t samples = 4;

    if( channel <= ADC_CHANNEL_EXT ){

        if( channel == ADC_CHANNEL_VSUPPLY ){

            samples = 16;
            adc_v_set_ref( ADC_VREF_INT1V );
        }
        else{

            adc_v_set_ref( ADC_VREF_INTVCC_DIV1V6 );
        }

        ADCA.CH0.CTRL = ADC_CH_INPUTMODE_DIFF_gc;

        switch( channel ){
            case IO_PIN_5_ADC1:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN0_gc;
                break;

            case IO_PIN_4_ADC0:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN1_gc;
                break;

            case ADC_CHANNEL_VSUPPLY:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN3_gc;
                break;

            case IO_PIN_6_DAC0:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN10_gc;
                break;

            case IO_PIN_7_DAC1:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN11_gc;
                break;

            default:
                break;
        }

        // set negative input
        ADCA.CH0.MUXCTRL |= ADC_CH_MUXNEG_GND_MODE3_gc;
    }
    // internal channels
    else{

        adc_v_set_ref( ADC_VREF_INT1V );

        ADCA.CH0.CTRL = ADC_CH_INPUTMODE_INTERNAL_gc;

        switch( channel ){
            case ADC_CHANNEL_BG:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_BANDGAP_gc;
                break;

            case ADC_CHANNEL_VCC:
                samples = 16;
                ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_SCALEDVCC_gc;
                break;

            case ADC_CHANNEL_TEMP:
                ADCA.CH0.MUXCTRL = ADC_CH_MUXINT_TEMP_gc;
                break;

            default:
                break;
        }
    }

    int16_t accumulator = 0;

    for( uint8_t i = 0; i < samples; i++ ){

        // clear interrupt flag
        ADCA.CH0.INTFLAGS = ADC_CH_CHIF_bm;

        // start conversion
        ADCA.CH0.CTRL |= ADC_CH_START_bm;

        BUSY_WAIT( ADCA.CH0.INTFLAGS == 0 );

        accumulator += ADCA.CH0.RES;
    }

    accumulator /= samples;

    return (int16_t)accumulator;
}

void adc_v_init( void ){

    ADCA.CTRLB = ADC_RESOLUTION_12BIT_gc | ADC_CONMODE_bm; // set 12 bit, right adjusted, signed mode
    adc_v_set_ref( ADC_VREF_INT1V ); // select 1.0V reference and turn on band gap

    // ADC clock needs to be between 100khz and 125 khz
    // for internal signals
    ADCA.PRESCALER = ADC_PRESCALER_DIV256_gc;
    // 32MHz / 256 = 125 kHz

    // read linearity calibration from signature row
    NVM.CMD   = NVM_CMD_READ_CALIB_ROW_gc;
    ADCA.CALL = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL0));
    ADCA.CALH = pgm_read_byte(offsetof(NVM_PROD_SIGNATURES_t, ADCACAL1));
    NVM.CMD   = 0;

    ADCA.CTRLA = ADC_ENABLE_bm; // enable ADC

    // set vsupply pin to input
    ADC_VSUPPLY_PORT.DIRCLR = ( 1 << ADC_VSUPPLY_PIN );
}

void adc_v_shutdown( void ){

    ADCA.CTRLA = 0;
}

void adc_v_set_ref( uint8_t ref ){

    ADCA.REFCTRL = ref | ADC_BANDGAP_bm;    
} 

uint16_t adc_u16_read_raw( uint8_t channel ){

    int16_t res = _adc_i16_internal_read( channel );

    // clamp to 0, negative values are invalid
    if( res < 0 ){

        res = 0;
    }

    return res;
}

uint16_t adc_u16_read_supply_voltage( void ){

    uint32_t mv = adc_u16_convert_to_millivolts( adc_u16_read_raw( ADC_CHANNEL_VSUPPLY ) );

    // divider ratio is 49.9 + 2.2) / 2.2 = 23.682

    return ( mv * 23682 ) / 1000;
}

uint16_t adc_u16_read_vcc( void ){

    uint16_t mv = adc_u16_convert_to_millivolts( adc_u16_read_raw( ADC_CHANNEL_VCC ) * 10 );

    return mv;
}

uint16_t adc_u16_convert_to_millivolts( uint16_t raw_value ){

	uint32_t millivolts = 0;

    if( ( ADCA.REFCTRL & ADC_REFSEL_gm ) == ADC_VREF_INT1V ){

    	millivolts = (uint32_t)raw_value * 1000;
        millivolts /= 2048;
    }
    else if( ( ADCA.REFCTRL & ADC_REFSEL_gm ) == ADC_VREF_INTVCC_DIV1V6 ){

        // uint16_t mv = adc_u16_read_vcc();

        millivolts = (uint32_t)raw_value * (3300 / 1.6);
        millivolts /= 2048;
    }

	return (uint16_t)millivolts;
}
