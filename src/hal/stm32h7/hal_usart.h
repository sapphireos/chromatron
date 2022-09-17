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

#define HAL_USART USART1

typedef UART_HandleTypeDef USART_t;


void usart_v_init( USART_t *usart );
void usart_v_set_baud( USART_t *usart, baud_t baud );
void usart_v_set_double_speed( USART_t *usart, bool clk2x );
void usart_v_send_byte( USART_t *usart, uint8_t data );
void usart_v_send_data( USART_t *usart, const uint8_t *data, uint16_t len );
int16_t usart_i16_get_byte( USART_t *usart );
uint8_t usart_u8_bytes_available( USART_t *usart );

#endif
