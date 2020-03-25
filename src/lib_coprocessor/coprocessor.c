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

#include "sapphire.h"

#include "coprocessor.h"

#include "hal_usart.h"

static coproc_hdr_t rx_hdr;
static uint8_t rx_buf[COPROC_BUF_SIZE];


static void receive_block( uint8_t *data, uint8_t len ){

	while( len > 0 ){

		*data++ = usart_i16_get_byte( UART_CHANNEL );
		len--;
	}
}


// uint16_t coproc_v_crc( coproc_hdr_t *hdr ){

// 	return crc_u16_block( (uint8_t *)hdr, sizeof(coproc_hdr_t) + hdr->length );
// }

// process a message
// assumes CRC is valid
void coproc_v_dispatch( coproc_hdr_t *hdr ){

	coproc_hdr_t reply_hdr;

	reply_hdr.opcode 	= hdr->opcode;
	reply_hdr.length 	= 0;
	reply_hdr.padding 	= 0;


	uint8_t len = hdr->length;
	uint8_t *data = (uint8_t *)( hdr + 1 );

	uint8_t tx_buf[COPROC_BUF_SIZE];	

	if( hdr->opcode == OPCODE_TEST ){

		memcpy( tx_buf, data, len );
		reply_hdr.length = len;
	}
	else{

		ASSERT( FALSE );
	}

	usart_v_send_byte( UART_CHANNEL, COPROC_SOF );
	usart_v_send_data( UART_CHANNEL, (uint8_t *)&reply_hdr, sizeof(reply_hdr) );
	usart_v_send_data( UART_CHANNEL, tx_buf, reply_hdr.length );	
}



uint8_t coproc_u8_issue( 
	uint8_t opcode, 
	uint8_t *data, 
	uint8_t len,
	uint8_t **rx_data ){

	coproc_hdr_t hdr;

	hdr.opcode 	= opcode;
	hdr.length 	= len;
	hdr.padding = 0;

	usart_v_send_byte( UART_CHANNEL, COPROC_SOF );
	usart_v_send_data( UART_CHANNEL, (uint8_t *)&hdr, sizeof(hdr) );
	usart_v_send_data( UART_CHANNEL, data, len );


	// wait for response
	// note there is no timeout.
	// if the coprocessor does not respond, the system is broken.

	// synchronize to SOF
	while( usart_i16_get_byte( UART_CHANNEL ) != COPROC_SOF );

	// wait for header
	while( usart_u8_bytes_available( UART_CHANNEL ) < sizeof(hdr) );

	// receive header
	receive_block( (uint8_t *)&rx_hdr, sizeof(rx_hdr) );

	ASSERT( rx_hdr.length < sizeof(rx_buf) );
	
	receive_block( &rx_buf[sizeof(rx_hdr)], rx_hdr.length );	

	*rx_data = rx_buf;

	return rx_hdr.length;
}


void coproc_v_test( void ){

	uint8_t response_len;
	uint8_t *response;

	uint8_t data[4];
	data[0] = 1;
	data[1] = 2;
	data[2] = 3;
	data[3] = 4;

	response_len = coproc_u8_issue( OPCODE_TEST, data, sizeof(data), &response );

	ASSERT( response_len == sizeof(data) );
	ASSERT( response[0] == 1 );
	ASSERT( response[1] == 1 );
	ASSERT( response[2] == 1 );
	ASSERT( response[3] == 1 );
}

