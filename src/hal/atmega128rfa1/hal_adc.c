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

    // get ADMUX value and mask off channel bits
    uint8_t temp = ADMUX & 0b11000000;
    
    if( channel < 8 ){
        
        temp |= channel;
        
        // enable ADC, disable interrupt, auto trigger disabled
        // set prescaler to / 8
        ADCSRA = ( 1 << ADEN ) | ( 0 << ADATE ) | ( 0 << ADIE ) | 
                 ( 0 << ADPS2 ) | ( 1 << ADPS1 ) | ( 1 << ADPS0 );
    }
    else if( channel == ADC_CHANNEL_1V2_BG ){
        
        temp |= 0b00011110;
        
        // enable ADC, disable interrupt, auto trigger disabled
        // set prescaler to / 8
        ADCSRA = ( 1 << ADEN ) | ( 0 << ADATE ) | ( 0 << ADIE ) | 
                 ( 0 << ADPS2 ) | ( 1 << ADPS1 ) | ( 1 << ADPS0 );
    }
    else if( channel == ADC_CHANNEL_0V0_AVSS ){
        
        temp |= 0b00011111;
        
        // enable ADC, disable interrupt, auto trigger disabled
        // set prescaler to / 8
        ADCSRA = ( 1 << ADEN ) | ( 0 << ADATE ) | ( 0 << ADIE ) | 
                 ( 0 << ADPS2 ) | ( 1 << ADPS1 ) | ( 1 << ADPS0 );
    }
    else if( channel == ADC_CHANNEL_TEMP ){
        
        temp |= 0b00001001;
        
        // enable ADC, disable interrupt, auto trigger disabled
        // set prescaler to / 32
        ADCSRA = ( 1 << ADEN ) | ( 0 << ADATE ) | ( 0 << ADIE ) | 
                 ( 1 << ADPS2 ) | ( 0 << ADPS1 ) | ( 1 << ADPS0 );
    }
    else{
    
        ASSERT( FALSE );
    }
    
    // check if we need to set ADMUX5
    if( channel == ADC_CHANNEL_TEMP ){
        
        ADCSRB |= ( 1 << MUX5 );
    }
    else{
        
        ADCSRB &= ~( 1 << MUX5 );
    }
    
    // apply channel selection
    ADMUX = temp;
    
    // start conversion
    ADCSRA |= ( 1 << ADSC );
    
    // wait until conversion is complete
    while( !( ADCSRA & ( 1 << ADIF ) ) );
    
    // clear ADIF bit
    ADCSRA |= ( 1 << ADIF );
    
    // return result
    return ADC;
}

void adc_v_init( void ){

    // set reference to 1.6v internal
    // channel 0, right adjusted
    ADMUX = ( 1 << REFS1 ) | ( 1 << REFS0 );
    
    // set tracking time to 4 cycles and maximum start up time
    ADCSRC = ( 1 << ADTHT1 ) | ( 1 << ADTHT0 ) | ( 1 << ADSUT4 ) |
             ( 1 << ADSUT3 ) | ( 1 << ADSUT2 ) | ( 1 << ADSUT1 ) | ( 1 << ADSUT0 );
    
    /*
    Timing notes:
    
    Fcpu = 16,000,000
    ADC prescaler = /8
    Fadc = 2,000,000
    
    Tracking time set to 4 cycles (2 microseconds)
    
    Total conversion time is 15 ADC cycles, 120 CPU cycles
    
    
    Clock for temperature sensor measurement: Fadc < 500,000
    Prescaler / 32
    */
}

void adc_v_shutdown( void ){

    // ADCA.CTRLA = 0;
}

void adc_v_set_ref( uint8_t ref ){

    // ADCA.REFCTRL = ref | ADC_BANDGAP_bm;    
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

    uint32_t v = adc_u16_read_raw( ADC_CHANNEL_VSUPPLY );
    
    /*
    Conversion:
    
    pin voltage = ( ADC * Vref ) / 1024
    
    supply voltage = pin voltage * 11
    
    */
    
    return ( ( (float)v * 1.6 ) / 1024 ) * 11;
}

uint16_t adc_u16_read_vcc( void ){

    uint16_t mv = adc_u16_convert_to_millivolts( adc_u16_read_raw( ADC_CHANNEL_VCC ) * 10 );

    return mv;
}

uint16_t adc_u16_convert_to_millivolts( uint16_t raw_value ){

	// this calculation is for the 1.6v reference

    uint32_t millivolts;
    
    millivolts = (uint32_t)raw_value * 1600; 
    millivolts /= 1023;

    return (uint16_t)millivolts;
}
