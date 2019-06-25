/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef _USART_FIFO_H
#define _USART_FIFO_H

typedef struct{
	volatile uint8_t *buf;
	uint16_t len;
	
	uint16_t ins;
	uint16_t ext;
	uint16_t count;
} usart_fifo_t;


void usart_fifo_v_init( volatile usart_fifo_t *fifo, volatile uint8_t *buf, uint16_t len );
uint16_t usart_fifo_u16_get_count( volatile usart_fifo_t *fifo );
int8_t usart_fifo_i8_insert( volatile usart_fifo_t *fifo, uint8_t data );
int16_t usart_fifo_i16_extract( volatile usart_fifo_t *fifo );


#endif
