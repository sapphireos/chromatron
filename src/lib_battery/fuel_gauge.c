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


#include "battery.h"
#include "fuel_gauge.h"
#include "energy.h"
#include "graphics.h"

/*


Another take on this:

We really need a battery cycle counter.  This can just be mV recovered by charge.  Nothing fancy.

Basic SoC on voltage is usable.  Later we can compensate for load, but the cycle count is more valuable.


Some knowledge of the day/night cycle for solar charging would be useful.  This could come from light level, if available.

This module might not be the right place for that though.  The top level solar charge control loop should do this.


Might turn off the recorder, and just rewrite this module to be more streamlined.  Not sure the filters are useful either.

Just want cycle count and basic Soc!

Some kind of very basic lifetime logging would be neat.  Like, ever N cycles (10? 100?) record a data type in a 
fixed sized buffer.


To enhance the accuracy, we need to disable the charger while we take our voltage measurement during a charge cycle.
(We should also attempt to get a voltage drop estimate from pixels off to on at a given power level - maybe could 
do a calibration at assembly time, and maybe semi periodically).

Re-enable the charger after the measurement.  This should be driven externally by the overall solar charge control loop.
The fuel gauge does not need to be updated often.  10 minute intervals is a good start, and on some events such as changing
from charge to discharge or enabling/disabling the pixel array.



Solar system modes:

IDLE/RESET - this is an unknown state, the system has usually just started.
DISCHARGE_IDLE - battery discharging, system is running, pixels are OFF
DISCHARGE_PIXELS - above, but pixels are ON
CHARGE_DC - charging on DC wall power
CHARGE_SOLAR - charging on solar power
FULL_CHARGE - system is fully charged and is connected to a valid power source (DC plugged in, or solar is generating enough)





Fuel gauge notes:


Available input data points:

battery voltage
pixel power
is_charging status
time

battery temp (for SoC ambient correction) - optional for sure



Algorithm goal:

Accurately estimate state of charge based on pixel power.
Track battery cycle counts for lifetime estimation.
Track degradation of battery.


Some ideas:


Use averaged inputs to reduce noise, especially on the pixel power which can swing wildly 
frame to frame but should have DC average on a multi-minute basis.

The estimator only needs to update around once per minute, and when there is a dramatic change
in pixel power.

Therefore, the estimator can use inputs averaged over the previous 60 seconds.
The filtering could be a moving average, ewma (but watch for stability and resolution issues), etc.



Approx once per minute:

Record current averaged voltage and pixel power.
Integrate pixel power to track one-minute pixel energy usage.





Sources of truth:

Open circuit voltage indicates if fully charged.
Low voltage cutoff is 0% SoC.
Discharge rate at a given load.







*/

#define MODE_UNKNOWN        0
#define MODE_DISCHARGE      1 // discharging on battery power
#define MODE_CHARGE         2 // charging on wall power
#define MODE_FLOAT          3 // fully charged and running on VBUS
static uint8_t mode;


static uint8_t batt_soc = 50; // state of charge in percent
static uint16_t soc_state;
#define SOC_MAX_VOLTS   ( batt_u16_get_charge_voltage() - 100 )
#define SOC_MIN_VOLTS   ( batt_u16_get_discharge_voltage() )
#define SOC_FILTER      64

static uint32_t total_charge_cycles_percent; // in 0.01% SoC increments
// static uint16_t charge_cycle_start_volts;



static uint16_t filtered_5sec_batt_volts;
static uint16_t filtered_5sec_pix_power;
static int8_t filtered_5sec_batt_temp;

static uint16_t filtered_1min_batt_volts;
static uint16_t filtered_1min_pix_power;
static int8_t filtered_1min_batt_temp;

static uint16_t filtered_5min_batt_volts;
static uint16_t filtered_5min_pix_power;
static int8_t filtered_5min_batt_temp;


KV_SECTION_OPT kv_meta_t fuel_gauge_info_kv[] = {
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_soc,                    0,  "batt_soc" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_5sec_batt_volts,    0,  "batt_filtered_5sec_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_5sec_pix_power,     0,  "batt_filtered_5sec_pix_power" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY, &filtered_5sec_batt_temp,     0,  "batt_filtered_5sec_temp" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_1min_batt_volts,    0,  "batt_filtered_1min_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_1min_pix_power,     0,  "batt_filtered_1min_pix_power" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY, &filtered_1min_batt_temp,     0,  "batt_filtered_1min_temp" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_5min_batt_volts,    0,  "batt_filtered_5min_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_5min_pix_power,     0,  "batt_filtered_5min_pix_power" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY, &filtered_5min_batt_temp,     0,  "batt_filtered_5min_temp" },

    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_PERSIST | 
                              KV_FLAGS_READ_ONLY, &total_charge_cycles_percent, 0,  "batt_charge_cycles_percent" },
};



