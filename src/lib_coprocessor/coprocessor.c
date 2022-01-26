// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "config.h"

#include "boot_data.h"
#include "coprocessor.h"

#include "hal_usart.h"
#include "ffs_fw.h"

#ifdef AVR
#include "hal_esp8266.h"
#endif

// bootloader shared memory
extern boot_data_t BOOTDATA boot_data;

static uint8_t buffer[COPROC_BUF_SIZE] __attribute__((aligned(4)));

static bool sync;

void coproc_v_send_block( uint8_t data[COPROC_BLOCK_LEN] ){

	coproc_block_t block;
	
	block.data[0] = data[0];
	block.data[1] = data[1];
	block.data[2] = data[2];
	block.data[3] = data[3];

	block.crc = crc_u16_block( data, COPROC_BLOCK_LEN ) & 0xff;
	
	#ifdef ESP8266
	usart_v_send_data( UART_CHANNEL, (uint8_t *)&block, sizeof(block) );
	#else
	hal_wifi_v_usart_send_data( (uint8_t *)&block, sizeof(block) );
	#endif
}

void coproc_v_receive_block( uint8_t data[COPROC_BLOCK_LEN] ){

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

	// error check.  fatal if this fails.
	if( ( crc_u16_block( data, COPROC_BLOCK_LEN ) & 0xff ) != block.crc ){

		// BOOM!
		while(1);
	}
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

	// send protocol version
	usart_v_send_byte( UART_CHANNEL, COPROC_VERSION );

	while( usart_u8_bytes_available( UART_CHANNEL ) == 0 );

	if( usart_i16_get_byte( UART_CHANNEL ) != COPROC_VERSION ){

		// lets hope this was a bus error and rebooting will recover
		sys_reboot();
	}

	sync = TRUE;
}


void coproc_v_init( void ){

	if( !sync ){

		return;
	}

	char coproc_firmware_version[FW_VER_LEN];
    memset( coproc_firmware_version, 0, FW_VER_LEN );
    coproc_v_fw_version( coproc_firmware_version );
    log_v_debug_P( PSTR("coproc ver: %s reset: %u"), coproc_firmware_version, coproc_i32_call0( OPCODE_GET_RESET_SOURCE ) );
}


int32_t coproc_i32_debug_print( char *s ){

	return coproc_i32_callv( OPCODE_DEBUG_PRINT, (uint8_t *)s, strnlen( s, COPROC_BUF_SIZE ) );
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

	coproc_v_send_block( (uint8_t *)&hdr );

	int16_t send_len = len;

	while( send_len > 0 ){

		coproc_v_send_block( data );

		data += COPROC_BLOCK_LEN;
		send_len -= COPROC_BLOCK_LEN;
	}

	memset( &hdr, 0, sizeof(hdr) );

	// wait for response
	// note there is no timeout.
	// if the coprocessor does not respond, the system is broken.

	// wait for header
	coproc_v_receive_block( (uint8_t *)&hdr );
	
	ASSERT( hdr.sof == COPROC_SOF );
	ASSERT( hdr.length < sizeof(buffer) );
		
	data = buffer;
	int16_t rx_len = hdr.length;
	while( rx_len > 0 ){

		coproc_v_receive_block( data );

		data += COPROC_BLOCK_LEN;
		rx_len -= COPROC_BLOCK_LEN;
	}

	return hdr.length;
}

// note this function should not return, the coprocessor will hit the reset line.
void coproc_v_reboot( void ){

	coproc_i32_call0( OPCODE_REBOOT );
}


