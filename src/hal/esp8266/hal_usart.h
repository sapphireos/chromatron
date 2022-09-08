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


#ifndef _HAL_USART_H
#define _HAL_USART_H

#include "cpu.h"
#include "usart_bauds.h"

#ifdef ENABLE_COPROCESSOR
#define USER_USART          1
#else
#define USER_USART          0
#endif


void usart_v_init( uint8_t channel );
void usart_v_set_baud( uint8_t channel, baud_t baud );
void usart_v_send_byte( uint8_t channel, uint8_t data );
void usart_v_send_data( uint8_t channel, const uint8_t *data, uint16_t len );
int16_t usart_i16_get_byte( uint8_t channel );
uint8_t usart_u8_bytes_available( uint8_t channel );
uint8_t usart_u8_get_bytes( uint8_t channel, uint8_t *ptr, uint8_t len );

#endif