PT_THREAD( fuel_gauge_thread( pt_t *pt, void *state ) );



void fuel_v_init( void ){

    kv_v_add_db_info( fuel_gauge_info_kv, sizeof(fuel_gauge_info_kv) );

    thread_t_create( fuel_gauge_thread,
                     PSTR("batt_fuel_gauge"),
                     0,
                     0 );

}

uint8_t fuel_u8_get_soc( void ){

    return batt_soc;
}


// static uint16_t calc_raw_soc( uint16_t volts ){

//     uint16_t temp_soc = 0;

//     if( volts < LION_MIN_VOLTAGE ){

//         temp_soc = 0;
//     }
//     else if( volts > LION_MAX_VOLTAGE ){

//         temp_soc = 10000;
//     }
//     else{

//         temp_soc = util_u16_linear_interp( volts, LION_MIN_VOLTAGE, 0, LION_MAX_VOLTAGE, 10000 );
//     }

//     return temp_soc;    
// }

static uint8_t calc_batt_soc( uint16_t volts ){

    uint16_t temp_soc = 0;

    if( volts < SOC_MIN_VOLTS ){

        temp_soc = 0;
    }
    else if( volts > SOC_MAX_VOLTS ){

        temp_soc = 10000;
    }
    else{

        temp_soc = util_u16_linear_interp( volts, SOC_MIN_VOLTS, 0, SOC_MAX_VOLTS, 10000 );
    }

    if( soc_state == 0 ){

        soc_state = temp_soc;
    }
    else{

        soc_state = util_u16_ewma( temp_soc, soc_state, SOC_FILTER );
    }

    return soc_state / 100;
}


// 5 second moving averages
static uint16_t batt_volts_filter_state_5sec[5];
static uint16_t pix_power_filter_state_5sec[5];
static int8_t batt_temp_filter_state_5sec[5];
static uint8_t filter_index_5sec;

// 60 second moving averages
static uint16_t batt_volts_filter_state_1min[12];
static uint16_t pix_power_filter_state_1min[12];
static int8_t batt_temp_filter_state_1min[12];
static uint8_t filter_index_1min;

// 5 minute moving averages
static uint16_t batt_volts_filter_state_5min[5];
static uint16_t pix_power_filter_state_5min[5];
static int8_t batt_temp_filter_state_5min[5];
static uint8_t filter_index_5min;

static void reset_filters( void ){

    uint16_t batt_volts = batt_u16_get_batt_volts();
    uint16_t pix_power = gfx_u16_get_pixel_power_mw();
    int8_t batt_temp = batt_i8_get_batt_temp();

    for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_5sec); i++ ){

        batt_volts_filter_state_5sec[i] = batt_volts;
        pix_power_filter_state_5sec[i] = pix_power;
        batt_temp_filter_state_5sec[i] = batt_temp;
    }

    for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_1min); i++ ){

        batt_volts_filter_state_1min[i] = batt_volts;
        pix_power_filter_state_1min[i] = pix_power;
        batt_temp_filter_state_1min[i] = batt_temp;
    }

    for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_5min); i++ ){

        batt_volts_filter_state_5min[i] = batt_volts;
        pix_power_filter_state_5min[i] = pix_power;
        batt_temp_filter_state_5min[i] = batt_temp;
    }
}



static uint64_t discharge_power_pix_accumlator;


static fuel_gauge_data_t recorder_buffer[FFS_PAGE_DATA_SIZE / sizeof(fuel_gauge_data_t)];
static uint8_t recorder_buffer_size;

static uint16_t record_id;
static uint8_t previous_record_flags;

static void init_recorder( void ){

    file_t f = fs_f_open_P( PSTR("batt_recorder"), FS_MODE_CREATE_IF_NOT_FOUND | FS_MODE_WRITE_OVERWRITE );

    if( f < 0 ){

        goto end;
    }

    // search for record start

    fuel_gauge_record_start_t start;

    while( fs_i16_read( f, &start, sizeof(start) ) == sizeof(start) ){

        if( start.flags == FUEL_RECORD_TYPE_RECORD_START ){

            // log_v_debug_P( PSTR("start record found %d"), start.record_id );

            if( start.record_id > record_id ){

                record_id = start.record_id;
            }
        }
    }

    log_v_debug_P( PSTR("batt recorder ID: %d"), record_id );

    // pad file to page size
    while( ( fs_i32_get_size( f ) % FFS_PAGE_DATA_SIZE ) != 0 ){

        fuel_gauge_record_start_t blank;
        memset( &blank, 0, sizeof(blank) );

        fs_i16_write( f, &blank, sizeof(blank) );
    }

end:
    if( f > 0 ){

        fs_f_close( f );   
    }
}

