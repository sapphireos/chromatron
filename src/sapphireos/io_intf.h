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

#ifndef _IO_INTF_H
#define _IO_INTF_H

#include "system.h"

// hack around Atmel USB stack defining these.
#ifdef HIGH
#undef HIGH
#endif
#ifdef LOW
#undef LOW
#endif

#define HIGH ( TRUE )
#define LOW  ( FALSE )

typedef uint8_t io_mode_t8;
#define IO_MODE_INPUT               0
#define IO_MODE_INPUT_PULLUP        1
#define IO_MODE_INPUT_PULLDOWN      2
#define IO_MODE_OUTPUT              3
#define IO_MODE_OUTPUT_OPEN_DRAIN   4

typedef uint8_t io_int_mode_t8;
#define IO_INT_LOW              0
#define IO_INT_CHANGE           1
#define IO_INT_FALLING          2
#define IO_INT_RISING           3

typedef void (*io_int_handler_t)( void );

void io_v_init( void );
uint8_t io_u8_get_board_rev( void );

void io_v_set_mode( uint8_t pin, io_mode_t8 mode );
io_mode_t8 io_u8_get_mode( uint8_t pin );

void io_v_digital_write( uint8_t pin, bool state );
bool io_b_digital_read( uint8_t pin );

bool io_b_button_down( void );
void io_v_disable_jtag( void );

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode );

void io_v_disable_interrupt( uint8_t int_number );

#endif
