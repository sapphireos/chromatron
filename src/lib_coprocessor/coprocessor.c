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

static void receive_block( uint8_t *data, uint8_t len ){

	while( len > 0 ){

		*data++ = usart_i16_get_byte();
		len--;
	}
}

static uint8_t rx_buf[128];


uint16_t coproc_v_crc( coproc_hdr_t *hdr ){

	return crc_u16_block( (uint8_t *)hdr, sizeof(coproc_hdr_t) + hdr->length );
}

// process a message
// assumes CRC is valid
void coproc_v_dispatch( coproc_hdr_t *hdr ){

	if( hdr->opcode == OPCODE_TEST ){


	}



}


int16_t coproc_i16_issue( uint8_t opcode, uint8_t *data, uint8_t len ){

	coproc_hdr_t hdr;

	hdr.opcode 	= opcode;
	hdr.length 	= len;
	hdr.padding = 0;

	uint16_t crc = crc_u16_start();
	crc = crc_u16_partial_block( crc, (uint8_t *)&hdr, sizeof(hdr) );
	crc = crc_u16_partial_block( crc, data, len );
	crc = crc_u16_finish( crc );

	usart_v_send_byte( 0, COPROC_SOF );
	usart_v_send_data( 0, (uint8_t *)&hdr, sizeof(hdr) );
	usart_v_send_data( 0, data, len );
	usart_v_send_data( 0, (uint8_t *)&crc, sizeof(crc) );

	// wait for response
	// note there is no timeout.
	// if the coprocessor does not respond, the system is broken.

	// synchronize to SOF
	while( usart_i16_get_byte( 0 ) != COPROC_SOF );

	// wait for header
	while( usart_u8_bytes_available( 0 ) < sizeof(hdr) );

	// receive header
	receive_block( (uint8_t *)&hdr, sizeof(hdr) );

	ASSERT( hdr.length < sizeof(rx_buf) );
	
	receive_block( rx_buf, hdr.length );	

	return hdr.length;
}
















