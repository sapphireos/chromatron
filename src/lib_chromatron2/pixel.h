// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifndef _PIXEL_H_
#define _PIXEL_H_

#include "pix_modes.h"

#define PIX0_SPI SPI1
#define PIX1_SPI SPI2

void pixel_v_init( void );

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b );

bool pixel_b_enabled( void );

uint8_t pixel_u8_get_mode( void );

void pixel_v_load_rgb(
    uint16_t index,
    uint16_t len,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *d );

void pixel_v_load_rgb16(
    uint16_t index,
    uint16_t r,
    uint16_t g,
    uint16_t b );

void pixel_v_load_hsv(
    uint16_t index,
    uint16_t len,
    uint16_t *h,
    uint16_t *s,
    uint16_t *v );

void pixel_v_get_rgb_totals( uint16_t *r, uint16_t *g, uint16_t *b );

#endif
