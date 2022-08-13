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

#include "ets_sys.h"
#include "osapi.h"
#include "osapi.h"
#include "mem.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "driver/uart_register.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif


// UartDev is defined and initialized in rom code.
extern UartDevice    UartDev;

static uint8_t rx_fifo_len(void){

    return (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;
} 

static void usart_v_config( uint8_t channel ){

    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

    uart_div_modify(channel, UART_CLK_FREQ / (UartDev.baut_rate));//SET BAUDRATE

    WRITE_PERI_REG(UART_CONF0(channel), ((UartDev.exist_parity & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S) //SET BIT AND PARITY MODE
                   | ((UartDev.parity & UART_PARITY_M)  << UART_PARITY_S)
                   | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                   | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));

    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(channel), UART_RXFIFO_RST | UART_TXFIFO_RST);    //RESET FIFO
    CLEAR_PERI_REG_MASK(UART_CONF0(channel), UART_RXFIFO_RST | UART_TXFIFO_RST);

    if (channel == UART0) {
        //set rx fifo trigger
        WRITE_PERI_REG(UART_CONF1(channel),
                       ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |

                       (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                       UART_RX_TOUT_EN |
                       ((0x10 & UART_TXFIFO_EMPTY_THRHD) << UART_TXFIFO_EMPTY_THRHD_S)); //wjl
        SET_PERI_REG_MASK(UART_INT_ENA(channel), UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA);
    } 
    else {
        WRITE_PERI_REG(UART_CONF1(channel), ((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S)); //TrigLvl default val == 1
    }

    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(channel), 0xffff);
}

void usart_v_init( uint8_t channel ){
    
    if( channel == 0 ){
        usart_v_config( channel );
    }
    #ifdef ENABLE_COPROCESSOR
    else if( channel == USER_USART ){

        coproc_i32_call0( OPCODE_IO_USART_INIT );
    }
    #endif
}

void usart_v_set_baud( uint8_t channel, baud_t baud ){

    if( channel == 0 ){
        UartDev.baut_rate = baud;
        usart_v_config( channel );
    }
    #ifdef ENABLE_COPROCESSOR
    else if( channel == USER_USART ){

        coproc_i32_call1( OPCODE_IO_USART_SET_BAUD, baud );
    }
    #endif
}

void usart_v_send_byte( uint8_t channel, uint8_t data ){

    if( channel == 0 ){

        while (true) {
            uint32_t fifo_cnt = READ_PERI_REG(UART_STATUS(channel)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);

            if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
                break;
            }
        }

        WRITE_PERI_REG(UART_FIFO(channel), data);
    }
    #ifdef ENABLE_COPROCESSOR
    else if( channel == USER_USART ){

        coproc_i32_call1( OPCODE_IO_USART_SEND_CHAR, data );
    }
    #endif
}

void usart_v_send_data( uint8_t channel, const uint8_t *data, uint16_t len ){

    while( len > 0 ){

        usart_v_send_byte( 0, *data );
        data++;
        len--;
    }
}

int16_t usart_i16_get_byte( uint8_t channel ){

    if( channel == 0 ){
    
    	if( rx_fifo_len() == 0 ){

    		return -1;
    	}

    	return READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
    }
    #ifdef ENABLE_COPROCESSOR
    else if( channel == USER_USART ){

        return coproc_i32_call0( OPCODE_IO_USART_GET_CHAR );
    }
    #endif

    return -1;
}

uint8_t usart_u8_bytes_available( uint8_t channel ){

    if( channel == 0 ){
        return rx_fifo_len();
    }
    #ifdef ENABLE_COPROCESSOR
    else if( channel == USER_USART ){

        return coproc_i32_call0( OPCODE_IO_USART_RX_SIZE );
    }
    #endif

    return 0;
}

