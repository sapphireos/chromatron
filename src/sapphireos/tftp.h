/* 
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
 */


#ifndef _TFTP_H
#define _TFTP_H

#if 0

/*! 
    Packet transfer timeout in milliseconds.
*/
#define TFTP_TIMEOUT_MS 10000

#define TFTP_SERVER_PORT 69

#define TFTP_WRITE_PACKET_FULL_LEN 516

// opcodes
#define TFTP_RRQ 1 // read request
#define TFTP_WRQ 2 // write request	
#define TFTP_DATA 3 // data
#define TFTP_ACK 4 // ack
#define TFTP_ERROR 5 // error

typedef struct{
	uint16_t opcode;
	char filename[64]; // first 64 characters of filename
} tftp_cmd_t;

typedef struct{
	uint16_t opcode;
	uint16_t block;
	uint8_t data[512];
} tftp_data_t;

typedef struct{
	uint16_t opcode;
	uint16_t block;
} tftp_ack_t;

typedef struct{
    uint16_t opcode;
    uint16_t err_code;
    char err_string[64]; // this implementation limits error message length to 63 chars + 0 terminator
} tftp_err_t;

// error codes
// this lists all codes in RFC 1350, however not all are
// used in this implementation
#define TFTP_ERR_UNDEFINED              0
#define TFTP_ERR_FILE_NOT_FOUND         1
#define TFTP_ERR_ACCESS_VIOLATION       2
#define TFTP_ERR_DISK_FULL              3
#define TFTP_ERR_ILLEGAL_OP             4
#define TFTP_ERR_UNKNOWN_XFER_ID        5
#define TFTP_ERR_FILE_ALREADY_EXISTS    6
#define TFTP_ERR_NO_SUCH_USER           7
#define TFTP_ERR_TIMED_OUT              8


void tftp_v_init( void );

#endif

#endif
