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

#ifndef _UTIL_H
#define _UTIL_H

#include <stdint.h>

float f_abs( float x );
uint8_t abs8( int8_t a );
uint16_t abs16( int16_t a );
uint32_t abs32( int32_t a );
uint64_t abs64( int64_t a );
int8_t util_i8_compare_sequence_u16( uint16_t a, uint16_t b );
int8_t util_i8_compare_sequence_u32( uint32_t a, uint32_t b );
uint16_t util_u16_linear_interp(
    uint16_t x,
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1 );
void util_v_bubble_sort_u16( uint16_t *array, uint8_t len );
void util_v_bubble_sort_u32( uint32_t *array, uint8_t len );
void util_v_bubble_sort_reversed_u32( uint32_t *array, uint8_t len );
// uint8_t util_u8_ewma( uint8_t new, uint8_t old, uint8_t ratio );
// int8_t util_i8_ewma( int8_t new, int8_t old, uint8_t ratio );
int16_t util_i16_ewma( int16_t new, int16_t old, uint8_t ratio );
uint16_t util_u16_ewma( uint16_t new, uint16_t old, uint8_t ratio );
uint32_t util_u32_ewma( uint32_t new, uint32_t old, uint8_t ratio );
uint8_t util_u8_average( uint8_t data[], uint8_t len );
int16_t util_i16_average( int16_t data[], uint16_t len );
uint16_t util_u16_average( uint16_t data[], uint16_t len );

float util_f_degrees_to_radians( float degrees );
float util_f_radians_to_degrees( float radians );
float util_f_distance( float x0, float y0, float x1, float y1 );
void util_v_cart2pol( float x, float y, float *rho, float *phi );
void util_v_pol2cart( float rho, float phi, float *x, float *y );
float util_f_ewma( float new, float old, float ratio );

#endif

