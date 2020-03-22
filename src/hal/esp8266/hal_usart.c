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

#include "system.h"
#include "timers.h"


#include "hal_usart.h"

static baud_t baud0;
static baud_t baud1;

static uint8_t rx_buf[256];

PT_THREAD( uart_rx_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );  

    while(1){

    	THREAD_YIELD( pt );
        	
        uint8_t temp;
        uint16_t len = rx_buff_deq( &temp, sizeof(temp) );

        if( len == 0 ){

        	continue;
        }
        
        trace_printf("0x%02x\n", temp);
    }

PT_END( pt );
}

void usart_v_init( uint8_t channel ){

	thread_t_create_critical( THREAD_CAST(uart_rx_thread),
                              PSTR("uart_rx"),
                              0,
                              0 );

}

void usart_v_set_baud( uint8_t channel, baud_t baud ){

	if( channel == 0 ){

		baud0 = baud;
	}
	else{

		baud1 = baud;
	}

	uart_init( baud0, baud1 );
}

void usart_v_send_byte( uint8_t channel, uint8_t data ){

	uart_tx_one_char( channel, data );
}

void usart_v_send_data( uint8_t channel, const uint8_t *data, uint16_t len ){

	while( len > 0 ){

		usart_v_send_byte( 0, *data );
		data++;
		len--;
	}
}

int16_t usart_i16_get_byte( uint8_t channel ){

	return -1;
}

uint8_t usart_u8_bytes_available( uint8_t channel ){

	return 0;
}

