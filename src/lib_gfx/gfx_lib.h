/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef _GFX_LIB_H
#define _GFX_LIB_H

#include <stdint.h>
#include "bool.h"
#include "catbus_common.h"
#include "target.h"

#define GFX_VERSION             1

typedef struct  __attribute__((packed)){
    uint8_t version;
    uint16_t pix_count;
    uint16_t pix_size_x;
    uint16_t pix_size_y;
    uint8_t pix_mode;
    bool interleave_x;
    bool transpose;
    uint16_t hs_fade;
    uint16_t v_fade;
    uint16_t master_dimmer;
    uint16_t sub_dimmer;
    uint16_t frame_rate;
    uint16_t dimmer_curve;
    uint16_t virtual_array_start;
    uint16_t virtual_array_length;
    uint32_t sync_group_hash;
    uint16_t sat_curve;
} gfx_params_t;

typedef struct  __attribute__((packed)){
    uint16_t h;
    uint16_t s;
    uint16_t v;
} gfx_pixel_t;

#define GFX_DIMMER_CURVE_DEFAULT    128
#define GFX_SAT_CURVE_DEFAULT       128

#define ARRAY_OBJ_TYPE      0
#define PIX_OBJ_TYPE        1

#define PIX_ATTR_HUE        0
#define PIX_ATTR_SAT        1
#define PIX_ATTR_VAL        2
#define PIX_ATTR_HS_FADE    3
#define PIX_ATTR_V_FADE     4

// note this needs to pad to 32 bit alignment!
typedef struct  __attribute__((packed)){
    int32_t count;
    int32_t index;
    int32_t mirror;
    int32_t offset;
    int32_t palette;
    int32_t reverse;
    int32_t size_x;
    int32_t size_y;
} gfx_pixel_array_t;

// note this needs to pad to 32 bit alignment!
typedef struct  __attribute__((packed)){
    int32_t pval;
    int32_t hue;
    int32_t sat;
    int32_t val;
} gfx_palette_t;

void gfx_v_set_vm_frame_rate( uint16_t frame_rate );
uint16_t gfx_u16_get_vm_frame_rate( void );

void gfx_v_set_params( gfx_params_t *params );
void gfx_v_get_params( gfx_params_t *params );

int32_t gfx_i32_lib_call( catbus_hash_t32 func_hash, int32_t *params, uint16_t param_len );

void gfx_v_set_pix_count( uint16_t setting );
uint16_t gfx_u16_get_pix_count( void );

void gfx_v_set_master_dimmer( uint16_t setting );
uint16_t gfx_u16_get_master_dimmer( void );

void gfx_v_set_submaster_dimmer( uint16_t setting );
uint16_t gfx_u16_get_submaster_dimmer( void );

void gfx_v_set_size_x( uint16_t size );
uint16_t gfx_u16_get_size_x( void );

void gfx_v_set_size_y( uint16_t size );
uint16_t gfx_u16_get_size_y( void );

void gfx_v_set_interleave_x( bool setting );
bool gfx_b_get_interleave_x( void );

void gfx_v_set_transpose( bool setting );
bool gfx_b_get_transpose( void );

void gfx_v_set_hsfade( uint16_t setting );
uint16_t gfx_u16_get_hsfade( void );

void gfx_v_set_vfade( uint16_t setting );
uint16_t gfx_u16_get_vfade( void );

void gfx_v_array_move( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_add( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_sub( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_mul( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_div( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_mod( uint8_t obj, uint8_t attr, int32_t src );

uint16_t gfx_u16_get_dimmed_val( uint16_t _val );

void gfx_v_process_faders( void );

uint16_t *gfx_u16p_get_hue( void );
uint16_t *gfx_u16p_get_sat( void );
uint16_t *gfx_u16p_get_val( void );

#ifndef USE_HSV_BRIDGE
uint8_t *gfx_u8p_get_red( void );
uint8_t *gfx_u8p_get_green( void );
uint8_t *gfx_u8p_get_blue( void );
uint8_t *gfx_u8p_get_dither( void );
#endif

void gfx_v_set_background_hsv( int32_t h, int32_t s, int32_t v );

void gfx_v_set_hsv( int32_t h, int32_t s, int32_t v, uint16_t index );
void gfx_v_set_hsv_2d( int32_t h, int32_t s, int32_t v, uint16_t x, uint16_t y );

void gfx_v_set_hue( uint16_t h, uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_hue( uint16_t x, uint16_t y, uint8_t obj );

void gfx_v_set_sat( uint16_t s, uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_sat( uint16_t x, uint16_t y, uint8_t obj );

void gfx_v_set_val( uint16_t v, uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_val( uint16_t x, uint16_t y, uint8_t obj );

void gfx_v_set_pval( uint16_t v, uint16_t x, uint16_t y, uint8_t obj, gfx_palette_t *palette );
uint16_t gfx_u16_get_pval( uint16_t x, uint16_t y, uint8_t obj );

void gfx_v_set_hs_fade( uint16_t a, uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_hs_fade( uint16_t x, uint16_t y, uint8_t obj );

void gfx_v_set_v_fade( uint16_t a, uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_v_fade( uint16_t x, uint16_t y, uint8_t obj );

uint16_t gfx_u16_get_is_v_fading( uint16_t x, uint16_t y, uint8_t obj );
uint16_t gfx_u16_get_is_hs_fading( uint16_t x, uint16_t y, uint8_t obj );

#ifndef USE_HSV_BRIDGE
uint16_t gfx_u16_get_pix0_red( void );
uint16_t gfx_u16_get_pix0_green( void );
uint16_t gfx_u16_get_pix0_blue( void );
#endif

void gfx_v_clear( void );
void gfx_v_reset_faders( void );

void gfxlib_v_init( void );

void gfx_v_sync_array( void );

void gfx_v_reset( void );
void gfx_v_init_pixel_arrays( gfx_pixel_array_t *array_ptr, uint8_t count );
void gfx_v_delete_pixel_arrays( void );

void gfx_v_init_noise( void );
uint16_t gfx_u16_noise( uint16_t x );

#endif
