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












*/


static uint8_t batt_soc = 50; // state of charge in percent
static uint16_t soc_state;
#define SOC_MAX_VOLTS   ( batt_u16_get_charge_voltage() - 100 )
#define SOC_MIN_VOLTS   ( batt_u16_get_discharge_voltage() )
#define SOC_FILTER      64

static uint32_t total_charge_cycles_percent; // in 0.01% SoC increments
static uint16_t charge_cycle_start_volts;



static uint16_t filtered_batt_volts;
static uint16_t filtered_pix_power;


KV_SECTION_META kv_meta_t fuel_gauge_info_kv[] = {
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_soc,                    0,  "batt_soc" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_batt_volts,         0,  "batt_volts_filtered" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY, &filtered_pix_power,          0,  "batt_pix_power_filtered" },


    { CATBUS_TYPE_UINT32, 0, KV_FLAGS_PERSIST | KV_FLAGS_READ_ONLY,    &total_charge_cycles_percent, 0,                "batt_charge_cycles_percent" },
};



PT_THREAD( fuel_gauge_thread( pt_t *pt, void *state ) );



void fuel_v_init( void ){

    thread_t_create( fuel_gauge_thread,
                     PSTR("batt_fuel_gauge"),
                     0,
                     0 );

}

uint8_t fuel_u8_get_soc( void ){

    return batt_soc;
}


static uint16_t calc_raw_soc( uint16_t volts ){

    uint16_t temp_soc = 0;

    if( volts < LION_MIN_VOLTAGE ){

        temp_soc = 0;
    }
    else if( volts > LION_MAX_VOLTAGE ){

        temp_soc = 10000;
    }
    else{

        temp_soc = util_u16_linear_interp( volts, LION_MIN_VOLTAGE, 0, LION_MAX_VOLTAGE, 10000 );
    }

    return temp_soc;    
}

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



// 60 second moving averages
static uint16_t batt_volts_filter_state[60];
static uint16_t pix_power_filter_state[60];
static uint8_t filter_index;

static void reset_filters( void ){

    uint16_t batt_volts = batt_u16_get_batt_volts();
    uint16_t pix_power = gfx_u16_get_pixel_power_mw();

    for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state); i++ ){

        batt_volts_filter_state[i] = batt_volts;
        pix_power_filter_state[i] = pix_power;
    }
}


PT_THREAD( fuel_gauge_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // wait for battery controller to wake up
    THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

    // initialize filters
    reset_filters();

    thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );

    while(1){
            
        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );        
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !sys_b_is_shutting_down() );

        if( sys_b_is_shutting_down() ){

            // save settings and data


            THREAD_EXIT( pt );    
        }


        // get sensor parameters
        uint16_t batt_volts = batt_u16_get_batt_volts();
        uint16_t pix_power = gfx_u16_get_pixel_power_mw();

        // add to filter state
        batt_volts_filter_state[filter_index] = batt_volts;
        pix_power_filter_state[filter_index] = pix_power;

        filter_index++;
        if( filter_index >= cnt_of_array(batt_volts_filter_state) ){

            filter_index = 0;
        }

        // compute moving average
        uint32_t volts_acc = 0;
        uint32_t power_acc = 0;

        for( uint8_t i = 0; i < cnt_of_array(batt_volts_filter_state); i++ ){

            volts_acc += batt_volts_filter_state[i];
            power_acc += pix_power_filter_state[i];
        }

        filtered_batt_volts = volts_acc / cnt_of_array(batt_volts_filter_state);
        filtered_pix_power = power_acc / cnt_of_array(pix_power_filter_state);




        // basic SoC: just from batt volts:
        batt_soc = calc_batt_soc( filtered_batt_volts );
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