/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include <driver/adc.h>

#ifndef BOOTLOADER
#include <esp_adc_cal.h>

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

#define V_REF 1100

static esp_adc_cal_characteristics_t characteristics;

PT_THREAD( hal_adc_thread( pt_t *pt, void *state ) );

#endif

void adc_v_init( void ){

    #ifndef BOOTLOADER

    adc1_config_width( ADC_WIDTH_BIT_12 );

    esp_adc_cal_characterize( ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, V_REF, &characteristics );
	
	thread_t_create( hal_adc_thread,
                     PSTR("hal_adc"),
                     0,
                     0 );

    #endif
}

PT_THREAD( hal_adc_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    

PT_END( pt );
}

static int16_t _adc_i16_internal_read( uint8_t channel ){

    uint8_t adc_channel = 0;

    // convert IO channel to internal
    if( channel == IO_PIN_36_A4 ){

        adc_channel = ADC1_CHANNEL_0;
    }
    else if( channel == ADC_CHANNEL_VSUPPLY ){

        adc_channel = ADC1_CHANNEL_7;
    }
    else{

        return 0;
    }

    adc1_config_channel_atten( adc_channel, ADC_ATTEN_DB_11 );

	return adc1_get_raw( adc_channel );
}

void adc_v_shutdown( void ){

}

void adc_v_set_ref( uint8_t ref ){


} 

uint16_t adc_u16_read_raw( uint8_t channel ){

    return _adc_i16_internal_read( channel );
}

uint16_t adc_u16_read_supply_voltage( void ){

    uint16_t mv = adc_u16_read_mv( ADC_CHANNEL_VSUPPLY );

    return mv * 2;
}

uint16_t adc_u16_read_vcc( void ){

    return 0;
}

uint16_t adc_u16_convert_to_millivolts( uint16_t raw_value ){

    #ifndef BOOTLOADER

    return esp_adc_cal_raw_to_voltage( raw_value, &characteristics );

    #else

    return 0;

    #endif
}


