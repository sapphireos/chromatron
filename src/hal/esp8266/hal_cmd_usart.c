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

#include "cmd_usart.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

void cmd_usart_v_set_baud( baud_t baud ){

}

bool cmd_usart_b_received_char( void ){

    return cmd_usart_u16_rx_size() > 0;
}

void cmd_usart_v_send_char( uint8_t data ){

    #ifdef ENABLE_COPROCESSOR
    coproc_i32_call1( OPCODE_IO_CMD_SEND_CHAR, data );
    #endif
}

void cmd_usart_v_send_data( const uint8_t *data, uint16_t len ){

    #ifdef ENABLE_COPROCESSOR
    while( len > 0 ){

        cmd_usart_v_send_char( *data );

        data++;
        len--;
    }
    #endif
}


int16_t cmd_usart_i16_get_char( void ){

    if( cmd_usart_u16_rx_size() == 0 ){

        return -1;
    }

    #ifdef ENABLE_COPROCESSOR
    return coproc_i32_call0( OPCODE_IO_CMD_GET_CHAR );
    #endif

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

    #ifdef ENABLE_COPROCESSOR
    return coproc_i32_call0( OPCODE_IO_CMD_RX_SIZE );
    #endif

    return 0;
}

void cmd_usart_v_flush( void ){

    #ifdef ENABLE_COPROCESSOR
    coproc_i32_call0( OPCODE_IO_CMD_FLUSH );
    #endif
}

void hal_cmd_usart_v_init( void ){

    
}

