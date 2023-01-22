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

#include "solar.h"
#include "charger2.h"
#include "patch_board.h"
#include "buttons.h"
#include "battery.h"
#include "pca9536.h"


#define MAX_BUTTONS 4

static uint8_t button_state;
static int8_t batt_ui_button = -1; // physically installed button for QON and wired direct to MCU
// the batt ui button is always button channel 0

// button events:
static uint8_t button_event_prev[MAX_BUTTONS];
static uint8_t button_event[MAX_BUTTONS];
#define BUTTON_EVENT_NONE           0
#define BUTTON_EVENT_PRESSED        1
#define BUTTON_EVENT_RELEASED       2
#define BUTTON_EVENT_HOLD           3
#define BUTTON_EVENT_HOLD_RELEASED  4

static uint8_t button_hold_duration[MAX_BUTTONS];

#define BUTTON_CHECK_TIMING         50
#define BUTTON_IO_CHECKS            4

#define BUTTON_CHECK_TIMING         50

#define BUTTON_HOLD_TIME            ( 750 / BUTTON_CHECK_TIMING )
#define BUTTON_SHUTDOWN_TIME        ( 3000 / BUTTON_CHECK_TIMING )
#define BUTTON_WIFI_TIME            ( 1000 / BUTTON_CHECK_TIMING )

static uint8_t batt_request_shutdown;



KV_SECTION_OPT kv_meta_t button_batt_opt_kv[] = {
    { CATBUS_TYPE_BOOL,   0, 0,                   &batt_request_shutdown,       0,  "batt_request_shutdown" },

    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[0],             0,  "batt_button_event" },    
};

KV_SECTION_OPT kv_meta_t button_ui_opt_kv[] = {
    #if MAX_BUTTONS >=1
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_state,                0,  "button_state" },

    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[0],             0,  "button_event_0" },    
    #elif MAX_BUTTONS >= 2
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[1],             0,  "button_event_1" },    
    #elif MAX_BUTTONS >= 3
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[2],             0,  "button_event_2" },    
    #elif MAX_BUTTONS >= 4
    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &button_event[3],             0,  "button_event_3" },    
    #endif
};


PT_THREAD( button_thread( pt_t *pt, void *state ) );


void button_v_init( void ){

    if( batt_b_enabled() ){

        // optional controls for battery systems:
        kv_v_add_db_info( button_batt_opt_kv, sizeof(button_batt_opt_kv) );    
    }

    // normal button controls:
    kv_v_add_db_info( button_ui_opt_kv, sizeof(button_ui_opt_kv) );


    #if defined(ESP8266)
    batt_ui_button = IO_PIN_6_DAC0;
    #elif defined(ESP32)

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        batt_ui_button = IO_PIN_21;
    }
    else{

        batt_ui_button = IO_PIN_17_TX;
    }
    #endif

    if( batt_ui_button >= 0 ){

        io_v_set_mode( batt_ui_button, IO_MODE_INPUT_PULLUP );     
    }


    thread_t_create( button_thread,
                     PSTR("button"),
                     0,
                     0 );
}


bool button_b_is_button_pressed( uint8_t button ){

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


bool button_b_is_button_hold( uint8_t button ){

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


bool button_b_is_button_released( uint8_t button ){

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


bool button_b_is_button_hold_released( uint8_t button ){

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

bool button_b_is_shutdown_requested( void ){

    return batt_request_shutdown;
}

// UNPRESSED BUTTONS READ TRUE (HIGH!)
static bool _button_b_read_button( uint8_t ch ){

    if( ch >= MAX_BUTTONS ){

        return TRUE;
    }

    if( solar_b_has_charger2_board() ){
        
        if( ch == 0 ){

            return charger2_b_read_qon();
        }
        else if( ch == 1 ){

            return charger2_b_read_s2();
        }
        // else if( ch == 2 ){

        //     return charger2_b_read_spare(); // spare has never been connected on actual hardware
        // }
    }
    else if( solar_b_has_patch_board() ){

        if( ch == 0 ){

            return io_b_digital_read( batt_ui_button );
        }
        else if( ch == 1 ){

            return patchboard_b_read_io2();
        }
    }
    else{

        if( ch == 0 ){

            return io_b_digital_read( batt_ui_button );
        }
        #if MAX_BUTTONS >= 2
        else if( ch == 1 ){

            #if defined(ESP32)
            return io_b_digital_read( IO_PIN_13_A12 ); // PLACEHOLDER FIX THIS!
            #endif

            return TRUE;
        }
        #endif
        #if MAX_BUTTONS >= 3
        else if( ch == 2 ){

            #if defined(ESP32)
            return io_b_digital_read( IO_PIN_13_A12 ); // PLACEHOLDER FIX THIS!
            #endif

        return TRUE;
        }
        #if MAX_BUTTONS >= 4
        else if( ch == 3 ){

            #if defined(ESP32)
            return io_b_digital_read( IO_PIN_13_A12 ); // PLACEHOLDER FIX THIS!
            #endif

            return TRUE;
        }
        #endif
        #endif
    }

    return TRUE;
}

static bool _button_b_button_down( uint8_t ch ){

    // some filtering on button pin
    for( uint8_t i = 0; i < BUTTON_IO_CHECKS; i++){

        if( _button_b_read_button( ch ) ){ // note the inverted logic, unpressed buttons read as high/true

            return FALSE;
        }
    }

    return TRUE;
}

PT_THREAD( button_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, BUTTON_CHECK_TIMING );


        // sample buttons:

        for( uint8_t i = 0; i < MAX_BUTTONS; i++ ){

            uint8_t button_mask = 1 << i;

            if( _button_b_button_down( i ) ){

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

        if( batt_b_enabled() ){

            // check for shutdown
            if( button_hold_duration[0] >= BUTTON_SHUTDOWN_TIME ){

                if( button_hold_duration[1] < BUTTON_WIFI_TIME ){

                    if( !batt_b_is_external_power() ){

                        log_v_debug_P( PSTR("Button commanded shutdown") );

                        // set shutdown request
                        batt_request_shutdown = TRUE;

                        TMR_WAIT( pt, 120000 ); 
                        // power should be off by now, but if not,
                        // just carry on?    
                    }
                }
                else{

                    log_v_debug_P( PSTR("Button commanded switch to AP mode") );

                    wifi_v_switch_to_ap();
                }
            }
        }
    }
    

PT_END( pt );
}
