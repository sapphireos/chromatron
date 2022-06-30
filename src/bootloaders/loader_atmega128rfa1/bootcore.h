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
 
#ifndef BOOTCORE_H
#define BOOTCORE_H


void boot_v_copy_loader( void );
void boot_v_erase_page( uint16_t pagenumber );
void boot_v_write_page( uint16_t pagenumber, uint8_t *data );
void boot_v_read_page( uint16_t pagenumber, uint8_t *dest );
void boot_v_read_data( uint32_t offset, uint8_t *dest, uint16_t length );
uint16_t boot_u16_get_page_size( void );

#endif




