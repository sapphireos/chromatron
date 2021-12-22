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

#include "hal_boards.h"
#include "flash_fs.h"

#include "gfx_lib.h"
#include "vm.h"
#include "pixel.h"

#include "energy.h"
#include "battery.h"

#include "bq25895.h"
#include "pca9536.h"

#include "hal_pixel.h"


static bool batt_enable;
static int8_t batt_ui_state;
static bool request_pixels_enabled = FALSE;
static bool request_pixels_disabled = FALSE;
static bool pixels_enabled = FALSE;
static uint8_t button_state;
static uint8_t ui_button;
static bool fan_on;

static uint8_t batt_state;
#define BATT_STATE_OK           0
#define BATT_STATE_LOW          1
#define BATT_STATE_CRITICAL     2
#define BATT_STATE_CUTOFF       3
static uint8_t batt_request_shutdown;


#define EMERGENCY_CUTOFF_VOLTAGE ( BQ25895_CUTOFF_VOLTAGE - 100 ) // set 100 mv below the main cutoff, to give a little headroom


KV_SECTION_META kv_meta_t ui_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable,           0,   "batt_enable" },
    { CATBUS_TYPE_INT8,   0, KV_FLAGS_READ_ONLY,  &batt_ui_state,         0,   "batt_ui_state" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &pixels_enabled,        0,   "batt_pixel_power" },
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_state,            0,   "batt_state" },
    { CATBUS_TYPE_BOOL,   0, 0,                   &batt_request_shutdown, 0,   "batt_request_shutdown" },
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_state,          0,   "batt_button_state" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &fan_on,                0,   "batt_fan_on" },
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

#define MAX_BUTTONS 3

static uint8_t button_hold_duration[MAX_BUTTONS];

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

static uint8_t fx_low_batt[] __attribute__((aligned(4))) = {
    #include "low_batt.fx.carray"
};

static uint16_t fx_low_batt_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    uint16_t ret_val = len;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){
        case FS_VFILE_OP_READ:
            memcpy( ptr, &fx_low_batt[pos], len );
            break;

        case FS_VFILE_OP_SIZE:
            ret_val = sizeof(fx_low_batt);
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}


static uint8_t fx_crit_batt[] __attribute__((aligned(4))) = {
    #include "crit_batt.fx.carray"
};

static uint16_t fx_crit_batt_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    uint16_t ret_val = len;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){
        case FS_VFILE_OP_READ:
            memcpy( ptr, &fx_crit_batt[pos], len );
            break;

        case FS_VFILE_OP_SIZE:
            ret_val = sizeof(fx_crit_batt);
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}

PT_THREAD( ui_thread( pt_t *pt, void *state ) );


#define FAN_IO IO_PIN_19_MISO
#define BOOST_IO IO_PIN_4_A5

void batt_v_init( void ){

    #if defined(ESP8266)
    ui_button = IO_PIN_6_DAC0;
    #elif defined(ESP32)

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        ui_button = IO_PIN_21;


        // BOOST
        io_v_set_mode( BOOST_IO, IO_MODE_OUTPUT );    
        io_v_digital_write( BOOST_IO, 0 );

        // FAN
        io_v_set_mode( FAN_IO, IO_MODE_OUTPUT );    
        io_v_digital_write( FAN_IO, 0 );
    }
    else{

        ui_button = IO_PIN_17_TX;
    }
    #endif

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

        pca9536_v_set_input( BATT_IO_QON );
        pca9536_v_set_input( BATT_IO_S2 );
        pca9536_v_set_input( BATT_IO_SPARE );
        pca9536_v_set_output( BATT_IO_BOOST );
    }
    else{

        io_v_set_mode( ui_button, IO_MODE_INPUT_PULLUP );    
    }

    trace_printf("Battery controller enabled\n");

    // if pixels are below the low power threshold,
    // set the CPU to low speed
    if( gfx_u16_get_pix_count() < BATT_PIX_COUNT_LOW_POWER_THRESHOLD ){

        cpu_v_set_clock_speed_low();
    }

    batt_v_enable_pixels();

    thread_t_create( ui_thread,
                     PSTR("ui"),
                     0,
                     0 );

    fs_f_create_virtual( PSTR("low_batt.fxb"), fx_low_batt_vfile_handler );
    fs_f_create_virtual( PSTR("crit_batt.fxb"), fx_crit_batt_vfile_handler );
}

