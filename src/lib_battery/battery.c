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

#include "motor.h"


static bool batt_enable;
static int8_t batt_ui_state;
static bool pixels_enabled = TRUE;
// static uint8_t buttons;

static uint8_t batt_state;
#define BATT_STATE_OK           0
#define BATT_STATE_LOW          1
#define BATT_STATE_CRITICAL     2
#define BATT_STATE_CUTOFF       3
static uint8_t batt_request_shutdown;


#define EMERGENCY_CUTOFF_VOLTAGE ( BQ25895_CUTOFF_VOLTAGE - 100 ) // set 100 mv below the main cutoff, to give a little headroom


KV_SECTION_META kv_meta_t ui_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable,           0,   "batt_enable" },
    { SAPPHIRE_TYPE_INT8,   0, KV_FLAGS_READ_ONLY,  &batt_ui_state,         0,   "batt_ui_state" },
    { SAPPHIRE_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &pixels_enabled,        0,   "batt_pixel_power" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_state,            0,   "batt_state" },
    { SAPPHIRE_TYPE_BOOL,   0, 0,                   &batt_request_shutdown, 0,   "batt_request_shutdown" },
    // { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &buttons,       0,        "batt_button_state" },
};


/*

Button notes:
The dimming control works on the submaster dimmer.
The master dimmer is available to configure as desired
over the network.

Interface:

single tap - dimming control, in steps. up then down.
- not yet implemented

hold - 2-3 seconds, battery system turns on
       3 seconds - shuts down system

enable wifi AP:
hold btn 0 for 3 seconds and btn 1 for 6 seconds


*/

#define MAX_BUTTONS 2

static uint8_t button_hold_duration[2];

static bool pca9536_enabled;


#define BUTTON_IO_CHECKS            4

#define BUTTON_CHECK_TIMING         50

#define BUTTON_TAP_TIME             8
#define BUTTON_MIN_TIME             1

#define BUTTON_HOLD_TIME            20
#define BUTTON_SHUTDOWN_TIME        60
#define BUTTON_WIFI_TIME            120

#define BUTTON_WAIT_FOR_RELEASE     255
#define DIMMER_RATE                 5000
#define MIN_DIMMER                  20000

PT_THREAD( ui_thread( pt_t *pt, void *state ) );

void batt_v_init( void ){

    energy_v_init();

    if( !batt_enable ){

        return;
    }

    if( bq25895_i8_init() < 0 ){

        return;
    }

    log_v_info_P( PSTR("BQ25895 detected") );

    // motor_v_init();

    if( pca9536_i8_init() == 0 ){

        log_v_info_P( PSTR("PCA9536 detected") );
        pca9536_enabled = TRUE;
    }
    else{

        io_v_set_mode( UI_BUTTON, IO_MODE_INPUT_PULLUP );    
    }

    trace_printf("Battery controller enabled\n");

    // if pixels are below the low power threshold,
    // set the CPU to low speed
    if( gfx_u16_get_pix_count() < BATT_PIX_COUNT_LOW_POWER_THRESHOLD ){

        cpu_v_set_clock_speed_low();
    }

    thread_t_create( ui_thread,
                     PSTR("ui"),
                     0,
                     0 );


    // dimming_direction_down = TRUE;
    // last_dimmer = gfx_u16_get_submaster_dimmer();
}

