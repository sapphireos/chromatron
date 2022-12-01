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

#include "hal_boards.h"
#include "flash_fs.h"

#include "status_led.h"

#include "gfx_lib.h"
#include "vm.h"
#include "pixel.h"

#include "energy.h"
#include "battery.h"
#include "fuel_gauge.h"

#include "bq25895.h"
#include "pca9536.h"
#include "mcp73831.h"

#include "solar.h"

#include "hal_pixel.h"

#ifdef ENABLE_BATTERY

static bool batt_enable;
static bool batt_enable_mcp73831;

static int8_t batt_ui_state;
static bool request_pixels_enabled = FALSE;
static bool request_pixels_disabled = FALSE;
static bool pixels_enabled = FALSE;



#define MAX_BUTTONS 3

static uint8_t button_state;
static uint8_t ui_button;

// button events:
static uint8_t button_event_prev[MAX_BUTTONS];
static uint8_t button_event[MAX_BUTTONS];
#define BUTTON_EVENT_NONE           0
#define BUTTON_EVENT_PRESSED        1
#define BUTTON_EVENT_RELEASED       2
#define BUTTON_EVENT_HOLD           3
#define BUTTON_EVENT_HOLD_RELEASED  4
static uint8_t button_hold_duration[MAX_BUTTONS];

static bool pca9536_enabled;


#define BUTTON_IO_CHECKS            4

#define BUTTON_CHECK_TIMING         50

#define BUTTON_TAP_TIME             8
#define BUTTON_MIN_TIME             1

#define BUTTON_HOLD_TIME            15
#define BUTTON_SHUTDOWN_TIME        60
#define BUTTON_WIFI_TIME            20

#define BUTTON_WAIT_FOR_RELEASE     255
#define DIMMER_RATE                 5000
#define MIN_DIMMER                  20000



static bool fan_on;


static uint16_t batt_max_charge_voltage = BATT_MAX_FLOAT_VOLTAGE;
static uint16_t batt_min_discharge_voltage = BATT_CUTOFF_VOLTAGE;

static uint8_t batt_cells; // number of cells in system
static uint16_t cell_capacity; // mAh capacity of each cell
static uint32_t total_nameplate_capacity;



static void set_batt_capacity( void ){

    uint8_t n_cells = batt_cells;

    if( n_cells < 1 ){

        n_cells = 1;
    }

    if( cell_capacity == 0 ){

        cell_capacity = 3400; // default to NCR18650B
    }

    total_nameplate_capacity = n_cells * cell_capacity;
}


int8_t batt_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__batt_max_charge_voltage ){

            // clamp charge voltage
            if( batt_max_charge_voltage > BATT_MAX_FLOAT_VOLTAGE ){

                batt_max_charge_voltage = BATT_MAX_FLOAT_VOLTAGE;
            }

            if( batt_enable_mcp73831 ){

                // mcp73831 has a fixed charge voltage:
                batt_max_charge_voltage = MCP73831_FLOAT_VOLTAGE;
            }
        }
        else if( hash == __KV__batt_min_discharge_voltage ){

            // clamp charge voltage
            if( batt_min_discharge_voltage < BATT_CUTOFF_VOLTAGE ){

                batt_min_discharge_voltage = BATT_CUTOFF_VOLTAGE;
            }
        }   
        else if( ( hash == __KV__batt_cells ) ||
                 ( hash == __KV__batt_cell_capacity ) ||
                 ( hash == __KV__batt_nameplate_capacity ) ){

            set_batt_capacity();
        }
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}


static uint8_t batt_state;
#define BATT_STATE_OK           0
#define BATT_STATE_LOW          1
#define BATT_STATE_CRITICAL     2
#define BATT_STATE_CUTOFF       3
static uint8_t batt_request_shutdown;


#define EMERGENCY_CUTOFF_VOLTAGE ( BATT_CUTOFF_VOLTAGE - 100 ) // set 100 mv below the main cutoff, to give a little headroom


KV_SECTION_META kv_meta_t battery_enable_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable,                 0,  "batt_enable" },

};

