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

#ifndef _COPROCESSOR_H_
#define _COPROCESSOR_H_

#define UART_CHANNEL 0

#define COPROC_BLOCK_LEN 4

typedef struct __attribute__((packed)){
	uint8_t data[COPROC_BLOCK_LEN];
	uint8_t parity[3];
} coproc_block_t;


#define COPROC_SYNC		0xB9

#define COPROC_SOF		0xE6

typedef struct __attribute__((packed)){
	uint8_t sof;
	uint8_t opcode;
	uint8_t length;
	uint8_t padding;
} coproc_hdr_t;

#define COPROC_BUF_SIZE				255

#define OPCODE_TEST					0x01
#define OPCODE_REBOOT				0x02
#define OPCODE_LOAD_DISABLE			0x03
#define OPCODE_IO_SET_MODE			0x10
#define OPCODE_IO_GET_MODE			0x11
#define OPCODE_IO_DIGITAL_WRITE		0x12
#define OPCODE_IO_DIGITAL_READ		0x13
#define OPCODE_IO_READ_ADC			0x14
#define OPCODE_IO_WRITE_PWM			0x15

#define OPCODE_FW_CRC				0x20
#define OPCODE_FW_ERASE				0x21
#define OPCODE_FW_LOAD				0x22
#define OPCODE_FW_BOOTLOAD			0x23
#define OPCODE_FW_VERSION			0x24

#define OPCODE_PIX_SET_COUNT		0x30
#define OPCODE_PIX_SET_MODE			0x31
#define OPCODE_PIX_SET_DITHER		0x32
#define OPCODE_PIX_SET_CLOCK		0x33
#define OPCODE_PIX_SET_RGB_ORDER	0x34
#define OPCODE_PIX_SET_APA102_DIM	0x35
#define OPCODE_PIX_LOAD				0x36

#define OPCODE_IO_CMD_IS_RX_CHAR	0x40
#define OPCODE_IO_CMD_SEND_CHAR		0x41
#define OPCODE_IO_CMD_SEND_DATA		0x42
#define OPCODE_IO_CMD_GET_CHAR		0x43
#define OPCODE_IO_CMD_GET_DATA		0x44
#define OPCODE_IO_CMD_RX_SIZE		0x45
#define OPCODE_IO_CMD_FLUSH			0x46


#define COPROC_PIX_WAIT_COUNT		16

#define COPROC_FW_INFO_ADDRESS 0x1FC // this must match the offset in the xmega makefile!

void coproc_v_send_block( uint8_t data[COPROC_BLOCK_LEN] );
void coproc_v_receive_block( uint8_t data[COPROC_BLOCK_LEN] );

void coproc_v_sync( void );

void coproc_v_parity_check( coproc_block_t *block );
void coproc_v_parity_generate( coproc_block_t *block );


void coproc_v_dispatch( 
	coproc_hdr_t *hdr, 
	const uint8_t *data,
	uint8_t *response_len,
	uint8_t response[COPROC_BUF_SIZE] );

uint8_t coproc_u8_issue( 
	uint8_t opcode, 
	uint8_t *data, 
	uint8_t len );

void coproc_v_reboot( void );

void coproc_v_test( void );

void coproc_v_fw_version( char firmware_version[FW_VER_LEN] );
uint16_t coproc_u16_fw_crc( void );
void coproc_v_fw_erase( void );
void coproc_v_fw_load( uint8_t *data, uint32_t len );
void coproc_v_fw_bootload( void );

int32_t coproc_i32_call0( uint8_t opcode );
int32_t coproc_i32_call1( uint8_t opcode, int32_t param0 );
int32_t coproc_i32_call2( uint8_t opcode, int32_t param0, int32_t param1 );
int32_t coproc_i32_call3( uint8_t opcode, int32_t param0, int32_t param1, int32_t param2 );



#endif
