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
#include "ffs_fw.h"

#ifdef AVR
#include "hal_esp8266.h"
#endif

static coproc_hdr_t rx_hdr;
static uint8_t rx_buf[COPROC_BUF_SIZE];

static bool sync;

static void send_block( uint8_t data[COPROC_BLOCK_LEN] ){

	coproc_block_t block;
	
	block.data[0] = data[0];
	block.data[1] = data[1];
	block.data[2] = data[2];
	block.data[3] = data[3];

	block.parity[0] = 0xff;
	block.parity[1] = 0xff;
	block.parity[2] = 0xff;

	#ifdef ESP8266
	usart_v_send_data( UART_CHANNEL, (uint8_t *)&block, sizeof(block) );
	#else
	hal_wifi_v_usart_send_data( (uint8_t *)&block, sizeof(block) );
	#endif
}

static void receive_block( uint8_t data[COPROC_BLOCK_LEN] ){

	coproc_block_t block;
	uint8_t *rx_data = (uint8_t *)&block;
	uint8_t len = sizeof(block);

	#ifdef ESP8266
	while( len > 0 ){

		// wait for data
		while( usart_u8_bytes_available( UART_CHANNEL ) == 0 );

		*rx_data++ = usart_i16_get_byte( UART_CHANNEL );
		len--;
	}
	#else
	hal_wifi_i8_usart_receive( rx_data, len, 10000000 );
	#endif

	data[0] = block.data[0];
	data[1] = block.data[1];
	data[2] = block.data[2];
	data[3] = block.data[3];
}

static void flush( void ){

	while( usart_i16_get_byte( UART_CHANNEL ) >= 0 );
}


void coproc_v_sync( void ){

	flush();

	int16_t byte = -1;

	while( byte != COPROC_SYNC ){

		usart_v_send_byte( UART_CHANNEL, COPROC_SYNC );

		uint32_t start_time = tmr_u32_get_system_time_ms();

		do{

			byte = usart_i16_get_byte( UART_CHANNEL );

		} while( ( byte != COPROC_SYNC ) &&
			     ( tmr_u32_elapsed_time_ms( start_time ) < 50 ) );
	}

	sync = TRUE;
}

void coproc_v_parity_check( coproc_block_t *block ){

}

void coproc_v_parity_generate( coproc_block_t *block ){

}


static uint32_t fw_addr;

// process a message
// assumes CRC is valid
void coproc_v_dispatch( 
    coproc_hdr_t *hdr, 
    const uint8_t *data,
    uint8_t *response_len,
    uint8_t response[COPROC_BUF_SIZE] ){

    uint8_t len = hdr->length;
    int32_t *params = (int32_t *)data;
    int32_t retval = 0;

    if( hdr->opcode == OPCODE_TEST ){

        memcpy( response, data, len );
        *response_len = len;
    }
    else if( hdr->opcode == OPCODE_IO_SET_MODE ){

		io_v_set_mode( params[0], params[1] );
	}
	else if( hdr->opcode == OPCODE_IO_GET_MODE ){

		retval = io_u8_get_mode( params[0] );
		memcpy( response, &retval, sizeof(retval) );
	}
	else if( hdr->opcode == OPCODE_IO_DIGITAL_WRITE ){

		io_v_digital_write( params[0], params[1] );
	}
	else if( hdr->opcode == OPCODE_IO_DIGITAL_READ ){

		retval = io_b_digital_read( params[0] );
		memcpy( response, &retval, sizeof(retval) );
	}
	else if( hdr->opcode == OPCODE_IO_READ_ADC ){

		retval = adc_u16_read_mv( params[0] );
		memcpy( response, &retval, sizeof(retval) );
	}
	else if( hdr->opcode == OPCODE_IO_FW_CRC ){

		retval = ffs_fw_u16_crc();
		memcpy( response, &retval, sizeof(retval) );
	}
	else if( hdr->opcode == OPCODE_IO_FW_ERASE ){
			
		// immediate (non threaded) erase of main fw partition
		ffs_fw_v_erase( 0, TRUE );
		fw_addr = 0;
	}
	else if( hdr->opcode == OPCODE_IO_FW_LOAD ){
		
		ffs_fw_i32_write( 0, fw_addr, data, len );
		fw_addr += len;
	}	
	else if( hdr->opcode == OPCODE_IO_FW_BOOTLOAD ){
		
		sys_v_reboot_delay( SYS_REBOOT_LOADFW );
	}	
    else{

        ASSERT( FALSE );
    }
}




