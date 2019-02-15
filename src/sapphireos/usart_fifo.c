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

#include "sapphire.h"


void usart_fifo_v_init( volatile usart_fifo_t *fifo, volatile uint8_t *buf, uint16_t len ){

	fifo->buf = buf;
	fifo->len = len;

	fifo->ins = 0;
	fifo->ext = 0;
	fifo->count = 0;
}


uint16_t usart_fifo_u16_get_count( volatile usart_fifo_t *fifo ){

	ATOMIC;
    uint16_t temp = fifo->count;
    END_ATOMIC;

    return temp;
}

int8_t usart_fifo_i8_insert( volatile usart_fifo_t *fifo, uint8_t data ){

	int8_t status = 0;

	ATOMIC;
	if( fifo->count >= fifo->len ){

        status = -1;
        goto end;
    }

    fifo->buf[fifo->ins] = data;
    fifo->ins++;

    if( fifo->ins >= fifo->len ){

        fifo->ins = 0;
    }

    fifo->count++;

end:
	END_ATOMIC;
    return status;
}

int16_t usart_fifo_i16_extract( volatile usart_fifo_t *fifo ){

	uint8_t temp = fifo->buf[fifo->ext];

    fifo->ext++;

    if( fifo->ext >= fifo->len ){

        fifo->ext = 0;
    }

	ATOMIC;

    fifo->count--;

	END_ATOMIC;

	return temp;
}