#ifndef ESP8266
KV_SECTION_OPT kv_meta_t battery_enable_mcp73831_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable_mcp73831,        0,  "batt_enable_mcp73831" },
};
#endif

KV_SECTION_OPT kv_meta_t battery_info_kv[] = {
    { CATBUS_TYPE_INT8,   0, KV_FLAGS_READ_ONLY,  &batt_ui_state,               0,  "batt_ui_state" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &pixels_enabled,              0,  "batt_pixel_power" },
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &batt_state,                  0,  "batt_state" },
    { CATBUS_TYPE_BOOL,   0, 0,                   &batt_request_shutdown,       0,  "batt_request_shutdown" },
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_state,                0,  "batt_button_state" },
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[0],             0,  "batt_button_event" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &fan_on,                      0,  "batt_fan_on" },
    
    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &batt_max_charge_voltage,     batt_kv_handler,  "batt_max_charge_voltage" },
    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &batt_min_discharge_voltage,  batt_kv_handler,  "batt_min_discharge_voltage" },

    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_PERSIST,    &batt_cells,                  batt_kv_handler,  "batt_cells" },
    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &cell_capacity,               batt_kv_handler,  "batt_cell_capacity" },
    { CATBUS_TYPE_UINT32, 0, KV_FLAGS_READ_ONLY,  &total_nameplate_capacity,    batt_kv_handler,  "batt_nameplate_capacity" },
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


// static uint8_t fx_low_batt[] __attribute__((aligned(4))) = {
//     #include "low_batt.fx.carray"
// };

// static uint32_t fx_low_batt_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

//     uint32_t ret_val = len;

//     // the pos and len values are already bounds checked by the FS driver
//     switch( op ){
//         case FS_VFILE_OP_READ:
//             memcpy( ptr, &fx_low_batt[pos], len );
//             break;

//         case FS_VFILE_OP_SIZE:
//             ret_val = sizeof(fx_low_batt);
//             break;

//         default:
//             ret_val = 0;
//             break;
//     }

//     return ret_val;
// }


// static uint8_t fx_crit_batt[] __attribute__((aligned(4))) = {
//     #include "crit_batt.fx.carray"
// };

// static uint32_t fx_crit_batt_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

//     uint32_t ret_val = len;

//     // the pos and len values are already bounds checked by the FS driver
//     switch( op ){
//         case FS_VFILE_OP_READ:
//             memcpy( ptr, &fx_crit_batt[pos], len );
//             break;

//         case FS_VFILE_OP_SIZE:
//             ret_val = sizeof(fx_crit_batt);
//             break;

//         default:
//             ret_val = 0;
//             break;
//     }

//     return ret_val;
// }

PT_THREAD( battery_ui_thread( pt_t *pt, void *state ) );

void batt_v_init( void ){

    set_batt_capacity();

    energy_v_init();

    if( !batt_enable ){

        pixels_enabled = TRUE;

        return;
    }

    #ifndef ESP8266
    kv_v_add_db_info( battery_enable_mcp73831_kv, sizeof(battery_enable_mcp73831_kv) );
    #endif

    if( batt_enable_mcp73831 ){

        mcp73831_v_init();
    }
    else if( bq25895_i8_init() < 0 ){

        return;
    }


    // only add batt info if a battery controller is actually present
    kv_v_add_db_info( battery_info_kv, sizeof(battery_info_kv) );


    if( batt_enable_mcp73831 ){

        // mcp73831 has a fixed charge voltage:
        // need to do this after the KV DB is inited:
        batt_max_charge_voltage = MCP73831_FLOAT_VOLTAGE;
    }


    #if defined(ESP8266)
    ui_button = IO_PIN_6_DAC0;
    #elif defined(ESP32)

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        ui_button = IO_PIN_21;
    }
    else{

        ui_button = IO_PIN_17_TX;
    }
    #endif

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

    set_batt_capacity();
    fuel_v_init();

    trace_printf("Battery controller enabled\n");

    // if pixels are below the low power threshold,
    // set the CPU to low speed
    // if( gfx_u16_get_pix_count() < BATT_PIX_COUNT_LOW_POWER_THRESHOLD ){

    //     cpu_v_set_clock_speed_low();
    // }

    batt_v_enable_pixels();

    thread_t_create( battery_ui_thread,
                     PSTR("batt_ui"),
                     0,
                     0 );

    // fs_f_create_virtual( PSTR("low_batt.fxb"), fx_low_batt_vfile_handler );
    // fs_f_create_virtual( PSTR("crit_batt.fxb"), fx_crit_batt_vfile_handler );


    solar_v_init();
}