static bool _ui_b_button_down( uint8_t ch ){

    uint8_t btn = 255;

    if( ch == 0 ){

        if( pca9536_enabled ){    

            btn = BATT_IO_QON;
        }
        else{

            btn = ui_button;
        }
    }    
    else if( ch == 1 ){

        if( pca9536_enabled ){    

            btn = BATT_IO_S2;
        }
    }    
    else if( ch == 2 ){

        if( pca9536_enabled ){    

            btn = BATT_IO_SPARE;
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

    request_pixels_enabled = TRUE;
}

void batt_v_disable_pixels( void ){

    request_pixels_disabled = TRUE;
}

bool batt_b_pixels_enabled( void ){

    return pixels_enabled;
}


PT_THREAD( fan_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // BOOST
    io_v_set_mode( BOOST_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( BOOST_IO, 1 );

    // FAN
    io_v_set_mode( FAN_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( FAN_IO, 1 );

    TMR_WAIT( pt, 5000 );
    // io_v_digital_write( BOOST_IO, 0 );
    io_v_digital_write( FAN_IO, 0 );


    while(1){

        TMR_WAIT( pt, 10000 );

        if( fan_on ){

            if( bq25895_i8_get_temp() <= 37 ){

                fan_on = FALSE;
            }
        }
        else{

            if( bq25895_i8_get_temp() >= 38 ){

                fan_on = TRUE;
            }
        }

        // fan control for elite board
        if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

            io_v_set_mode( FAN_IO, IO_MODE_OUTPUT );    

            if( fan_on ){

                io_v_digital_write( FAN_IO, 1 );
            }
            else{

                io_v_digital_write( FAN_IO, 0 );
            }
        }        

    }

PT_END( pt );
}

PT_THREAD( ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );


    // TEST
    if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

        thread_t_create( fan_thread,
                         PSTR("fan_control"),
                         0,
                         0 );

    }

    // wait until battery controller has started and is reporting voltage
    THREAD_WAIT_WHILE( pt, bq25895_u16_get_batt_voltage() == 0 );

    while(1){

        TMR_WAIT( pt, BUTTON_CHECK_TIMING );

        // check if pixels should be enabled:
        if( request_pixels_enabled ){

            if( pca9536_enabled ){

                pca9536_v_gpio_write( BATT_IO_BOOST, 0 ); // Enable BOOST output
            }
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                bq25895_v_set_boost_mode( TRUE );

                // wait for boost to start up
                TMR_WAIT( pt, 40 );

                io_v_set_mode( BOOST_IO, IO_MODE_OUTPUT );    
                io_v_digital_write( BOOST_IO, 1 );

                TMR_WAIT( pt, 10 );
            }

            pixels_enabled = TRUE;
            request_pixels_enabled = FALSE;   
        }

        if( request_pixels_disabled ){

            if( pca9536_enabled ){

                pca9536_v_gpio_write( BATT_IO_BOOST, 1 ); // Disable BOOST output

                pixels_enabled = FALSE;
            }
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                io_v_set_mode( BOOST_IO, IO_MODE_OUTPUT );    
                io_v_digital_write( BOOST_IO, 0 );

                bq25895_v_set_boost_mode( FALSE );

                pixels_enabled = FALSE;
            }

            request_pixels_disabled = FALSE;
        }

        // check charger status
        uint8_t charge_status = bq25895_u8_get_charge_status();

        if( ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE) ||
            ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE) ||
            bq25895_b_solar_hold() ||
            ( bq25895_u8_get_faults() != 0 ) ){

            batt_state = BATT_STATE_OK;

            gfx_v_set_system_enable( FALSE );
            
            vm_v_resume( 0 );
            vm_v_stop( VM_LAST_VM );
        }
        else if( charge_status == BQ25895_CHARGE_STATUS_CHARGE_DONE ){

            batt_state = BATT_STATE_OK;

            gfx_v_set_system_enable( TRUE );

            vm_v_resume( 0 );
            vm_v_stop( VM_LAST_VM );
        }
        else{ // DISCHARGE

            gfx_v_set_system_enable( TRUE );

            uint16_t batt_volts = bq25895_u16_get_batt_voltage();

            // the low battery states are latching, so that a temporary increase in SOC due to voltage fluctuations will not
            // toggle between states.  States only flow towards lower SOC, unless the charger is activated.
            if( ( bq25895_u8_get_soc() <= 0 ) || ( batt_volts < EMERGENCY_CUTOFF_VOLTAGE ) ){
                // for cutoff, we also check voltage as a backup, in case the SOC calculation has a problem.

                if( ( batt_state != BATT_STATE_CUTOFF ) && ( batt_volts != 0 ) ){

                    log_v_debug_P( PSTR("Batt cutoff, shutting down: %u"), batt_volts );
                }

                batt_state = BATT_STATE_CUTOFF;
            }
            else if( bq25895_u8_get_soc() <= 3 ){

                if( batt_state < BATT_STATE_CRITICAL ){

                    log_v_debug_P( PSTR("Batt critical: %u"), batt_volts );
                    
                    batt_state = BATT_STATE_CRITICAL;

                    vm_v_pause( 0 );
                    vm_v_run_prog( "crit_batt.fxb", VM_LAST_VM );
                }
            }
            else if( bq25895_u8_get_soc() <= 10 ){

                if( batt_state < BATT_STATE_LOW ){

                    log_v_debug_P( PSTR("Batt low: %u"), batt_volts );

                    batt_state = BATT_STATE_LOW;

                    vm_v_pause( 0 );
                    vm_v_run_prog( "low_batt.fxb", VM_LAST_VM );
                }
            }

            if( ( batt_state == BATT_STATE_CUTOFF ) || ( batt_request_shutdown ) ){

                batt_ui_state = -2;

                sys_v_initiate_shutdown( 5 );

                THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                bq25895_v_enable_ship_mode( FALSE );
            }
        }

        button_state = 0;

        for( uint8_t i = 0; i < MAX_BUTTONS; i++ ){

            if( _ui_b_button_down( i ) ){

                button_state |= 1 << i;

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

