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

#include "sapphire.h"

#include "gfx_lib.h"
#include "vm.h"

#include "energy.h"
#include "battery.h"

#include "bq25895.h"
#include "pca9536.h"

static bool batt_enable;
static int8_t batt_ui_state;
static uint8_t buttons;

KV_SECTION_META kv_meta_t ui_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable, 0,          "batt_enable" },
    { SAPPHIRE_TYPE_INT8,   0, KV_FLAGS_READ_ONLY,  &batt_ui_state, 0,        "batt_ui_state" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &buttons,       0,        "batt_button_state" },
};


/*

Button notes:
The dimming control works on the submaster dimmer.
The master dimmer is available to configure as desired
over the network.

Interface:

single tap - dimming control, in steps. up then down.

hold - 2-3 seconds, battery system turns on
       5 seconds - shuts down system

*/

static uint8_t button_hold_duration;
static bool dimming_direction_down;
// static uint16_t last_dimmer;

#define BUTTON_IO_CHECKS            4

#define BUTTON_CHECK_TIMING         50

#define BUTTON_TAP_TIME             8
#define BUTTON_MIN_TIME             1

#define BUTTON_HOLD_TIME            20
#define BUTTON_SHUTDOWN_TIME        60

#define BUTTON_WAIT_FOR_RELEASE     255
#define DIMMER_RATE                 5000
#define MIN_DIMMER                  20000


PT_THREAD( ui_thread( pt_t *pt, void *state ) );

static bool pca9536_enabled;
static bool pixels_enabled = TRUE;

void batt_v_init( void ){

    energy_v_init();

    if( !batt_enable ){

        return;
    }

    if( bq25895_i8_init() < 0 ){

        return;
    }

    log_v_info_P( PSTR("BQ25895 detected") );

    if( pca9536_i8_init() == 0 ){

        log_v_info_P( PSTR("PCA9536 detected") );
        pca9536_enabled = TRUE;
    }
    else{

        io_v_set_mode( UI_BUTTON, IO_MODE_INPUT_PULLUP );    
    }

    thread_t_create( ui_thread,
                     PSTR("ui"),
                     0,
                     0 );


    dimming_direction_down = TRUE;
    // last_dimmer = gfx_u16_get_submaster_dimmer();
}

static bool _ui_b_button_down( void ){

    // some filtering on button pin
    for( uint8_t i = 0; i < BUTTON_IO_CHECKS; i++){

        if( pca9536_enabled ){

            if( pca9536_b_gpio_read( BATT_IO_QON ) ){

                return FALSE;
            }
        }
        else{

            if( io_b_digital_read( UI_BUTTON ) ){

                return FALSE;
            }
        }
    }

    return TRUE;
}

void batt_v_enable_pixels( void ){

    if( pca9536_enabled ){

        pca9536_v_gpio_write( BATT_IO_BOOST, 0 ); // Enable BOOST output
    }

    pixels_enabled = TRUE;
}

void batt_v_disable_pixels( void ){

    if( pca9536_enabled ){

        pca9536_v_gpio_write( BATT_IO_BOOST, 1 ); // Disable BOOST output

        pixels_enabled = FALSE;
    }
}

bool batt_b_pixels_enabled( void ){

    return pixels_enabled;
}

PT_THREAD( ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // gfx_v_set_submaster_dimmer( 65535 );

    // last_dimmer = gfx_u16_get_submaster_dimmer();
    
    // charger2 board, setup IO
    if( pca9536_enabled ){
        
        pca9536_v_set_input( BATT_IO_QON );
        pca9536_v_set_input( BATT_IO_S2 );
        pca9536_v_set_input( BATT_IO_SPARE );

        pca9536_v_set_output( BATT_IO_BOOST );

        batt_v_enable_pixels();
    }

    while(1){

        TMR_WAIT( pt, BUTTON_CHECK_TIMING );

        buttons = 0;
        if( pca9536_enabled ){

            if( pca9536_b_gpio_read( BATT_IO_QON ) ){

                buttons |= ( 1 << BATT_IO_QON );
            }
            if( pca9536_b_gpio_read( BATT_IO_S2 ) ){

                buttons |= ( 1 << BATT_IO_S2 );
            }
            if( pca9536_b_gpio_read( BATT_IO_SPARE ) ){

                buttons |= ( 1 << BATT_IO_SPARE );
            }
        }

        uint8_t charge_status = bq25895_u8_get_charge_status();

        if( ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE) ||
            ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE) ){

            // vm_v_stop();
            // last_dimmer = gfx_u16_get_submaster_dimmer();
            // gfx_v_set_submaster_dimmer( MIN_DIMMER );
        }
        else if( charge_status == BQ25895_CHARGE_STATUS_CHARGE_DONE ){

            // gfx_v_set_submaster_dimmer( MIN_DIMMER );

            // TMR_WAIT( pt, 5000 );
        }
        else{

            if( bq25895_u8_get_soc() <= 5 ){

                // last_dimmer = gfx_u16_get_submaster_dimmer();
                // gfx_v_set_submaster_dimmer( MIN_DIMMER );
            }
            else if( bq25895_u8_get_soc() <= 2 ){

                log_v_debug_P( PSTR("Low batt, shutting down: %u"), bq25895_u16_get_batt_voltage() );

                batt_ui_state = -2;

                sys_v_initiate_shutdown( 5 );

                THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                bq25895_v_enable_ship_mode( FALSE );
            }
            else{

                // gfx_v_set_submaster_dimmer( last_dimmer );
            }
        }

        if( _ui_b_button_down() ){

            // check for hold
            if( ( button_hold_duration >= BUTTON_HOLD_TIME ) &&
                ( button_hold_duration < BUTTON_WAIT_FOR_RELEASE ) ){

                if( button_hold_duration >= BUTTON_SHUTDOWN_TIME ){

                    // vm_v_shutdown();
                    batt_ui_state = -1;

                    TMR_WAIT( pt, 5000 );

                    bq25895_v_enable_ship_mode( FALSE );
                }
            }

            if( button_hold_duration < 255 ){

                button_hold_duration++;
            }
        }
        else{

            // check for single tap
            if( ( button_hold_duration <= BUTTON_TAP_TIME ) &&
                ( button_hold_duration > BUTTON_MIN_TIME ) ){

                // int32_t dimmer = gfx_u16_get_submaster_dimmer();
                //
                // if( dimming_direction_down ){
                //
                //     dimmer -= DIMMER_RATE;
                // }
                // else{
                //
                //     dimmer += DIMMER_RATE;
                // }
                //
                // if( dimmer > 65535 ){
                //
                //     dimmer = 65535;
                //     dimming_direction_down = TRUE;
                // }
                // else if( dimmer < MIN_DIMMER ){
                //
                //     dimmer = MIN_DIMMER;
                //     dimming_direction_down = FALSE;
                // }
                //
                // last_dimmer = dimmer;
                // gfx_v_set_submaster_dimmer( dimmer );
            }

            button_hold_duration = 0;
        }
    }

PT_END( pt );
}

