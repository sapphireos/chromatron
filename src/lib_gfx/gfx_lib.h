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

#ifndef _GFX_LIB_H
#define _GFX_LIB_H

#include <stdint.h>
#include "bool.h"
#include "catbus_common.h"
#include "target.h"
#include "hsv_to_rgb.h"

#define GFX_DIMMER_CURVE_DEFAULT    128
#define GFX_SAT_CURVE_DEFAULT       128

#define ARRAY_OBJ_TYPE      0
#define PIX_OBJ_TYPE        1

#define PIX_ARRAY_ATTR_HUE        1
#define PIX_ARRAY_ATTR_SAT        2
#define PIX_ARRAY_ATTR_VAL        3
#define PIX_ARRAY_ATTR_HS_FADE    4
#define PIX_ARRAY_ATTR_V_FADE     5

#define PIX_OP_ADD                1
#define PIX_OP_SUB                2
#define PIX_OP_MUL                3
#define PIX_OP_DIV                4
#define PIX_OP_MOD                5

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
    int32_t reserved0;
    int32_t reserved1;
    int32_t reserved2;
    int32_t reserved3;
} gfx_pixel_array_t;

#define PIX_ATTR_IS_V_FADING    200
#define PIX_ATTR_IS_HS_FADING   201


// note this needs to pad to 32 bit alignment!
typedef struct  __attribute__((packed)){
    int32_t pval;
    int32_t hue;
    int32_t sat;
    int32_t val;
} gfx_palette_t;

void gfx_v_set_vm_frame_rate( uint16_t frame_rate );
uint16_t gfx_u16_get_vm_frame_rate( void );

int32_t gfx_i32_lib_call( catbus_hash_t32 func_hash, int32_t *params, uint16_t param_len );

uint16_t gfx_u16_get_pix_count( void );
uint16_t gfx_u16_get_physical_pix_count( void );
uint16_t gfx_u16_get_pix_driver_count( uint8_t output );
uint16_t gfx_u16_get_pix_driver_offset( uint8_t output );

void gfx_v_set_hue_1d( uint16_t a, uint16_t index );
uint16_t gfx_u16_get_hue_1d( uint16_t index );
void gfx_v_set_sat_1d( uint16_t a, uint16_t index );
uint16_t gfx_u16_get_sat_1d( uint16_t index );
void gfx_v_set_val_1d( uint16_t a, uint16_t index );
uint16_t gfx_u16_get_val_1d( uint16_t index );
void gfx_v_set_hs_fade_1d( uint16_t a, uint16_t index );
uint16_t gfx_u16_get_hs_fade_1d( uint16_t index );
void gfx_v_set_v_fade_1d( uint16_t a, uint16_t index );
uint16_t gfx_u16_get_v_fade_1d( uint16_t index );

void gfx_v_array_move( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_add( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_sub( uint8_t obj, uint8_t attr, int32_t src );
void gfx_v_array_mul( uint8_t obj, uint8_t attr, int32_t src, catbus_type_t8 type );
void gfx_v_array_div( uint8_t obj, uint8_t attr, int32_t src, catbus_type_t8 type );
void gfx_v_array_mod( uint8_t obj, uint8_t attr, int32_t src );

// void gfx_v_pixel_move( uint8_t obj, uint16_t index, uint8_t attr, int32_t src );
void gfx_v_pixel_add( uint8_t obj, uint16_t index, uint8_t attr, int32_t src );
void gfx_v_pixel_sub( uint8_t obj, uint16_t index, uint8_t attr, int32_t src );
void gfx_v_pixel_mul( uint8_t obj, uint16_t index, uint8_t attr, int32_t src, catbus_type_t8 type );
void gfx_v_pixel_div( uint8_t obj, uint16_t index, uint8_t attr, int32_t src, catbus_type_t8 type );
void gfx_v_pixel_mod( uint8_t obj, uint16_t index, uint8_t attr, int32_t src );
void gfx_v_pixel_store( uint8_t obj, uint16_t index, uint8_t attr, int32_t src );

uint16_t gfx_u16_get_dimmed_val( uint16_t _val );

void gfx_v_set_system_enable( bool enable );
bool gfx_b_is_system_enabled( void );

void gfx_v_process_faders( void );

uint16_t *gfx_u16p_get_hue( void );
uint16_t *gfx_u16p_get_sat( void );
uint16_t *gfx_u16p_get_val( void );

uint8_t *gfx_u8p_get_red( void );
uint8_t *gfx_u8p_get_green( void );
uint8_t *gfx_u8p_get_blue( void );
uint8_t *gfx_u8p_get_dither( void );

uint16_t gfx_u16_calc_index( uint8_t obj, uint16_t x, uint16_t y );

void gfx_v_drop( int32_t h, int32_t s, int32_t v, int32_t x, int32_t y, uint16_t radius );

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

uint16_t gfx_u16_get_pix0_red( void );
uint16_t gfx_u16_get_pix0_green( void );
uint16_t gfx_u16_get_pix0_blue( void );

void gfx_v_clear( void );
void gfx_v_shutdown_graphic( void );
void gfx_v_power_limiter_graphic( void );

void gfxlib_v_init( void );

bool gfx_b_is_output_zero( void );

void gfx_v_sync_array( void );

void gfx_v_reset( void );
void gfx_v_init_pixel_arrays( gfx_pixel_array_t *array_ptr, uint8_t count );
void gfx_v_delete_pixel_arrays( void );
int8_t gfx_i8_get_pixel_array( uint8_t obj, gfx_pixel_array_t **array_ptr );

int32_t gfx_i32_get_pixel_attr( uint8_t obj, uint8_t attr );
int32_t gfx_i32_get_pixel_attr_single( uint16_t index, uint8_t attr );
void gfx_v_set_pixel_attr( uint8_t obj, uint8_t attr, int32_t value );

void gfx_v_init_noise( void );
uint16_t gfx_u16_noise( uint16_t x );

void gfx_v_init_shuffle( void );
uint16_t gfx_u16_shuffle_count( void );
uint16_t gfx_u16_shuffle( void );

uint32_t gfx_u32_get_pixel_r( void );
uint32_t gfx_u32_get_pixel_g( void );
uint32_t gfx_u32_get_pixel_b( void );
uint32_t gfx_u32_get_pixel_w( void );

// implemented by VM to signal frame rate changes
void gfx_vm_v_update_frame_rate( uint16_t new_frame_rate );

// logs the value -> RGB curve to a file
// debug only!
void gfx_v_log_value_curve( void );

// declared here so pixel driver can get to it
// see pixel_power.h
bool pixelpower_b_pixels_enabled( void );
bool pixelpower_b_power_control_enabled( void );

#endif
