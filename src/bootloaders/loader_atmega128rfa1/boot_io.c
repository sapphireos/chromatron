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


#include "system.h"
#include "hal_status_led.h"

void ldr_v_set_green_led( void ){

    LED_GREEN_PORT |= ( 1 << LED_GREEN_PIN );
}

void ldr_v_clear_green_led( void ){

    LED_GREEN_PORT &= ~( 1 << LED_GREEN_PIN );
}

void ldr_v_set_yellow_led( void ){

    LED_YELLOW_PORT |= ( 1 << LED_YELLOW_PIN );
}

void ldr_v_clear_yellow_led( void ){

    LED_YELLOW_PORT &= ~( 1 << LED_YELLOW_PIN );
}

void ldr_v_set_red_led( void ){

    LED_RED_PORT |= ( 1 << LED_RED_PIN );
}

void ldr_v_clear_red_led( void ){

    LED_RED_PORT &= ~( 1 << LED_RED_PIN );
}
