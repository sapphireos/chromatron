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

#include "mppt.h"
#include "bq25895.h"


static bool enable_mppt;
static uint16_t mppt_current_vindpm;
static uint8_t mppt_index;
static uint32_t mppt_time;
static bool mppt_done;

#define MPPT_BINS ( ( BQ25895_MAX_MPPT_VINDPM - BQ25895_MIN_MPPT_VINDPM ) / BQ25895_MPPT_VINDPM_STEP )
static uint16_t mppt_bins[MPPT_BINS];

KV_SECTION_OPT kv_meta_t mppt_opt_kv[] = {
    { CATBUS_TYPE_UINT16, MPPT_BINS - 1, KV_FLAGS_READ_ONLY,  &mppt_bins,                  0,  "batt_mppt_bins" },

};

void mppt_v_init( void ){

    kv_v_add_db_info( mppt_opt_kv, sizeof(mppt_opt_kv) );
}

static void reset_mppt( void ){

    if( !enable_mppt ){

        return;
    }

    mppt_current_vindpm = BQ25895_MIN_MPPT_VINDPM;
    mppt_index = 0;
    bq25895_v_set_vindpm( mppt_current_vindpm );

    memset( mppt_bins, 0, sizeof(mppt_bins) );

    mppt_time = tmr_u32_get_system_time_ms();
    mppt_done = FALSE;
}

static void do_mppt( uint16_t charge_current ){

    if( !enable_mppt ){

        return;
    }

    int16_t delta_current = (int16_t)charge_current - (int16_t)mppt_bins[mppt_index];

    // check if algorithm is running:
    if( mppt_done ){

        // check if time to run
        if( ( tmr_u32_elapsed_time_ms( mppt_time) > MPPT_CHECK_INTERVAL ) ||
            ( abs16( delta_current ) >= MPPT_CURRENT_THRESHOLD ) ){ // sudden change in charge current

            mppt_done = FALSE; // note that mppt will start on the NEXT cycle, not this one.
            mppt_index = 0;

            log_v_debug_P( PSTR("start mppt: delta: %d charge: %u index: %u bin: %u"), delta_current, charge_current, mppt_index, mppt_bins[mppt_index] );
        }

        return;
    }   

    // MPPT RUNNING
    
    // store current in bin
    mppt_bins[mppt_index] = charge_current;
    mppt_index++;

    if( mppt_current_vindpm == 0 ){

        mppt_current_vindpm = BQ25895_MIN_MPPT_VINDPM;
    }
    else{

        mppt_current_vindpm += BQ25895_MPPT_VINDPM_STEP;
    }

    // if( mppt_current_vindpm >= BQ25895_MAX_MPPT_VINDPM ){
    if( mppt_index >= cnt_of_array(mppt_bins) ){ // check if done scanning

        // MPPT is finished
        mppt_done = TRUE;
        mppt_time = tmr_u32_get_system_time_ms();

        // select vindpm. this is max of currents stored in mppt_bins, the index representing
        // the vindpm setting that produced it.
        uint16_t best_current = 0;
        uint8_t best_index = 0;

        for( uint8_t i = 0; i < MPPT_BINS; i++ ){

            if( mppt_bins[i] > best_current ){

                best_current = mppt_bins[i];
                best_index = i;
            }
        }

        mppt_index = best_index;

        mppt_current_vindpm = BQ25895_MIN_MPPT_VINDPM + ( best_index * BQ25895_MPPT_VINDPM_STEP );

        log_v_debug_P( PSTR("mppt done: vindpm %u current: %u"), mppt_current_vindpm, best_current );
    }

    bq25895_v_set_vindpm( mppt_current_vindpm );
}




void mppt_v_run( uint16_t charge_current ){

	do_mppt( charge_current );
}

void mppt_v_reset( void ){

	reset_mppt();
}

void mppt_v_enable( void ){

    enable_mppt = TRUE;
}

void mppt_v_disable( void ){

    enable_mppt = FALSE;

    bq25895_v_set_vindpm( 0 );
}

bool mppt_b_is_running( void ){

    return !mppt_done;
}