uint16_t batt_u16_get_charge_voltage( void ){

    return batt_max_charge_voltage;
}

uint16_t batt_u16_get_discharge_voltage( void ){

    return batt_min_discharge_voltage;
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

    if( !batt_enable ){

        // pixels are always enabled if battery system is not enabled (since we can't turn them off)

        return TRUE;
    }

    return pixels_enabled;
}


int8_t batt_i8_get_batt_temp( void ){

    if( batt_enable_mcp73831 ){

        return -127;
    }

    return bq25895_i8_get_temp();
}

uint16_t batt_u16_get_vbus_volts( void ){

    if( batt_enable_mcp73831 ){

        return mcp73831_u16_get_vbus_volts();
    }

    return bq25895_u16_read_vbus();
}

uint16_t batt_u16_get_batt_volts( void ){

    if( batt_enable_mcp73831 ){

        return mcp73831_u16_get_batt_volts();
    }

    return bq25895_u16_get_batt_voltage();
}

uint8_t batt_u8_get_soc( void ){

    return fuel_u8_get_soc();    
}

bool batt_b_is_charging( void ){

    if( batt_enable_mcp73831 ){

        return mcp73831_b_is_charging();        
    }

    return bq25895_b_is_charging();
}

bool batt_b_is_wall_power( void ){

    if( batt_u16_get_vbus_volts() >= BATT_WALL_POWER_THRESHOLD ){

        return TRUE;
    }

    return FALSE;
}

bool batt_b_is_batt_fault( void ){

    if( batt_enable_mcp73831 ){

        return 0;
    }

    return bq25895_u8_get_faults() != 0;
}

uint16_t batt_u16_get_nameplate_capacity( void ){

    return total_nameplate_capacity;
}


static int8_t get_case_temp( void ){

    if( batt_enable_mcp73831 ){

        return -127;
    }

    return bq25895_i8_get_case_temp();
}

// static int8_t get_ambient_temp( void ){

//     if( batt_enable_mcp73831 ){

//         return -127;
//     }

//     return bq25895_i8_get_ambient_temp();
// }

static void shutdown_power( void ){

    if( batt_enable_mcp73831 ){

        mcp73831_v_shutdown();

        return;
    }

    bq25895_v_enable_ship_mode( FALSE );
    bq25895_v_enable_ship_mode( FALSE );
    bq25895_v_enable_ship_mode( FALSE );
}


#if defined(ESP32)

#define FAN_IO IO_PIN_19_MISO
#define BOOST_IO IO_PIN_4_A5

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
    io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( ELITE_FAN_IO, 1 );

    TMR_WAIT( pt, 5000 );
    // io_v_digital_write( ELITE_BOOST_IO, 0 );
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
                // ( get_case_temp() > ( get_ambient_temp() + 2 ) ) ||
                ( get_case_temp() >= 55 ) ){

                fan_on = TRUE;
            }
        }

        while( fan_on && !sys_b_is_shutting_down() ){

            TMR_WAIT( pt, 100 );

            io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
            io_v_digital_write( ELITE_FAN_IO, 1 );

            if( ( batt_i8_get_batt_temp() <= 37 ) &&
                // ( get_case_temp() <= ( get_ambient_temp() + 1 ) ) &&
                ( get_case_temp() <= 52 ) ){

                fan_on = FALSE;
            }
        }
    }

PT_END( pt );
}

#endif

