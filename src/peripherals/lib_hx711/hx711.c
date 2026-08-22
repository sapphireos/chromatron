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

#include "sapphire.h"

#include "hx711.h"

#ifdef ESP32

static int32_t load_cell;
static int32_t offset_cal;

KV_SECTION_META kv_meta_t hx711_kv[] = {
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, 0,                        0, "hx711_enable" },

    { CATBUS_TYPE_INT32,      0, KV_FLAGS_READ_ONLY, &load_cell,             0, "hx711_load_cell" },
    { CATBUS_TYPE_INT32,      0, KV_FLAGS_PERSIST,   &offset_cal,            0, "hx711_offset_cal" },
};
	
static uint8_t io_pd_clk;
static uint8_t io_dout;

static uint8_t clock_pulse( void ){

    io_v_digital_write( io_pd_clk, 1 );
    _delay_us( 1 );
    io_v_digital_write( io_pd_clk, 0 );
    _delay_us( 1 );
    
    // data is valid on falling edge:
    bool data = io_b_digital_read( io_dout );

    if( data ){

        return 1;
    }

    return 0;
}

#define N_PULSES_INPUT_A_GAIN_128   25
#define N_PULSES_INPUT_B_GAIN_32    26
#define N_PULSES_INPUT_A_GAIN_64    27

static int32_t read_data( uint8_t setting ){

    ASSERT( ( setting >= N_PULSES_INPUT_A_GAIN_128 ) && ( setting <= N_PULSES_INPUT_A_GAIN_64 ) );

    int32_t data = 0;  

    setting -= 24;

    // clock out 24 bits:
    for( uint8_t i = 0; i < 24; i++ ){

        data |= clock_pulse();
        data <<= 1;
    }

    // apply setting bits.  what a strange interface.
    while( setting > 0 ){

        clock_pulse();
        setting--;
    }

    if( data & 0x800000 ){
        // sign extend to 32 bits

        data |= 0xff000000;
    }

    return data; // data format is raw 24 bit ADC value
}


static uint32_t median_filter[5];
static uint8_t median_index;

PT_THREAD( hx711_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    TMR_WAIT( pt, 200 );

    // DOUT is high while data is NOT ready:
    THREAD_WAIT_WHILE( pt, io_b_digital_read( io_dout ) );    

    // set initial value
    load_cell = read_data( N_PULSES_INPUT_A_GAIN_64 ) - offset_cal;

    if( load_cell < 0 ){

        load_cell = 0;
    }

    while(1){

        TMR_WAIT( pt, 100 );

        // DOUT is high while data is NOT ready:
        THREAD_WAIT_WHILE( pt, io_b_digital_read( io_dout ) );

        // data is ready

        int32_t data = read_data( N_PULSES_INPUT_A_GAIN_64 );

        // log_v_debug_P( PSTR("%d -> 0x%x"), data, data );

        int32_t offset_data = data - offset_cal;

        // clamp data to >= 0
        // the actual sensor can measure negative, but
        // we want to calibrate so that around 0 is 
        // no load anyway
        if( offset_data < 0 ){

            offset_data = 0;
        }

        // update median filter:
        median_filter[median_index] = offset_data;
        median_index++;
        median_index %= cnt_of_array(median_filter);

        // sort filter
        util_v_bubble_sort_u32( median_filter, cnt_of_array(median_filter) );

        uint32_t input_data = median_filter[(cnt_of_array(median_filter) - 1) / 2]; // select middle item from filter

        // run EWMA filter
        load_cell = util_u32_ewma( input_data, load_cell, 32 );
    }


PT_END( pt );	
}


void hx711_v_set_io( uint8_t pd_clk, uint8_t dout ){

    io_pd_clk = pd_clk;
    io_dout = dout;    

    io_v_set_mode( io_pd_clk, IO_MODE_OUTPUT );
    io_v_digital_write( io_pd_clk, 0 ); // clock idles low
    io_v_set_mode( io_dout, IO_MODE_INPUT );
}

void hx711_v_init( void ){

    if( !kv_b_get_boolean( __KV__hx711_enable ) ){

        return;
    }

    log_v_info_P(PSTR("HX711 enabled") );

    hx711_v_set_io( IO_PIN_17_TX, IO_PIN_16_RX );

    thread_t_create( hx711_thread,
                     PSTR("hx711"),
                     0,
                     0 );
}

#endif
