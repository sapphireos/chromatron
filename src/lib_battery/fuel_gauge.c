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



static uint8_t batt_soc = 50; // state of charge in percent
static uint16_t soc_state;
#define SOC_MAX_VOLTS   ( batt_u16_get_charge_voltage() - 100 )
#define SOC_MIN_VOLTS   ( batt_u16_get_discharge_voltage() )
#define SOC_FILTER      64

static uint32_t total_charge_cycles_percent; // in 0.01% SoC increments
static uint16_t charge_cycle_start_volts;


KV_SECTION_META kv_meta_t fuel_gauge_info_kv[] = {
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_soc,                    0,  "batt_soc" },

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




PT_THREAD( fuel_gauge_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

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


    while(1){
        
        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}