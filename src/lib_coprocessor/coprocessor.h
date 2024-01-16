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

#ifndef _COPROCESSOR_H_
#define _COPROCESSOR_H_

#define UART_CHANNEL 0

#define COPROC_BLOCK_LEN 4

typedef struct __attribute__((packed)){
	uint8_t data[COPROC_BLOCK_LEN];
	uint8_t crc;
} coproc_block_t;


#define COPROC_SYNC		0xB9
#define COPROC_VERSION 	0

#define COPROC_SOF		0xE6

typedef struct __attribute__((packed)){
	uint8_t sof;
	uint8_t opcode;
	uint8_t length;
	uint8_t padding;
} coproc_hdr_t;

#define COPROC_BUF_SIZE				255
#define COPROC_FLASH_XFER_LEN		252

#define OPCODE_TEST					0x01
#define OPCODE_REBOOT				0x02
#define OPCODE_LOAD_DISABLE			0x03
#define OPCODE_GET_RESET_SOURCE		0x04
#define OPCODE_GET_WIFI				0x05
#define OPCODE_DEBUG_PRINT  		0x06
#define OPCODE_GET_BOOT_MODE		0x07
#define OPCODE_LOADFW_1				0x08
#define OPCODE_LOADFW_2				0x09
#define OPCODE_SAFE_MODE     		0x0A
#define OPCODE_GET_ERROR_FLAGS		0x0B
#define OPCODE_CLEAR_ERROR_FLAGS	0x0C

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

// USART
#define OPCODE_IO_USART_INIT		0x50
#define OPCODE_IO_USART_SEND_CHAR	0x51
#define OPCODE_IO_USART_GET_CHAR	0x52
#define OPCODE_IO_USART_RX_SIZE		0x53
#define OPCODE_IO_USART_SET_BAUD	0x54
#define OPCODE_IO_USART_GET_CHUNK	0x55

// I2C
#define OPCODE_IO_I2C_INIT			0x60
#define OPCODE_IO_I2C_SET_PINS		0x61
#define OPCODE_IO_I2C_SETUP			0x62
#define OPCODE_IO_I2C_WRITE			0x63
#define OPCODE_IO_I2C_READ			0x64
#define OPCODE_IO_I2C_MEM_WRITE		0x65
#define OPCODE_IO_I2C_MEM_READ		0x66
#define OPCODE_IO_I2C_WRITE_REG8	0x67
#define OPCODE_IO_I2C_READ_REG8		0x68

// Flash25
#define OPCODE_IO_FLASH25_SIZE		0x70
#define OPCODE_IO_FLASH25_ADDR		0x71
#define OPCODE_IO_FLASH25_LEN		0x72
#define OPCODE_IO_FLASH25_ERASE		0x73
#define OPCODE_IO_FLASH25_READ		0x74
#define OPCODE_IO_FLASH25_WRITE		0x75
#define OPCODE_IO_FLASH25_READ2 	0x76


typedef struct __attribute__((packed, aligned(4))){
	uint32_t dev_addr;
	uint32_t len;
	uint32_t mem_addr;
	uint32_t addr_size;
	uint32_t delay_ms;
} i2c_setup_t;

#define COPROC_PIX_WAIT_COUNT		16

#define COPROC_FW_INFO_ADDRESS 0x1FC // this must match the offset in the xmega makefile!

void coproc_v_send_block( uint8_t data[COPROC_BLOCK_LEN] );
void coproc_v_receive_block( uint8_t data[COPROC_BLOCK_LEN], bool header );

void coproc_v_init( void );
void coproc_v_sync( void );
int32_t coproc_i32_debug_print( char *s );

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
void coproc_v_get_wifi( void );

int32_t coproc_i32_call0( uint8_t opcode );
int32_t coproc_i32_call1( uint8_t opcode, int32_t param0 );
int32_t coproc_i32_call2( uint8_t opcode, int32_t param0, int32_t param1 );
int32_t coproc_i32_call3( uint8_t opcode, int32_t param0, int32_t param1, int32_t param2 );

int32_t coproc_i32_callv( uint8_t opcode, const uint8_t *data, uint16_t len );

int32_t coproc_i32_callp( uint8_t opcode, uint8_t *data, uint16_t len );
int32_t coproc_i32_callp1( uint8_t opcode, int32_t param0, uint8_t *data, uint16_t len );
int32_t coproc_i32_callp2( uint8_t opcode, int32_t param0, int32_t param1, uint8_t *data, uint16_t len );



#define COPROC_ERROR_RX_FAIL	0x00000001
#define COPROC_ERROR_CRC_FAIL	0x00000002
#define COPROC_ERROR_SYNC_FAIL	0x00000004
#define COPROC_ERROR_PIX_STALL	0x00000008
#define COPROC_ERROR_VERSION	0x10000000
#define COPROC_ERROR_IMAGE_CRC	0x20000000

#ifdef AVR
void coproc_v_set_error_flags( uint32_t flags );
void coproc_v_clear_error_flags( void );
#else
#define coproc_v_set_error_flags( a )
#define coproc_v_clear_error_flags()
#endif

#endif
