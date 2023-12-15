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

#include "bq25895.h"
#include "charger2.h"
#include "patch_board.h"
#include "mcp73831.h"
#include "solar.h"
#include "pixel_power.h"
#include "battery.h"

static bool request_pixels_enabled = FALSE;
static bool request_pixels_disabled = FALSE;
static bool pixels_enabled = TRUE; // default to ON, so systems that do not use the pixel power control will output pixels.
static bool power_control_enabled = FALSE;


KV_SECTION_OPT kv_meta_t pixelpower_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &pixels_enabled,              0,  "batt_pixel_power" },
};


PT_THREAD( pixel_power_thread( pt_t *pt, void *state ) );



#if defined(ESP32)
static void enable_pixel_power_fet( void ){

    io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( ELITE_BOOST_IO, 1 );
}

static void disable_pixel_power_fet( void ){

    io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
    io_v_digital_write( ELITE_BOOST_IO, 0 );
}
#endif


static bool is_vbus_valid( void ){

    uint16_t vbus = batt_u16_get_vbus_volts();

    if( vbus >= PIXEL_POWER_MAX_VBUS ){

        //overvoltage
        // verify VBUS status

        if( bq25895_u8_get_vbus_status() != 7 ){

            return FALSE;
        }
    }

    return TRUE;
}


static void pixels_off( void ){

    // trace_printf("Pixel power DISABLE\r\n");

    if( solar_b_has_charger2_board() ){

        charger2_v_set_boost( FALSE );
    }
    else if( batt_b_is_mcp73831_enabled() ){

        mcp73831_v_disable_pixels();   
    }
    #if defined(ESP32)
    else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

        disable_pixel_power_fet();

        bq25895_v_set_boost_mode( FALSE );
    }
    #endif

    pixels_enabled = FALSE;
    request_pixels_disabled = FALSE;
}


void pixelpower_v_init( void ){

    // pixel power system defaults to OFF if power control is enabled
    pixels_enabled = FALSE;
    power_control_enabled = TRUE;

    #if defined(ESP32)
    disable_pixel_power_fet();
    #endif

    // check if hardware has power control:
    if( solar_b_has_charger2_board() ){

    }
    else if( batt_b_is_mcp73831_enabled() ){

    }
    #if defined(ESP32)
    else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

    }
    #endif
    else{

        pixels_enabled = TRUE;

        // this hardware does not have power control

        return;
    }

    kv_v_add_db_info( pixelpower_info_kv, sizeof(pixelpower_info_kv) );

    thread_t_create( pixel_power_thread,
                     PSTR("pixel_power_control"),
                     0,
                     0 );
}


void pixelpower_v_enable_pixels( void ){

    request_pixels_enabled = TRUE;
}

void pixelpower_v_disable_pixels( void ){

    request_pixels_disabled = TRUE;
}

bool pixelpower_b_pixels_enabled( void ){

    return pixels_enabled;
}

bool pixelpower_b_power_control_enabled( void ){

    return power_control_enabled;
}

// to be used only when shutting down the system
// for power off or a reboot.
// this is cut pixel power immediately.
void pixelpower_v_system_shutdown( void ){

    pixels_off();
}


PT_THREAD( pixel_power_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, is_vbus_valid() && !request_pixels_disabled && !request_pixels_enabled );


        // verify that vbus is below maximum
        // if not (meaning, it is above the trigger threshold)
        // pixel power will be deactivated continously.
        // pending enable requests will not be reset - 
        // if there is an enable (and no disable request) pending
        // when vbus becomes valid, the pixels will be enabled.
        // this loop, because it runs continuously, also serves
        // to continuously monitor the vbus voltage and will immediately 
        // (well, asap anyay) shut down pixel power.
        if( !is_vbus_valid() ){
            
            pixels_off();
            
            TMR_WAIT( pt, 20 );

            continue;
        }

        // trace_printf("Pixel power: enable: %d disable: %d\r\n", request_pixels_enabled, request_pixels_disabled );

        // check if both requests are set.
        // priority goes to shutting down the pixel power.

        // check if pixels should be ENabled:
        if( !request_pixels_disabled && request_pixels_enabled ){

            // trace_printf("Pixel power ENABLE\r\n");

            if( solar_b_has_charger2_board() ){

                // wait for boost to start up
                TMR_WAIT( pt, 40 );

                charger2_v_set_boost( TRUE );
            }
            else if( batt_b_is_mcp73831_enabled() ){

                mcp73831_v_enable_pixels();
            }
            #if defined(ESP32)
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                bq25895_v_set_boost_mode( TRUE );                

                // wait for boost to start up
                TMR_WAIT( pt, 40 );

                // trace_printf("Pixel power BOOST ON\r\n");

                enable_pixel_power_fet();

                TMR_WAIT( pt, 10 );
            }
            #endif

            // log_v_debug_P( PSTR("pixel power enabled!") );

            pixels_enabled = TRUE;
        }

        

        request_pixels_enabled = FALSE;

        // check if pixels should be DISabled:
        if( request_pixels_disabled ){

            // log_v_debug_P( PSTR("pixel power disabled!") );

            pixels_off();
        }

        TMR_WAIT( pt, 50 );
    }

PT_END( pt );
}