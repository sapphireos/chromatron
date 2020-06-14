/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#include "hal_cmd_usart.h"

#include "driver/uart.h"


void cmd_usart_v_set_baud( baud_t baud ){

}

bool cmd_usart_b_received_char( void ){

    return cmd_usart_u16_rx_size() > 0;
}

void cmd_usart_v_send_char( uint8_t data ){

    uart_write_bytes(HAL_CMD_UART, (const char *) &data, 1);
}

void cmd_usart_v_send_data( const uint8_t *data, uint16_t len ){

    uart_write_bytes(HAL_CMD_UART, (const char *) data, len);
}


int16_t cmd_usart_i16_get_char( void ){

    if( cmd_usart_u16_rx_size() == 0 ){

        return -1;
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

    return 0;
}

void cmd_usart_v_flush( void ){

}


void hal_cmd_usart_v_init( void ){

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(HAL_CMD_UART, &uart_config);
    uart_set_pin(HAL_CMD_UART, HAL_CMD_TXD, HAL_CMD_RXD, -1, -1);
    uart_driver_install(HAL_CMD_UART, HAL_CMD_RX_BUF_SIZE, 0, 0, NULL, 0);
}



    