void coproc_v_test( void ){

	uint8_t response_len;
	uint8_t *response = buffer;

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

void coproc_v_fw_version( char firmware_version[FW_VER_LEN] ){

	memset( firmware_version, 0, FW_VER_LEN );
	uint8_t response_len = coproc_u8_issue( OPCODE_FW_VERSION, 0, 0 );
	memcpy( firmware_version, buffer, response_len );
}

uint16_t coproc_u16_fw_crc( void ){

	return coproc_i32_call0( OPCODE_FW_CRC );
}

void coproc_v_fw_erase( void ){
	
	coproc_i32_call0( OPCODE_FW_ERASE );	
}

void coproc_v_fw_load( uint8_t *data, uint32_t len ){

	// this will load an entire image

	// check CRC
	uint16_t local_crc = crc_u16_block( data, len );

	if( local_crc != 0 ){

		// log_v_debug_P( PSTR("local coproc image crc fail: 0x%04x len: %u"), local_crc, len );

		return;
	}

	// check version
	char coproc_firmware_version[FW_VER_LEN];
	memset( coproc_firmware_version, 0, FW_VER_LEN );
    coproc_v_fw_version( coproc_firmware_version );

    char local_firmware_version[FW_VER_LEN];
    memset( local_firmware_version, 0, FW_VER_LEN );
    memcpy( local_firmware_version, data + COPROC_FW_INFO_ADDRESS + offsetof(fw_info_t, firmware_version), FW_VER_LEN );

    // log_v_debug_P( PSTR("coproc ver: %s"), coproc_firmware_version );
    // log_v_debug_P( PSTR("local  ver: %s"), local_firmware_version );

    if( memcmp( coproc_firmware_version, local_firmware_version, FW_VER_LEN ) == 0 ){

    	return;
    }

    log_v_debug_P( PSTR("loading new coprocessor firmware...") );

	sys_v_wdt_reset();

	coproc_v_fw_erase();

	while( len > 0 ){

		uint8_t copy_len = 32;
		if( copy_len > len ){

			copy_len = len;
		}

		coproc_u8_issue( OPCODE_FW_LOAD, data, copy_len );
		data += copy_len;
		len -= copy_len;

		sys_v_wdt_reset();
	}

	sys_v_wdt_reset();

	uint16_t coproc_crc = coproc_u16_fw_crc();

	log_v_debug_P( PSTR("coproc image crc: 0x%04x"), coproc_crc );

	if( coproc_crc != 0 ){

		ASSERT( FALSE );
	}

	// request coprocessor bootload
	coproc_v_fw_bootload();

	// the coprocessor will reset the ESP8266 at this point.
}

void coproc_v_fw_bootload( void ){

	// #ifdef ESP8266

	// // configure bootdata
	// boot_data.boot_mode = BOOT_MODE_REBOOT;
	// hal_cpu_v_store_bootdata(); // commit bootdata

	// #endif

	coproc_i32_call0( OPCODE_FW_BOOTLOAD );	

	// the coprocessor will reset the ESP8266 at this point.
}

void coproc_v_get_wifi( void ){

	uint8_t buf[WIFI_SSID_LEN + WIFI_PASS_LEN];
	memset( buf, 0, sizeof(buf) );

	cfg_i8_get( CFG_PARAM_WIFI_SSID, buf );

	if( buf[0] != 0 ){

		return;
	} 

	coproc_i32_callp( OPCODE_GET_WIFI, buf, sizeof(buf) );

	cfg_v_set( CFG_PARAM_WIFI_SSID, buf );
	cfg_v_set( CFG_PARAM_WIFI_PASSWORD, &buf[WIFI_SSID_LEN] );

	log_v_debug_P( PSTR("retrieved wifi config: %s"), buf );
}

int32_t coproc_i32_call0( uint8_t opcode ){

	int32_t retval = 0;

	coproc_u8_issue( opcode, 0, 0 );

	retval = *(int32_t *)buffer;

	return retval;
}

int32_t coproc_i32_call1( uint8_t opcode, int32_t param0 ){

	int32_t retval = 0;

	int32_t param_buf[1];
	param_buf[0] = param0;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)buffer;

	return retval;
}

int32_t coproc_i32_call2( uint8_t opcode, int32_t param0, int32_t param1 ){

	int32_t retval = 0;

	int32_t param_buf[2];
	param_buf[0] = param0;
	param_buf[1] = param1;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)buffer;

	return retval;
}

int32_t coproc_i32_call3( uint8_t opcode, int32_t param0, int32_t param1, int32_t param2 ){

	int32_t retval = 0;

	int32_t param_buf[3];
	param_buf[0] = param0;
	param_buf[1] = param1;
	param_buf[2] = param2;

	coproc_u8_issue( opcode, (uint8_t *)param_buf, sizeof(param_buf) );

	retval = *(int32_t *)buffer;

	return retval;
}

int32_t coproc_i32_callv( uint8_t opcode, const uint8_t *data, uint16_t len ){

	if( len >= COPROC_BUF_SIZE ){

		return -1;
	}

	int32_t retval = 0;

	coproc_u8_issue( opcode, (uint8_t *)data, len );

	retval = *(int32_t *)buffer;

	return retval;
}

int32_t coproc_i32_callp( uint8_t opcode, uint8_t *data, uint16_t len ){
	
	if( len >= COPROC_BUF_SIZE ){

		return -1;
	}

	uint8_t response_len = coproc_u8_issue( opcode, 0, 0 );

	if( response_len > len ){

		response_len = len;
	}

	memcpy( data, buffer, response_len );

	return response_len;
}

