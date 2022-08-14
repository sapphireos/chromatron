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

#include "sapphire.h"

#include "gfx_lib.h"
#include "pixel_mapper.h"
#include "keyvalue.h"

#include <math.h>


void mapper_v_init( void ){

    #ifdef ENABLE_PIXEL_MAPPER

    #endif
}


#ifdef ENABLE_PIXEL_MAPPER

KV_SECTION_META kv_meta_t pixel_mapper_kv[] = {
    // { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &max_power_sat,        0,                   "gfx_enable_max_power_sat" },
};


static mem_handle_t map_h;

static uint16_t get_map_size( void ){

    uint32_t size = gfx_u16_get_pix_count();
    size *= sizeof(pixel_mapping_t);

    ASSERT( size < 65535 );

    return size;
}

void mapper_v_reset( void ){

    if( map_h <= 0 ){

        return;
    }

    void *ptr = mem2_vp_get_ptr( map_h );

    memset( ptr, 0xff, mem2_u16_get_size( map_h ) );
}

void mapper_v_enable( void ){
        
    // check if already enabled
    if( map_h > 0 ){

        return;
    }

    map_h = mem2_h_alloc( get_map_size() );

    mapper_v_reset();
}

void mapper_v_disable( void ){

    // check if not enabled
    if( map_h <= 0 ){

        return;
    }

    mem2_v_free( map_h );
    map_h = -1;
}

void mapper_v_map_3d( uint16_t index, pixel_coord_t x, pixel_coord_t y, pixel_coord_t z ){

    // check if not enabled
    if( map_h <= 0 ){

        return;
    }

    // bounds check
    uint16_t size = mem2_u16_get_size( map_h );

    if( (uint32_t)index * sizeof(pixel_mapping_t) >= size ){

        return;
    }

    pixel_mapping_t *map = mem2_vp_get_ptr( map_h );

    map[index].x = x;
    map[index].y = y;
    map[index].z = z;
}

void mapper_v_map_polar( uint16_t index, pixel_coord_t rho, pixel_coord_t theta, pixel_coord_t z ){

    // check if not enabled
    if( map_h <= 0 ){

        return;
    }

    float radians = (float)theta * M_PI / 180.0;

    float x = (float)rho * cos( radians );
    float y = (float)rho * sin( radians );


    trace_printf("x: %f y: %f\r\n", x, y );
}

void mapper_v_clear( void ){

    // check if not enabled
    if( map_h <= 0 ){

        return;
    }

    gfx_v_clear();
}

// compute fast distance
// this drops the square root
// it is only suitable for comparisons, the actual
// distance will be incorrect.
static uint32_t distance_fast( 
    pixel_coord_t x1, 
    pixel_coord_t y1, 
    pixel_coord_t z1,
    pixel_mapping_t *mapping ){

    int32_t xd = x1 - mapping->x;
    int32_t yd = y1 - mapping->y;
    int32_t zd = z1 - mapping->z;

    return xd * xd + yd * yd + zd * zd;
}

void mapper_v_draw_3d( 
    uint16_t size, 
    uint16_t hue,
    uint16_t sat,
    uint16_t val,
    pixel_coord_t x, 
    pixel_coord_t y, 
    pixel_coord_t z ){

    // check if not enabled
    if( map_h <= 0 ){

        return;
    }

    pixel_mapping_t *map = mem2_vp_get_ptr_fast( map_h );

    uint16_t count = mem2_u16_get_size( map_h ) / sizeof(pixel_mapping_t);

    uint32_t closest_dist = 0xffffffff;
    uint16_t closest_idx = 0;

    for( uint16_t i = 0; i < count; i++ ){

        uint32_t d = distance_fast( x, y, z, &map[i] );

        if( d < closest_dist ){

            closest_dist = d;
            closest_idx = i;
        }
    }

    gfx_v_set_hsv( hue, sat, val, closest_idx );    
}

#endif