static void flush_recorder_buffer( void ){

    file_t f = fs_f_open_P( PSTR("batt_recorder"), FS_MODE_WRITE_APPEND );

    if( f < 0 ){

        goto end;
    }
    
    if( fs_i32_get_size( f ) >= FUEL_MAX_DISCHARGE_FILE_SIZE ){
        
        fs_v_seek( f, 0 );
    }

    fs_i16_write( f, recorder_buffer, recorder_buffer_size * sizeof(fuel_gauge_data_t) );

end:
    if( f > 0 ){

        fs_f_close( f );   
    }

    recorder_buffer_size = 0;
}

static void insert_record( void *record ){

    memcpy( &recorder_buffer[recorder_buffer_size], record, sizeof(fuel_gauge_data_t) );

    recorder_buffer_size++;

    if( recorder_buffer_size >= cnt_of_array(recorder_buffer) ){

        flush_recorder_buffer();        
    }
}

static void record_data( void ){

    uint8_t flags = FUEL_RECORD_TYPE_IDLE;

    if( mode == MODE_DISCHARGE ){

        flags = FUEL_RECORD_TYPE_DISCHARGE;
    }
    else if( mode == MODE_CHARGE ){

        flags = FUEL_RECORD_TYPE_CHARGE;
    }


    // on mode change, increment data record ID
    if( previous_record_flags != flags ){

        record_id++;

        fuel_gauge_record_start_t start = {
            FUEL_RECORD_TYPE_RECORD_START,
            record_id,
            BATT_RECORDER_RATE / 10, // 10 second increments for rate field
        };

        insert_record( &start );

        // log_v_debug_P( PSTR("batt record start: %d"), record_id );
    }

    previous_record_flags = flags;


    // compress sensor data to 8 bits
    uint8_t compressed_volts = ( filtered_5min_batt_volts - 2500 ) / 8;
    // volts range is 2500 to 4548 mV in 8 mV steps

    uint8_t compressed_power = filtered_5min_pix_power / 64;
    // power range is 0 to 16384 mW in 64 mW steps

    int8_t compressed_temp = filtered_5min_batt_temp;
    // temp is already 8 bits so it is left as-is

    fuel_gauge_data_t data = {
        flags,
        compressed_volts,
        compressed_power,
        compressed_temp,
    }; 

    insert_record( &data );
}


