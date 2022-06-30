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


#include "system.h"
#include "status_led.h"

void ldr_v_set_green_led( void ){

    status_led_v_set( 1, STATUS_LED_GREEN );
}

void ldr_v_clear_green_led( void ){

    status_led_v_set( 0, STATUS_LED_GREEN );
}

void ldr_v_set_yellow_led( void ){

    status_led_v_set( 1, STATUS_LED_YELLOW );
}

void ldr_v_clear_yellow_led( void ){

    status_led_v_set( 0, STATUS_LED_YELLOW );
}

void ldr_v_set_red_led( void ){

    status_led_v_set( 1, STATUS_LED_RED );
}

void ldr_v_clear_red_led( void ){

    status_led_v_set( 0, STATUS_LED_RED );
}
