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
#include "adc.h"
#include "timesync.h"

#include "status_led.h"



// rev 0.2
#define LED_RED_PORT    PORTC
#define LED_RED_PIN     4
#define LED_GREEN_PORT  PORTD
#define LED_GREEN_PIN   5
#define LED_BLUE_PORT   PORTD
#define LED_BLUE_PIN    4


void reset_all( void ){

    LED_GREEN_PORT.DIRSET = ( 1 << LED_GREEN_PIN );
    LED_BLUE_PORT.DIRSET = ( 1 << LED_BLUE_PIN );
    LED_RED_PORT.DIRSET = ( 1 << LED_RED_PIN );

    LED_GREEN_PORT.OUTSET = ( 1 << LED_GREEN_PIN );
    LED_BLUE_PORT.OUTSET = ( 1 << LED_BLUE_PIN );
    LED_RED_PORT.OUTSET = ( 1 << LED_RED_PIN );
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
            LED_BLUE_PORT.OUTCLR = ( 1 << LED_BLUE_PIN );
            break;

        case STATUS_LED_GREEN:
            LED_GREEN_PORT.OUTCLR = ( 1 << LED_GREEN_PIN );
            break;

        case STATUS_LED_RED:
            LED_RED_PORT.OUTCLR = ( 1 << LED_RED_PIN );
            break;

        case STATUS_LED_YELLOW:
            LED_GREEN_PORT.OUTCLR = ( 1 << LED_GREEN_PIN );
            LED_RED_PORT.OUTCLR = ( 1 << LED_RED_PIN );
            break;

        case STATUS_LED_PURPLE:
            LED_BLUE_PORT.OUTCLR = ( 1 << LED_BLUE_PIN );
            LED_RED_PORT.OUTCLR = ( 1 << LED_RED_PIN );
            break;

        case STATUS_LED_TEAL:
            LED_GREEN_PORT.OUTCLR = ( 1 << LED_GREEN_PIN );
            LED_BLUE_PORT.OUTCLR = ( 1 << LED_BLUE_PIN );
            break;

        case STATUS_LED_WHITE:
            LED_GREEN_PORT.OUTCLR = ( 1 << LED_GREEN_PIN );
            LED_RED_PORT.OUTCLR = ( 1 << LED_RED_PIN );
            LED_BLUE_PORT.OUTCLR = ( 1 << LED_BLUE_PIN );
            break;

        default:
            break;
    }
}