PT_THREAD( fuel_gauge_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    static uint16_t counter;
    counter = 0;

    mode = MODE_UNKNOWN;

    // wait for battery controller to wake up
    THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

    // initialize filters
    reset_filters();


    init_recorder();

    thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );

    while(1){
            
        // run once per second
        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );        
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );


        // check for shutdown
        if( sys_b_is_shutting_down() ){

            // save settings and data

            flush_recorder_buffer();

            THREAD_EXIT( pt );    
        }

        // get sensor parameters
        uint16_t batt_volts = batt_u16_get_batt_volts();
        uint16_t pix_power = gfx_u16_get_pixel_power_mw();
        int8_t batt_temp = batt_i8_get_batt_temp();

        /****************************************
        5 second filter:
        ****************************************/

        // add to filter state
        batt_volts_filter_state_5sec[filter_index_5sec] = batt_volts;
        pix_power_filter_state_5sec[filter_index_5sec] = pix_power;
        batt_temp_filter_state_5sec[filter_index_5sec] = batt_temp;

        filter_index_5sec++;
        if( filter_index_5sec >= cnt_of_array(batt_volts_filter_state_5sec) ){

            filter_index_5sec = 0;
        }

        // compute moving average
        uint32_t volts_acc = 0;
        uint32_t power_acc = 0;
        int16_t temp_acc = 0;

        for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_5sec); i++ ){

            volts_acc += batt_volts_filter_state_5sec[i];
            power_acc += pix_power_filter_state_5sec[i];
            temp_acc += batt_temp_filter_state_5sec[i];
        }

        filtered_5sec_batt_volts = volts_acc / cnt_of_array(batt_volts_filter_state_5sec);
        filtered_5sec_pix_power = power_acc / cnt_of_array(pix_power_filter_state_5sec);
        filtered_5sec_batt_temp = temp_acc / cnt_of_array(batt_temp_filter_state_5sec);



        /****************************************
        60 second filter:
        ****************************************/

        // integrate the 5sec filter data

        // add to filter state
        batt_volts_filter_state_1min[filter_index_1min] = filtered_5sec_batt_volts;
        pix_power_filter_state_1min[filter_index_1min] = filtered_5sec_pix_power;
        batt_temp_filter_state_1min[filter_index_1min] = filtered_5sec_batt_temp;

        filter_index_1min++;
        if( filter_index_1min >= cnt_of_array(batt_volts_filter_state_1min) ){

            filter_index_1min = 0;
        }

        // compute moving average
        volts_acc = 0;
        power_acc = 0;
        temp_acc = 0;

        for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_1min); i++ ){

            volts_acc += batt_volts_filter_state_1min[i];
            power_acc += pix_power_filter_state_1min[i];
            temp_acc += batt_temp_filter_state_1min[i];
        }

        filtered_1min_batt_volts = volts_acc / cnt_of_array(batt_volts_filter_state_1min);
        filtered_1min_pix_power = power_acc / cnt_of_array(pix_power_filter_state_1min);
        filtered_1min_batt_temp = temp_acc / cnt_of_array(batt_temp_filter_state_1min);



        
        /****************************************
        5 minute filter:
        ****************************************/

        // integrate the 60sec filter data

        // add to filter state
        batt_volts_filter_state_5min[filter_index_5min] = filtered_1min_batt_volts;
        pix_power_filter_state_5min[filter_index_5min] = filtered_1min_pix_power;
        batt_temp_filter_state_5min[filter_index_5min] = filtered_1min_batt_temp;

        filter_index_5min++;
        if( filter_index_5min >= cnt_of_array(batt_volts_filter_state_5min) ){

            filter_index_5min = 0;
        }

        // compute moving average
        volts_acc = 0;
        power_acc = 0;
        temp_acc = 0;

        for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state_5min); i++ ){

            volts_acc += batt_volts_filter_state_5min[i];
            power_acc += pix_power_filter_state_5min[i];
            temp_acc += batt_temp_filter_state_5min[i];
        }

        filtered_5min_batt_volts = volts_acc / cnt_of_array(batt_volts_filter_state_5min);
        filtered_5min_pix_power = power_acc / cnt_of_array(pix_power_filter_state_5min);
        filtered_5min_batt_temp = temp_acc / cnt_of_array(batt_temp_filter_state_5min);




        // basic SoC: just from batt volts:
        batt_soc = calc_batt_soc( filtered_1min_batt_volts );


        uint8_t prev_mode = mode;

        // every 10 seconds
        if( ( counter % 10 ) == 0 ){

            // get charge state
            bool is_charging = batt_b_is_charging();
            bool is_wall_power = batt_b_is_wall_power();

            if( is_charging ){

                mode = MODE_CHARGE;
            }
            else if( is_wall_power ){

                mode = MODE_FLOAT;
            }
            else{

                mode = MODE_DISCHARGE;
            }


            if( prev_mode != mode ){

                // mode change
                discharge_power_pix_accumlator = 0;

                log_v_debug_P( PSTR("mode switch: %d -> %d"), prev_mode, mode );
            }
        }

        // every 5 minutes
        if( ( counter % BATT_RECORDER_RATE ) == 0 ){

            if( mode != MODE_UNKNOWN ){

                record_data();    
            }
        }


        // every 1 second:        
        if( mode == MODE_DISCHARGE ){

            discharge_power_pix_accumlator += filtered_1min_pix_power;
        }








        counter++;
    }


    #if 0
    

        // update battery SOC
        if( ( soc_counter % ( 1000 / BUTTON_CHECK_TIMING ) == 0 ) ){

            uint16_t temp_batt_volts = batt_u16_get_batt_volts();
            batt_soc = calc_batt_soc( temp_batt_volts );

            // check if switching into a charge cycle:
            if( batt_b_is_charging() && ( charge_cycle_start_volts == 0 ) ){

                // record previous voltage, which will not be affected
                // by charge current
                charge_cycle_start_volts = temp_batt_volts;

                // !!! charge cycle stuff still needs some work.

                // log_v_debug_P( PSTR("Charge cycle start: %u mV %u mA"), charge_cycle_start_volts, batt_charge_current );
            }


            // check if switching into a discharge cycle:
            if( !batt_b_is_charging() && ( charge_cycle_start_volts != 0 ) ){

                // cycle end voltage is current batt_volts

                // verify that a start voltage was recorded.
                // also sanity check that the end voltage is higher than the start:
                if( charge_cycle_start_volts < temp_batt_volts ){

                    // calculate start and end SoC
                    // these values are in 0.01% increments (0 to 10000)
                    uint16_t start_soc = calc_raw_soc( charge_cycle_start_volts );
                    uint16_t end_soc = calc_raw_soc( temp_batt_volts );

                    uint16_t recovered_soc = end_soc - start_soc;

                    // if we have recovered at least 2% SoC, we can record the cycle:
                    if( recovered_soc >= 200 ){

                        log_v_debug_P( PSTR("Charge cycle complete: %u mv %u%% recovered"), temp_batt_volts, recovered_soc / 100 );

                        // update cycle totalizer
                        total_charge_cycles_percent += recovered_soc;

                        // kv_i8_persist( __KV__batt_charge_cycles_percent );
                    }
                }

                // reset cycle
                charge_cycle_start_volts = 0;
            }   
        }

        soc_counter++;


    #endif


PT_END( pt );
}