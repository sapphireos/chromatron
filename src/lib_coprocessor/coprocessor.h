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

#define COPROC_SOF		0x17

typedef struct __attribute__((packed)){
	uint8_t opcode;
	uint8_t length;
	uint16_t padding;
} coproc_hdr_t;

#define COPROC_BUF_SIZE				96

#define OPCODE_TEST					0x01


#define RESPONSE_OK	 				0xF0
#define RESPONSE_ERR				0xF1


#endif