bool batt_b_is_button_pressed( uint8_t button ){

    if( button >= MAX_BUTTONS ){

        return FALSE;
    }

    uint8_t event = button_event[button];

    if( event == BUTTON_EVENT_PRESSED ){

        // clear event
        button_event[button] = BUTTON_EVENT_NONE;

        return TRUE;
    }

    return FALSE;
}


bool batt_b_is_button_hold( uint8_t button ){

    if( button >= MAX_BUTTONS ){

        return FALSE;
    }

    uint8_t event = button_event[button];

    if( event == BUTTON_EVENT_HOLD ){

        // clear event
        button_event[button] = BUTTON_EVENT_NONE;

        return TRUE;
    }
    
    return FALSE;
}


bool batt_b_is_button_released( uint8_t button ){

    if( button >= MAX_BUTTONS ){

        return FALSE;
    }

    uint8_t event = button_event[button];

    if( event == BUTTON_EVENT_RELEASED ){

        // clear event
        button_event[button] = BUTTON_EVENT_NONE;

        return TRUE;
    }
    
    return FALSE;
}


bool batt_b_is_button_hold_released( uint8_t button ){

    if( button >= MAX_BUTTONS ){

        return FALSE;
    }

    uint8_t event = button_event[button];

    if( event == BUTTON_EVENT_HOLD_RELEASED ){

        // clear event
        button_event[button] = BUTTON_EVENT_NONE;

        return TRUE;
    }
    
    return FALSE;
}


