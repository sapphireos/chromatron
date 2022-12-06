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


#include "sapphire.h"

#include "flash_fs.h"
#include "hal_boards.h"

#include "battery.h"
#include "bq25895.h"

#include "thermal.h"


#ifdef ESP32
static int8_t case_temp = -127;
static int8_t ambient_temp = -127;
static int16_t case_temp_state;
static int16_t ambient_temp_state;

static bool fan_on;

#define FAN_IO IO_PIN_19_MISO
#define BOOST_IO IO_PIN_4_A5

#endif

KV_SECTION_OPT kv_meta_t thermal_info_kv[] = {
    #ifdef ESP32
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &case_temp,                  0,  "batt_case_temp" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &ambient_temp,               0,  "batt_ambient_temp" },

    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_PERSIST,    0,                           0,  "batt_enable_fan" },
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &fan_on,                     0,  "batt_fan_on" },
    #endif

};


#define THERM_FILTER 32


PT_THREAD( aux_temp_thread( pt_t *pt, void *state ) );
PT_THREAD( fan_thread( pt_t *pt, void *state ) );


void thermal_v_init( void ){

    #if defined(ESP32)
    
    if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

        kv_v_add_db_info( thermal_info_kv, sizeof(thermal_info_kv) );

        thread_t_create( aux_temp_thread,
                     PSTR("aux_temp"),
                     0,
                     0 );

        if( kv_b_get_boolean( __KV__batt_enable_fan ) ){

            // FAN init IO to OFF
            io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
            io_v_digital_write( ELITE_FAN_IO, 0 );
            
            thread_t_create( fan_thread,
                             PSTR("fan_control"),
                             0,
                             0 );
        }
    }

    #endif
}


int8_t thermal_i8_get_case_temp( void ){

    #ifdef ESP32
    return case_temp;
    #else
    return 0;
    #endif
}

int8_t thermal_i8_get_ambient_temp( void ){

    #ifdef ESP32
    return ambient_temp;
    #else
    return 0;
    #endif
}


// percent * 10
// IE, the first value is 41.8%
static const uint16_t aux_temp_table[] = {
418 , // -20C
415 ,
411 ,
407 ,
403 ,
399 ,
395 ,
390 ,
386 ,
381 ,
377 ,
372 ,
367 ,
363 ,
358 ,
353 ,
348 ,
343 ,
338 ,
333 ,
328 , // 0C
322 ,
317 ,
312 ,
307 ,
301 ,
296 ,
291 ,
285 ,
280 ,
275 ,
269 ,
264 ,
259 ,
254 ,
248 ,
243 ,
238 ,
233 ,
228 ,
223 ,
218 ,
213 ,
209 ,
204 ,
199 , // 25C
195 ,
190 ,
186 ,
181 ,
177 ,
173 ,
168 ,
164 ,
160 ,
156 ,
153 ,
149 ,
145 ,
141 ,
138 ,
134 ,
131 ,
128 ,
124 ,
121 ,
118 ,
115 ,
112 ,
109 ,
107 ,
104 ,
101 ,
99  ,
96  ,
94  ,
91  ,
89  ,
87  ,
84  ,
82  ,
80  ,
78  ,
76  ,
74  ,
72  ,
70  ,
69  ,
67  ,
65  ,
63  ,
62  ,
60  ,
59  ,
57  ,
56  ,
54  ,
53  ,
52  ,
50  ,
49  ,
48  ,
47  ,
46  ,
45  ,
43  , // 85C
};

// percent * 10, using table 2
int8_t _thermal_i8_calc_temp2( uint16_t percent ){

    for( uint8_t i = 0; i < cnt_of_array(aux_temp_table) - 1; i++ ){

        if( ( percent <= aux_temp_table[i] ) && ( percent >= aux_temp_table[i + 1] ) ){

            return (int16_t)i - 20;
        }
    }

    return -20;
}

#if defined(ESP32)

PT_THREAD( aux_temp_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    if( ffs_u8_read_board_type() != BOARD_TYPE_ELITE ){

        THREAD_EXIT( pt );
    }

    io_v_set_mode( ELITE_CASE_ADC_IO, IO_MODE_INPUT );      
    io_v_set_mode( ELITE_AMBIENT_ADC_IO, IO_MODE_INPUT );      

    while(1){

        TMR_WAIT( pt, 1000 );

        uint16_t sys_volts = bq25895_u16_get_sys_voltage();

        if( ( sys_volts <= 2500 ) ||
            ( sys_volts > 5000 ) ){

            // avoid divide by zero error

            // also, avoid nonsensical values
            // if system voltage is below 2.5V, something is very wrong anyway

            continue;
        }

        uint32_t case_adc = adc_u16_read_mv( ELITE_CASE_ADC_IO );
        uint32_t ambient_adc = adc_u16_read_mv( ELITE_AMBIENT_ADC_IO );

        int8_t temp = _thermal_i8_calc_temp2( ( case_adc * 1000 ) / sys_volts );

        if( case_temp != -127 ){

            case_temp_state = util_i16_ewma( temp * 256, case_temp_state, THERM_FILTER );    
            case_temp = case_temp_state / 256;
        }
        else{

            case_temp_state = temp * 256;
            case_temp = temp;
        }
        
        temp = _thermal_i8_calc_temp2( ( ambient_adc * 1000 ) / sys_volts );

        if( ambient_temp != -127 ){

            ambient_temp_state = util_i16_ewma( temp * 256, ambient_temp_state, THERM_FILTER );    
            ambient_temp = ambient_temp_state / 256;
        }
        else{

            ambient_temp_state = temp * 256;
            ambient_temp = temp;
        }
    }

PT_END( pt );
}


PT_THREAD( fan_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    if( ffs_u8_read_board_type() != BOARD_TYPE_ELITE ){

        THREAD_EXIT( pt );
    }

    // BOOST
    io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( ELITE_BOOST_IO, 1 );

    // FAN
    // turn on for 5 seconds for easy testing
    io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( ELITE_FAN_IO, 1 );

    TMR_WAIT( pt, 5000 );
    // turn fan off
    io_v_digital_write( ELITE_FAN_IO, 0 );

    fan_on = FALSE;


    while(1){
        
        while( sys_b_is_shutting_down() ){

            // ensure fan is off when shutting down.
            // if it is on, it can kick the battery controller back on as it winds down.

            io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
            io_v_digital_write( ELITE_FAN_IO, 0 );            

            fan_on = FALSE;

            TMR_WAIT( pt, 20 );
        }
        while( !fan_on && !sys_b_is_shutting_down() ){

            TMR_WAIT( pt, 100 );

            io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
            io_v_digital_write( ELITE_FAN_IO, 0 );

            if( ( batt_i8_get_batt_temp() >= 38 ) ||
                ( case_temp >= 55 ) ){

                fan_on = TRUE;
            }
        }

        while( fan_on && !sys_b_is_shutting_down() ){

            TMR_WAIT( pt, 100 );

            io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
            io_v_digital_write( ELITE_FAN_IO, 1 );

            if( ( batt_i8_get_batt_temp() <= 37 ) &&
                ( case_temp <= 52 ) ){

                fan_on = FALSE;
            }
        }
    }

PT_END( pt );
}

#endif