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
 

#ifndef _EEPROM_H
#define _EEPROM_H

#include "system.h"
#include "target.h"

#define EE_ARRAY_SIZE 4096 // size of eeprom storage area in bytes

#define EE_ERASE_ONLY 		0b00010000
#define EE_WRITE_ONLY 		0b00100000

void ee_v_write_byte_blocking( uint16_t address, uint8_t data );
void ee_v_write_block( uint16_t address, uint8_t *data, uint16_t len );
uint8_t ee_u8_read_byte( uint16_t address );
void ee_v_read_block( uint16_t address, uint8_t *data, uint16_t length );

#endif

