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

#include "hal_usb.h"
#include "cmd_usart.h"


void cmd_usart_v_set_baud( baud_t baud ){

}

bool cmd_usart_b_received_char( void ){

    return ( UCSR0A & ( 1 << RXC0 ) );
}

void cmd_usart_v_send_char( uint8_t data ){
    
    // wait for data register empty
    while( ( UCSR0A & ( 1 << UDRE0 ) ) == 0 );
    
    UDR0 = data;
}

void cmd_usart_v_send_data( const uint8_t *data, uint16_t len ){

    while( len > 0 ){
        
        cmd_usart_v_send_char( *data );

        data++;
        len--;
    }
}


int16_t cmd_usart_i16_get_char( void ){

    if( UCSR0A & ( 1 << RXC0 ) ){
    
        return UDR0;
    }

    return -1;
}

uint8_t cmd_usart_u8_get_data( uint8_t *data, uint8_t len ){
    
    uint8_t rx_size = cmd_usart_u16_rx_size();

    if( len > rx_size ){

        len = rx_size;
    }

    rx_size = len;

    while( len > 0 ){

        *data = cmd_usart_i16_get_char();
        data++;

        len--;
    }

    return rx_size;
}

uint16_t cmd_usart_u16_rx_size( void ){

    if( UCSR0A & ( 1 << RXC0 ) ){

        return 1;
    }

    return 0;
}

void cmd_usart_v_flush( void ){

}


void hal_cmd_usart_v_init( void ){

    UCSR0A = ( 1 << U2X0 ); // enable double speed mode
    UCSR0B = ( 1 << RXEN0 ) | ( 1 << TXEN0 ); // receive interrupt enabled, rx and tx enabled
    UCSR0C = ( 1 << UCSZ01 ) | ( 1 << UCSZ00 ); // select 8 bit characters

    // 115200 bps
    UBRR0 = 16;
}
