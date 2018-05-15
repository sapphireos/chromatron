// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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
#include "timers.h"
#include "hal_usart.h"


// tables for CLK2X = 0

static const PROGMEM uint8_t bsel_table[] = {
    12,
    12, // 4800
    12,
    138,
    12,
    137,
    12,
    135,
    12,
    131,
    123,
    7, // 250k
    107,
    3, // 500k
    1, // 1M
    0, // 2M
    103, // 74880
};

static const PROGMEM int8_t bscale_table[] = {
    6,
    5, // 4800 
    4,
    0,
    3,
    -1,
    2,
    -2,
    1,
    -3,
    -4,
    0, // 250k
    -5,
    0, // 500k
    0, // 1M
    0, // 2M
    -2, // 74880
};


void usart_v_init( USART_t *usart ){

    COMPILER_ASSERT( cnt_of_array(bsel_table) == cnt_of_array(bscale_table) );

    usart->CTRLA = 0;
    usart->CTRLB = USART_RXEN_bm | USART_TXEN_bm;
    usart->CTRLC = 0x03; // datasheet reset default
}

void usart_v_set_baud( USART_t *usart, baud_t8 baud ){

    ASSERT( baud < cnt_of_array(bsel_table) );

    uint8_t bsel = pgm_read_byte( &bsel_table[baud] );
    int8_t bscale = pgm_read_byte( &bscale_table[baud] );

    usart->BAUDCTRLA = bsel & 0xff;
    usart->BAUDCTRLB = bscale << USART_BSCALE_gp;
}

void usart_v_set_double_speed( USART_t *usart, bool clk2x ){

    if( clk2x ){

        usart->CTRLB |= USART_CLK2X_bm;
    }
    else{

        usart->CTRLB &= ~USART_CLK2X_bm;
    }
}


void usart_v_send_byte( USART_t *usart, uint8_t data ){

    SAFE_BUSY_WAIT( ( usart->STATUS & USART_DREIF_bm ) == 0 );
    usart->DATA = data;
}

void usart_v_send_data( USART_t *usart, const uint8_t *data, uint16_t len ){

    while( len > 0 ){

        SAFE_BUSY_WAIT( ( usart->STATUS & USART_DREIF_bm ) == 0 );

        usart->DATA = *data;
        len--;
        data++;
    }
}

int16_t usart_i16_get_byte( USART_t *usart ){

    if( ( usart->STATUS & USART_RXCIF_bm ) == 0 ){

        return -1;
    }

    return usart->DATA;
}

uint8_t usart_u8_bytes_available( USART_t *usart ){

    if( ( usart->STATUS & USART_RXCIF_bm ) == 0 ){

        return 0;
    }

    return 1;
}
