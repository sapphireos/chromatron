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


#define COPROC_SOF		0x17

typedef struct __attribute__((packed)){
	uint8_t sof;
	uint8_t opcode;
	uint8_t length;
	uint8_t padding;
} coproc_hdr_t;

#define COPROC_BUF_SIZE				96

#define OPCODE_TEST					0x01
#define OPCODE_IO_SET_MODE			0x10
#define OPCODE_IO_GET_MODE			0x11
#define OPCODE_IO_DIGITAL_WRITE		0x12
#define OPCODE_IO_DIGITAL_READ		0x13


#define RESPONSE_OK	 				0xF0
#define RESPONSE_ERR				0xF1


void coproc_v_parity_check( coproc_block_t *block );
void coproc_v_parity_generate( coproc_block_t *block );


void coproc_v_dispatch( 
	coproc_hdr_t *hdr, 
	const uint8_t *data,
	uint8_t *response_len,
	uint8_t response[COPROC_BUF_SIZE] );

void coproc_v_test( void );

int64_t coproc_i64_call0( uint8_t opcode );
int64_t coproc_i64_call1( uint8_t opcode, int64_t param0 );
int64_t coproc_i64_call2( uint8_t opcode, int64_t param0, int64_t param1 );
int64_t coproc_i64_call3( uint8_t opcode, int64_t param0, int64_t param1, int64_t param2 );


#endif
