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

#ifndef _PIXEL_MAPPER_H
#define _PIXEL_MAPPER_H

typedef int16_t pixel_coord_t;

typedef struct{
    pixel_coord_t x;
    pixel_coord_t y;
    pixel_coord_t z;
} pixel_mapping_t;


void mapper_v_init( void );

#ifdef ENABLE_PIXEL_MAPPER
void mapper_v_reset( void );
void mapper_v_enable( void );
void mapper_v_disable( void );
void mapper_v_map_3d( uint16_t index, pixel_coord_t x, pixel_coord_t y, pixel_coord_t z );
void mapper_v_map_polar( uint16_t index, pixel_coord_t rho, pixel_coord_t theta, pixel_coord_t z );

void mapper_v_clear( void );

void mapper_v_draw_3d( 
    uint16_t size, 
    uint16_t hue,
    uint16_t sat,
    uint16_t val,
    pixel_coord_t x, 
    pixel_coord_t y, 
    pixel_coord_t z );


#endif

#endif