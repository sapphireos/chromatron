/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
 */
 
#ifndef _FLASH25_H
#define _FLASH25_H

#define FLASH_WAIT_DELAY_US     1

// define chip type
//#define FLASH_AT25
#define FLASH_SST25

// 4.1:
#define FLASH_CS_DDR DDRB
#define FLASH_CS_PORT PORTB
#define FLASH_CS_PIN 5

#define FLASH_WP_DDR DDRB
#define FLASH_WP_PORT PORTB
#define FLASH_WP_PIN 4


// status register bits
#define FLASH_STATUS_BUSY			0x01
#define FLASH_STATUS_WEL			0x02 // write enable latch

#ifdef FLASH_SST25 // these bits unique to the SST25
	#define FLASH_STATUS_BP0			0x04
	#define FLASH_STATUS_BP1			0x08
	#define FLASH_STATUS_BP2			0x10
	#define FLASH_STATUS_BP3			0x20
	#define FLASH_STATUS_AAI			0x40 // auto address increment
	#define FLASH_STATUS_BPL			0x80
#endif

#ifdef FLASH_AT25 // these bits unique the the AT25
	#define FLASH_STATUS_SWP0			0x04
	#define FLASH_STATUS_SWP1			0x08
	#define FLASH_STATUS_WPP			0x10
	#define FLASH_STATUS_EPE			0x20
	#define FLASH_STATUS_RES			0x40
	#define FLASH_STATUS_SPRL			0x80
#endif

// instruction set
#define FLASH_CMD_READ					0x03 // this is the "low speed" read command
#define FLASH_CMD_ERASE_BLOCK_4K		0x20						
#define FLASH_CMD_ERASE_BLOCK_32K		0x52						
#define FLASH_CMD_ERASE_BLOCK_64K		0xD8
#define FLASH_CMD_CHIP_ERASE			0x60
#define FLASH_CMD_WRITE_BYTE			0x02
#define FLASH_CMD_READ_STATUS			0x05
#define FLASH_CMD_ENABLE_WRITE_STATUS	0x50
#define FLASH_CMD_WRITE_STATUS			0x01
#define FLASH_CMD_WRITE_ENABLE			0x06
#define FLASH_CMD_WRITE_DISABLE			0x04
#define FLASH_CMD_READ_ID				0x9F
#define FLASH_CMD_AAI_WRITE				0xAD
#define FLASH_CMD_EBUSY 				0x70
#define FLASH_CMD_DBUSY 				0x80


typedef struct{
	uint8_t mfg_id; // manufacturer's ID
	uint8_t dev_id_1;
	uint8_t dev_id_2;
} flash25_device_info_t;

#define FLASH_MFG_ATMEL					0x1F
#define FLASH_MFG_SST					0xBF

#define FLASH_DEV_ID1_SST25             0x25

void flash25_v_init( void );
uint8_t flash25_u8_read_status( void );
bool flash25_b_busy( void );
void flash25_v_write_status( uint8_t status );
void flash25_v_read( uint32_t address, void *ptr, uint32_t len );
uint8_t flash25_u8_read_byte( uint32_t address );
void flash25_v_write_enable( void );
void flash25_v_write_disable( void );
void flash25_v_write_byte( uint32_t address, uint8_t byte );
void flash25_v_write( uint32_t address, const void *ptr, uint32_t len );
void flash25_v_erase_4k( uint32_t address );
void flash25_v_erase_chip( void );
void flash25_v_read_device_info( flash25_device_info_t *info );
uint8_t flash25_u8_read_mfg_id( void );
uint32_t flash25_u32_capacity( void );









#endif