uint8_t coproc_u8_issue( 
	uint8_t opcode, 
	uint8_t *data, 
	uint8_t len ){

	if( !sync ){

		return 0;
	}

	coproc_hdr_t hdr;

	hdr.sof     = COPROC_SOF;
	hdr.opcode 	= opcode;
	hdr.length 	= len;
	hdr.padding = 0;

	send_block( (uint8_t *)&hdr );

	while( len > 0 ){

		send_block( data );

		data += COPROC_BLOCK_LEN;
		len -= COPROC_BLOCK_LEN;
	}

	memset( &hdr, 0, sizeof(hdr) );

	// wait for response
	// note there is no timeout.
	// if the coprocessor does not respond, the system is broken.

	// wait for header
	receive_block( (uint8_t *)&hdr );
	
	ASSERT( rx_hdr.length < sizeof(rx_buf) );
		
	data = rx_buf;
	len = rx_hdr.length;
	while( len > 0 ){

		receive_block( data );

		data += COPROC_BLOCK_LEN;
		len -= COPROC_BLOCK_LEN;
	}

	return rx_hdr.length;
}


void coproc_v_test( void ){

	uint8_t response_len;
	uint8_t *response = rx_buf;

	uint8_t data[4];
	data[0] = 1;
	data[1] = 2;
	data[2] = 3;
	data[3] = 4;

	response_len = coproc_u8_issue( OPCODE_TEST, data, sizeof(data) );

	ASSERT( response_len == sizeof(data) );
	ASSERT( response[0] == 1 );
	ASSERT( response[1] == 2 );
	ASSERT( response[2] == 3 );
	ASSERT( response[3] == 4 );
}

uint16_t coproc_u16_fw_crc( void ){

	return coproc_i32_call0( OPCODE_IO_FW_CRC );
}

void coproc_v_fw_erase( void ){
	
	coproc_i32_call0( OPCODE_IO_FW_ERASE );	
}

void coproc_v_fw_load( uint8_t *data, uint32_t len ){

	// this will load an entire image

	// check CRC
	uint16_t remote_crc = coproc_u16_fw_crc();
	uint16_t local_crc = crc_u16_block( data, len - sizeof(uint16_t) );

	if( remote_crc == local_crc ){

		return;
	}

	sys_v_wdt_reset();

	coproc_v_fw_erase();

	while( len > 0 ){

		uint8_t copy_len = 32;
		if( copy_len > len ){

			copy_len = len;
		}

		coproc_u8_issue( OPCODE_IO_FW_LOAD, data, copy_len );
		data += copy_len;
		len -= copy_len;

		sys_v_wdt_reset();
	}

	sys_v_wdt_reset();

	coproc_v_fw_bootload();

	// the coprocessor will reset the ESP8266 at this point.
}

void coproc_v_fw_bootload( void ){

	coproc_i32_call0( OPCODE_IO_FW_BOOTLOAD );	
}


int32_t coproc_i32_call0( uint8_t opcode ){

	int32_t retval = 0;

	coproc_u8_issue( opcode, 0, 0 );

	retval = *(int32_t *)rx_buf;

	return retval;
}

int32_t coproc_i32_call1( uint8_t opcode, int32_t param0 ){

	int32_t retval = 0;

	int32_t param_buf[1];
	param_buf[0] = param0;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)rx_buf;

	return retval;
}

int32_t coproc_i32_call2( uint8_t opcode, int32_t param0, int32_t param1 ){

	int32_t retval = 0;

	int32_t param_buf[2];
	param_buf[0] = param0;
	param_buf[1] = param1;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)rx_buf;

	return retval;
}

int32_t coproc_i32_call3( uint8_t opcode, int32_t param0, int32_t param1, int32_t param2 ){

	int32_t retval = 0;


	int32_t param_buf[3];
	param_buf[0] = param0;
	param_buf[1] = param1;
	param_buf[2] = param2;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)rx_buf;

	return retval;
}

