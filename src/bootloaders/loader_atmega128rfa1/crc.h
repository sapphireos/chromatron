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
 

#ifndef CRC_H
#define CRC_H

#include <inttypes.h>

#define BOOTLOADER

#define CRC_SIZE 2

void crc_v_init( void );
uint16_t crc_u16_block(uint8_t *ptr, uint16_t length);
uint16_t crc_u16_partial_block(uint16_t crc, uint8_t *ptr, uint16_t length);
uint16_t crc_u16_byte(uint16_t crc, uint8_t data);
//uint16_t crc_u16_inline(uint16_t crc, uint8_t data);

#endif

