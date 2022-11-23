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

#include "system.h"
#include "threading.h"
#include "timers.h"
#include "config.h"
#include "io.h"
#include "flash_fs.h"
#include "wifi.h"
#include "timesync.h"
#include "keyvalue.h"
#include "hal_boards.h"
#include "status_led.h"


/*


Refactor status LED module into a common module with a thin HAL layer here.


Add ability for a button press to disable led quiet mode for 10 seconds.

*/

void reset_all( void ){

    io_v_set_mode( IO_PIN_LED0, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED1, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED2, IO_MODE_OUTPUT );

    bool direction = TRUE;

    // wrover kit has inverted LED drive
    if( ffs_u8_read_board_type() == BOARD_TYPE_WROVER_KIT ){

        direction = FALSE;
    }

    io_v_digital_write( IO_PIN_LED0, direction );
    io_v_digital_write( IO_PIN_LED1, direction );
    io_v_digital_write( IO_PIN_LED2, direction );
}


#define LED_BLUE        IO_PIN_LED1
#define LED_GREEN       IO_PIN_LED2
#define LED_RED         IO_PIN_LED0


void hal_status_led_v_init( void ){

    if( hal_io_b_is_board_type_known() ){

        reset_all();    
    }
}

void status_led_v_set( uint8_t state, uint8_t led ){

    if( !hal_io_b_is_board_type_known() ){

        return;
    }

    reset_all();

    bool direction = FALSE;

    // wrover kit has inverted LED drive
    if( ffs_u8_read_board_type() == BOARD_TYPE_WROVER_KIT ){

        direction = TRUE;
    }

 
    if( state == 0 ){

        return;
    }

    switch( led ){
        case STATUS_LED_BLUE:
            io_v_digital_write( LED_BLUE, direction );
            break;

        case STATUS_LED_GREEN:
            io_v_digital_write( LED_GREEN, direction );
            break;

        case STATUS_LED_RED:
            io_v_digital_write( LED_RED, direction );
            break;

        case STATUS_LED_YELLOW:
            io_v_digital_write( LED_GREEN, direction );
            io_v_digital_write( LED_RED, direction );
            break;

        case STATUS_LED_PURPLE:
            io_v_digital_write( LED_BLUE, direction );
            io_v_digital_write( LED_RED, direction );
            break;

        case STATUS_LED_TEAL:
            io_v_digital_write( LED_BLUE, direction );
            io_v_digital_write( LED_GREEN, direction );
            break;

        case STATUS_LED_WHITE:
            io_v_digital_write( LED_RED,  direction );
            io_v_digital_write( LED_GREEN, direction );
            io_v_digital_write( LED_BLUE, direction );
            break;

        default:
            break;
    }
}
