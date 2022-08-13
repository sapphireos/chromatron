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
#include "timers.h"
#include "hal_usart.h"


// // tables for CLK2X = 0

// static const PROGMEM uint8_t bsel_table[] = {
//     12,
//     12, // 4800
//     12,
//     138,
//     12,
//     137,
//     12,
//     135,
//     12,
//     131,
//     123,
//     7, // 250k
//     107,
//     3, // 500k
//     1, // 1M
//     0, // 2M
//     103, // 74880
// };

// static const PROGMEM int8_t bscale_table[] = {
//     6,
//     5, // 4800 
//     4,
//     0,
//     3,
//     -1,
//     2,
//     -2,
//     1,
//     -3,
//     -4,
//     0, // 250k
//     -5,
//     0, // 500k
//     0, // 1M
//     0, // 2M
//     -2, // 74880
// };

// static USART_t* get_channel( uint8_t channel ){

//     if( channel == USER_USART ){

//         return &USER_USART_CH;    
//     }
//     else if( channel == WIFI_USART ){

//         return &WIFI_USART_CH;    
//     }
//     else if( channel == PIXEL_USART ){

//         return &PIXEL_DATA_PORT;
//     }

//     ASSERT( FALSE );

//     return 0;
// }

void usart_v_init( uint8_t channel ){

    // USART_t *usart = get_channel( channel );

    // if( channel == USER_USART ){

    //     io_v_set_mode( IO_PIN_2_TXD, IO_MODE_OUTPUT );
    //     io_v_set_mode( IO_PIN_3_RXD, IO_MODE_INPUT );
    // }

    // COMPILER_ASSERT( cnt_of_array(bsel_table) == cnt_of_array(bscale_table) );

    // usart->CTRLA = 0;
    // usart->CTRLB = USART_RXEN_bm | USART_TXEN_bm;
    // usart->CTRLC = 0x03; // datasheet reset default
}

void usart_v_set_baud( uint8_t channel, baud_t baud ){

    // USART_t *usart = get_channel( channel );

    // ASSERT( baud < cnt_of_array(bsel_table) );

    // uint8_t bsel = pgm_read_byte( &bsel_table[baud] );
    // int8_t bscale = pgm_read_byte( &bscale_table[baud] );

    // usart->BAUDCTRLA = bsel & 0xff;
    // usart->BAUDCTRLB = bscale << USART_BSCALE_gp;
}

void usart_v_set_double_speed( uint8_t channel, bool clk2x ){

    // USART_t *usart = get_channel( channel );

    // if( clk2x ){

    //     usart->CTRLB |= USART_CLK2X_bm;
    // }
    // else{

    //     usart->CTRLB &= ~USART_CLK2X_bm;
    // }
}


void usart_v_send_byte( uint8_t channel, uint8_t data ){

    // USART_t *usart = get_channel( channel );

    // BUSY_WAIT( ( usart->STATUS & USART_DREIF_bm ) == 0 );
    // usart->DATA = data;
}

void usart_v_send_data( uint8_t channel, const uint8_t *data, uint16_t len ){

    // USART_t *usart = get_channel( channel );

    // while( len > 0 ){

    //     BUSY_WAIT( ( usart->STATUS & USART_DREIF_bm ) == 0 );

    //     usart->DATA = *data;
    //     len--;
    //     data++;
    // }
}

int16_t usart_i16_get_byte( uint8_t channel ){

//     USART_t *usart = get_channel( channel );

//     if( ( usart->STATUS & USART_RXCIF_bm ) == 0 ){

//         return -1;
//     }

//     return usart->DATA;
}

uint8_t usart_u8_bytes_available( uint8_t channel ){

    // USART_t *usart = get_channel( channel );

    // if( ( usart->STATUS & USART_RXCIF_bm ) == 0 ){

    //     return 0;
    // }

    // return 1;
}
