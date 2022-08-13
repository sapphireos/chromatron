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
 
#include "cpu.h"
#include "hal_io.h"
#include "system.h"

static bool pressed;

bool button_b_is_pressed( void ){
    
    return pressed;
}

void button_v_init( void ){

    io_v_set_mode( IO_PIN_6_DAC0, IO_MODE_INPUT_PULLUP );
    io_v_set_mode( IO_PIN_7_DAC1, IO_MODE_INPUT_PULLUP );

    // check boot "button"

    // verify both pins are high
    if( io_b_digital_read( IO_PIN_6_DAC0 ) == 0 ){

        goto done;
    }

    if( io_b_digital_read( IO_PIN_7_DAC1 ) == 0 ){

        goto done;
    }

    // pull DAC0 down
    io_v_set_mode( IO_PIN_6_DAC0, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_digital_write( IO_PIN_6_DAC0, 0 );

    // check that DAC1 also came down
    if( io_b_digital_read( IO_PIN_7_DAC1 ) != 0 ){

        goto done;
    }

    io_v_set_mode( IO_PIN_6_DAC0, IO_MODE_INPUT_PULLUP );

    // pull DAC1 down
    io_v_set_mode( IO_PIN_7_DAC1, IO_MODE_OUTPUT_OPEN_DRAIN );
    io_v_digital_write( IO_PIN_7_DAC1, 0 );

    // check that DAC0 also came down
    if( io_b_digital_read( IO_PIN_6_DAC0 ) != 0 ){

        goto done;
    }

    pressed = TRUE;

done:

    // reset IO to input
    io_v_set_mode( IO_PIN_6_DAC0, IO_MODE_INPUT );
    io_v_set_mode( IO_PIN_7_DAC1, IO_MODE_INPUT );

}   



