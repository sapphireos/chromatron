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
static bool pixels_enabled = TRUE;


KV_SECTION_OPT kv_meta_t pixelpower_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &pixels_enabled,              0,  "batt_pixel_power" },
};


PT_THREAD( pixel_power_thread( pt_t *pt, void *state ) );


void pixelpower_v_init( void ){

    pixelpower_v_enable_pixels();

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

PT_THREAD( pixel_power_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 100 );

        THREAD_WAIT_WHILE( pt, !request_pixels_disabled && !request_pixels_enabled );

        // check if pixels should be ENabled:
        if( request_pixels_enabled ){

            request_pixels_disabled = FALSE;

            if( solar_b_has_charger2_board() ){

                // bq25895_v_set_boost_mode( TRUE );

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

            if( solar_b_has_charger2_board() ){

                charger2_v_set_boost( FALSE );

                // bq25895_v_set_boost_mode( FALSE );
            }
            else if( batt_b_is_mcp73831_enabled() ){

                mcp73831_v_disable_pixels();   
            }
            #if defined(ESP32)
            else if( ffs_u8_read_board_type() == BOARD_TYPE_ELITE ){

                io_v_set_mode( ELITE_BOOST_IO, IO_MODE_OUTPUT );    
                io_v_digital_write( ELITE_BOOST_IO, 0 );

                // bq25895_v_set_boost_mode( FALSE ); // DEBUG for TILT!
            }
            #endif

            pixels_enabled = FALSE;
            request_pixels_disabled = FALSE;
        }
    }

PT_END( pt );
}