PT_THREAD( battery_ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
        
    #if defined(ESP32)

    if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

        thread_t_create( fan_thread,
                         PSTR("fan_control"),
                         0,
                         0 );
    }
    #endif

    // wait until battery controller has started and is reporting voltage
    THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

    while(1){

        TMR_WAIT( pt, BUTTON_CHECK_TIMING );

        // check if pixels should be ENabled:
        if( request_pixels_enabled ){

            request_pixels_disabled = FALSE;

            if( pca9536_enabled ){

                bq25895_v_set_boost_mode( TRUE );

                // wait for boost to start up
                TMR_WAIT( pt, 40 );

                pca9536_v_gpio_write( BATT_IO_BOOST, 0 ); // Enable BOOST output
            }
            else if( batt_enable_mcp73831 ){

                mcp73831_v_enable_pixels();
            }
            #if defined(ESP32)
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                bq25895_v_set_boost_mode( TRUE );

                // wait for boost to start up
                TMR_WAIT( pt, 40 );

                io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
                io_v_digital_write( ELITE_BOOST_IO, 1 );

                TMR_WAIT( pt, 10 );
            }
            #endif

            pixels_enabled = TRUE;
            request_pixels_enabled = FALSE;   
        }

        // check if pixels should be DISabled:
        if( request_pixels_disabled ){

            if( pca9536_enabled ){

                pca9536_v_gpio_write( BATT_IO_BOOST, 1 ); // Disable BOOST output

                bq25895_v_set_boost_mode( FALSE );
            }
            else if( batt_enable_mcp73831 ){

                mcp73831_v_disable_pixels();   
            }
            #if defined(ESP32)
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
                io_v_digital_write( ELITE_BOOST_IO, 0 );

                // bq25895_v_set_boost_mode( FALSE ); // DEBUG!
            }
            #endif

            pixels_enabled = FALSE;
            request_pixels_disabled = FALSE;
        }



        // check charger status

        if( batt_b_is_charging() ||
            ( batt_u16_get_vbus_volts() > 5500 ) ||
            ( batt_b_is_batt_fault() != 0 ) ){

            // disable pixels if:
            // charging
            // battery controller reports a fault
            // if vbus is too high, since that could fry the pixels
            // this can occur with solar panels, hitting just over 7 volts.

            batt_state = BATT_STATE_OK;

            gfx_v_set_system_enable( FALSE );
        }
        else{ // DISCHARGE

            gfx_v_set_system_enable( TRUE );

            uint16_t batt_volts = batt_u16_get_batt_volts();

            // the low battery states are latching, so that a temporary increase in SOC due to voltage fluctuations will not
            // toggle between states.  States only flow towards lower SOC, unless the charger is activated.
            if( ( batt_u8_get_soc() == 0 ) || ( batt_volts < EMERGENCY_CUTOFF_VOLTAGE ) ){
                // for cutoff, we also check voltage as a backup, in case the SOC calculation has a problem.

                if( ( batt_state != BATT_STATE_CUTOFF ) && ( batt_volts != 0 ) ){

                    log_v_debug_P( PSTR("Batt cutoff, shutting down: %u"), batt_volts );
                }

                batt_state = BATT_STATE_CUTOFF;
            }
            else if( batt_u8_get_soc() <= 3 ){

                if( batt_state < BATT_STATE_CRITICAL ){

                    log_v_debug_P( PSTR("Batt critical: %u"), batt_volts );
                    
                    batt_state = BATT_STATE_CRITICAL;
                }
            }
            else if( batt_u8_get_soc() <= 10 ){

                if( batt_state < BATT_STATE_LOW ){

                    log_v_debug_P( PSTR("Batt low: %u"), batt_volts );

                    batt_state = BATT_STATE_LOW;
                }
            }

            if( batt_request_shutdown ){

                // check if on wall power - we cannot shut down when plugged in
                if( batt_b_is_wall_power() ){

                    log_v_debug_P( PSTR("On wall power, cannot initiate power down") );

                    batt_request_shutdown = FALSE;
                }
            }

            if( ( batt_state == BATT_STATE_CUTOFF ) || ( batt_request_shutdown ) ){

                if( batt_request_shutdown ){

                    log_v_debug_P( PSTR("Remotely commanded shutdown") );
                }
                else{

                    log_v_debug_P( PSTR("Low battery cutoff") );
                }

                batt_ui_state = -2;

                sys_v_initiate_shutdown( 5 );

                THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                shutdown_power();

                _delay_ms( 1000 );
            }
        }


        // sample buttons:

        for( uint8_t i = 0; i < MAX_BUTTONS; i++ ){

            uint8_t button_mask = 1 << i;

            if( _ui_b_button_down( i ) ){

                if( button_event_prev[i] != BUTTON_EVENT_PRESSED ){

                    button_event[i] = BUTTON_EVENT_PRESSED;
                    button_event_prev[i] = button_event[i];
                }

                button_state |= button_mask;

                if( button_hold_duration[i] < 255 ){

                    button_hold_duration[i]++;

                    if( button_hold_duration[i] >= BUTTON_HOLD_TIME ){

                        if( button_event_prev[i] != BUTTON_EVENT_HOLD ){

                            button_event[i] = BUTTON_EVENT_HOLD;
                            button_event_prev[i] = button_event[i];
                        }
                    }
                }
            }
            else{   

                // check if was pressed, now released
                if( ( button_state & button_mask ) != 0 ){

                    if( button_event_prev[i] == BUTTON_EVENT_HOLD ){

                        button_event[i] = BUTTON_EVENT_HOLD_RELEASED;
                        button_event_prev[i] = button_event[i];
                    }
                    else if( button_event_prev[i] != BUTTON_EVENT_RELEASED ){

                        button_event[i] = BUTTON_EVENT_RELEASED;
                        button_event_prev[i] = button_event[i];
                    }
                }

                button_state &= ~button_mask;
                button_hold_duration[i] = 0;
            }
        }

        // if button 0 was pressed:
        if( button_state & 1 ){

            // quick way to force a wifi scan if the device has the wifi powered down
            // if it couldn't find a router.
            wifi_v_reset_scan_timeout();

            // override quiet mode on status LED
            status_led_v_override();
        }


        // check for shutdown
        if( button_hold_duration[0] >= BUTTON_SHUTDOWN_TIME ){

            if( button_hold_duration[1] < BUTTON_WIFI_TIME ){

                if( !batt_b_is_wall_power() ){

                    log_v_debug_P( PSTR("Button commanded shutdown") );

                    batt_ui_state = -1;

                    sys_v_initiate_shutdown( 5 );

                    THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

                    shutdown_power();

                    _delay_ms( 1000 );
                }
            }
            else{

                wifi_v_switch_to_ap();
            }
        }
    }

PT_END( pt );
}

#endif