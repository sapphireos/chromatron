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

#include "status_led.h"


void reset_all( void ){

    #ifdef ENABLE_COPROCESSOR
    
    io_v_digital_write( IO_PIN_LED_RED, 1 );
    io_v_digital_write( IO_PIN_LED_GREEN, 1 );
    io_v_digital_write( IO_PIN_LED_BLUE, 1 );
    
    #else
    GPIO_OUTPUT_SET(2, 0);
    #endif
}

void hal_status_led_v_init( void ){

    reset_all();
}

void status_led_v_set( uint8_t state, uint8_t led ){

    reset_all();
 
    if( state == 0 ){

        return;
    }

    switch( led ){
        case STATUS_LED_BLUE:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_BLUE, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_GREEN:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_GREEN, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_RED:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_RED, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_YELLOW:
        #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_RED, 0 );
            io_v_digital_write( IO_PIN_LED_GREEN, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_PURPLE:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_RED, 0 );
            io_v_digital_write( IO_PIN_LED_BLUE, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_TEAL:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_BLUE, 0 );
            io_v_digital_write( IO_PIN_LED_GREEN, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        case STATUS_LED_WHITE:
            #ifdef ENABLE_COPROCESSOR
            io_v_digital_write( IO_PIN_LED_RED, 0 );
            io_v_digital_write( IO_PIN_LED_GREEN, 0 );
            io_v_digital_write( IO_PIN_LED_BLUE, 0 );
            #else
            GPIO_OUTPUT_SET(2, 1);
            #endif
            break;

        default:
            break;
    }
}
