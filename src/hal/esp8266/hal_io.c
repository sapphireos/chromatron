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

#include "system.h"
#include "target.h"

#include "hal_io.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

void io_v_init( void ){


}

uint8_t io_u8_get_board_rev( void ){

    return 0;
}


void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

	#ifdef ENABLE_COPROCESSOR
	coproc_i32_call2( OPCODE_IO_SET_MODE, pin, mode );
	#endif
}


io_mode_t8 io_u8_get_mode( uint8_t pin ){
	
	#ifdef ENABLE_COPROCESSOR
	return coproc_i32_call1( OPCODE_IO_GET_MODE, pin );
	#endif
}

void io_v_digital_write( uint8_t pin, bool state ){

	#ifdef ENABLE_COPROCESSOR
	coproc_i32_call2( OPCODE_IO_DIGITAL_WRITE, pin, state );
	#endif
}

bool io_b_digital_read( uint8_t pin ){
    	
    #ifdef ENABLE_COPROCESSOR
	return coproc_i32_call1( OPCODE_IO_DIGITAL_READ, pin );
	#endif

    return FALSE;
}

bool io_b_button_down( void ){

    return FALSE;
}

void io_v_disable_jtag( void ){

}

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode )
{

}

void io_v_disable_interrupt( uint8_t int_number )
{

}

void io_v_set_esp_led( bool state ){

    if( state ){

        GPIO_OUTPUT_SET(2, 0);    
    }
    else{

        GPIO_OUTPUT_SET(2, 1);    
    }
}