static bool _ui_b_button_down( uint8_t ch ){

    uint8_t btn = 255;

    if( ch == 0 ){

        if( pca9536_enabled ){    

            btn = BATT_IO_QON;
        }
        else{

            btn = UI_BUTTON;
        }
    }    
    else if( ch == 1 ){

        if( pca9536_enabled ){    

            btn = BATT_IO_S2;
        }
    }    
    
    if( btn == 255 ){

        return FALSE;
    }

    // some filtering on button pin
    for( uint8_t i = 0; i < BUTTON_IO_CHECKS; i++){

        if( pca9536_enabled ){

            if( pca9536_b_gpio_read( btn ) ){

                return FALSE;
            }
        }
        else{

            if( io_b_digital_read( btn ) ){

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
    gfx_v_set_pixel_power( pixels_enabled );
}

void batt_v_disable_pixels( void ){

    if( pca9536_enabled ){

        pca9536_v_gpio_write( BATT_IO_BOOST, 1 ); // Disable BOOST output

        pixels_enabled = FALSE;
    }

    gfx_v_set_pixel_power( pixels_enabled );
}

bool batt_b_pixels_enabled( void ){

    return pixels_enabled;
}

PT_THREAD( ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // charger2 board, setup IO
    if( pca9536_enabled ){
        
        pca9536_v_set_input( BATT_IO_QON );
        pca9536_v_set_input( BATT_IO_S2 );
        pca9536_v_set_input( BATT_IO_SPARE );

        pca9536_v_set_output( BATT_IO_BOOST );

        batt_v_disable_pixels();
    }

    while(1){

        TMR_WAIT( pt, BUTTON_CHECK_TIMING );

        // buttons = 0;
        if( pca9536_enabled ){

            // check if LEDs enabled.
            // this will automatically shutdown the pixel strip
            // if LED graphics are not enabled.
            if( gfx_b_enabled() ){

                if( !batt_b_pixels_enabled() ){
                    
                    trace_printf("Pixel power enabled\n");
                    batt_v_enable_pixels();
                }
            }
            else{

                if( batt_b_pixels_enabled() ){
                    
                    trace_printf("Pixel power disabled\n");
                    batt_v_disable_pixels();   
                }
            }

            // if( pca9536_b_gpio_read( BATT_IO_QON ) ){

            //     buttons |= ( 1 << BATT_IO_QON );
            // }
            // if( pca9536_b_gpio_read( BATT_IO_S2 ) ){

            //     buttons |= ( 1 << BATT_IO_S2 );
            // }
            // if( pca9536_b_gpio_read( BATT_IO_SPARE ) ){

            //     buttons |= ( 1 << BATT_IO_SPARE );
            // }
        }

        uint8_t charge_status = bq25895_u8_get_charge_status();

        if( ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE) ||
            ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE) ||
            ( bq25895_u8_get_faults() != 0 ) ){

            batt_state = BATT_STATE_OK;
            gfx_b_disable();
        }
        else if( charge_status == BQ25895_CHARGE_STATUS_CHARGE_DONE ){

            batt_state = BATT_STATE_OK;
            gfx_b_enable();
            batt_v_enable_pixels();
        }
        else{

            gfx_b_enable();
            batt_v_enable_pixels();

            // the low battery states are latching, so that a temporary increase in SOC due to voltage fluctuations will not
            // toggle between states.  States only flow towards lower SOC, unless the charger is activated.
            if( ( bq25895_u8_get_soc() <= 0 ) || ( bq25895_u16_get_batt_voltage() < EMERGENCY_CUTOFF_VOLTAGE ) ){
                // for cutoff, we also check voltage as a backup, in case the SOC calculation has a problem.

                if( batt_state != BATT_STATE_CUTOFF ){

                    log_v_debug_P( PSTR("Batt cutoff, shutting down: %u"), bq25895_u16_get_batt_voltage() );
                }

                batt_state = BATT_STATE_CUTOFF;
            }
            else if( bq25895_u8_get_soc() <= 3 ){

                if( batt_state < BATT_STATE_CRITICAL ){

                    log_v_debug_P( PSTR("Batt critical: %u"), bq25895_u16_get_batt_voltage() );
                    
                    batt_state = BATT_STATE_CRITICAL;
                }
            }
            else if( bq25895_u8_get_soc() <= 10 ){

                if( batt_state < BATT_STATE_LOW ){

                    log_v_debug_P( PSTR("Batt low: %u"), bq25895_u16_get_batt_voltage() );

                    batt_state = BATT_STATE_LOW;
                }
            }

            if( ( batt_state == BATT_STATE_CUTOFF ) || ( batt_request_shutdown ) ){

                gfx_b_disable();

                batt_ui_state = -2;

                sys_v_initiate_shutdown( 5 );

                THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                bq25895_v_enable_ship_mode( FALSE );
            }
            else if( batt_state == BATT_STATE_CRITICAL ){

                
            }
            else if( batt_state == BATT_STATE_LOW ){

                
            }
        }


        for( uint8_t i = 0; i < MAX_BUTTONS; i++ ){

            if( _ui_b_button_down( i ) ){

                if( button_hold_duration[i] < 255 ){

                    button_hold_duration[i]++;
                }
            }
            else{

                button_hold_duration[i] = 0;
            }
        }


        // check for shutdown
        if( button_hold_duration[0] >= BUTTON_SHUTDOWN_TIME ){

            if( button_hold_duration[1] < BUTTON_WIFI_TIME ){

                batt_ui_state = -1;

                sys_v_initiate_shutdown( 5 );

                THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                bq25895_v_enable_ship_mode( FALSE );
            }
            else{

                wifi_v_switch_to_ap();
            }
        }
    }

PT_END( pt );
}

