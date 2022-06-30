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
 

#ifndef SPI_H
#define SPI_H

#include <inttypes.h>

void spi_v_init(void);
uint8_t spi_u8_send(uint8_t data);
void spi_v_write_block( const uint8_t *data, uint16_t length );
void spi_v_read_block( uint8_t *data, uint16_t length );

#endif
