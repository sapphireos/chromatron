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

#include "target.h"

#ifdef USE_GFX_LIB

#include <inttypes.h>
#include "bool.h"
#include "trig.h"
#include "util.h"
#include "random.h"
#include "pix_modes.h"
#include "kvdb.h"
#include "hsv_to_rgb.h"

#include "gfx_lib.h"


#ifndef USE_HSV_BRIDGE
static uint8_t array_red[MAX_PIXELS];
static uint8_t array_green[MAX_PIXELS];
static uint8_t array_blue[MAX_PIXELS];
static uint8_t array_misc[MAX_PIXELS];

static uint16_t pix0_16bit_red;
static uint16_t pix0_16bit_green;
static uint16_t pix0_16bit_blue;
#endif

static uint16_t hue[MAX_PIXELS];
static uint16_t sat[MAX_PIXELS];
static uint16_t val[MAX_PIXELS];
static uint16_t pval[MAX_PIXELS];

static uint16_t target_hue[MAX_PIXELS];
static uint16_t target_sat[MAX_PIXELS];
static uint16_t target_val[MAX_PIXELS];

static uint16_t global_hs_fade = 1000;
static uint16_t global_v_fade = 1000;
static uint16_t hs_fade[MAX_PIXELS];
static uint16_t v_fade[MAX_PIXELS];

static int16_t hue_step[MAX_PIXELS];
static int16_t sat_step[MAX_PIXELS];
static int16_t val_step[MAX_PIXELS];

static uint16_t pix_master_dimmer = 0;
static uint16_t pix_sub_dimmer = 0;
static uint16_t target_dimmer = 0;
static uint16_t current_dimmer = 0;
static int16_t dimmer_step = 0;

static uint8_t pix_mode;
static uint16_t pix_count;
static uint16_t pix_size_x;
static uint16_t pix_size_y;
static bool pix_interleave_x;
static bool pix_transpose;

static uint8_t pix_array_count;
static gfx_pixel_array_t *pix_arrays;

static uint16_t virtual_array_start;
static uint16_t virtual_array_length;
static uint8_t virtual_array_sub_position;
static uint32_t scaled_pix_count;
static uint32_t scaled_virtual_array_length;

static uint16_t gfx_frame_rate = 100;

static uint8_t dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
static uint8_t sat_curve = GFX_SAT_CURVE_DEFAULT;


#define DIMMER_LOOKUP_SIZE 256
static uint16_t dimmer_lookup[DIMMER_LOOKUP_SIZE];
static uint16_t sat_lookup[DIMMER_LOOKUP_SIZE];


// smootherstep is an 8 bit lookup table for the function:
// 6 * pow(x, 5) - 15 * pow(x, 4) + 10 * pow(x, 3)
static uint8_t smootherstep_lookup[DIMMER_LOOKUP_SIZE] = {
    #include "smootherstep.csv"
};

#define NOISE_TABLE_SIZE 256
static uint8_t noise_table[NOISE_TABLE_SIZE];


#ifdef GFX_LIB_KV_LINKAGE
#include "keyvalue.h"

KV_SECTION_META kv_meta_t gfx_lib_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_sub_dimmer,              0,   "gfx_sub_dimmer" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_master_dimmer,           0,   "gfx_master_dimmer" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_x,                  0,   "pix_size_x" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_y,                  0,   "pix_size_y" },
    { SAPPHIRE_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &pix_interleave_x,            0,   "gfx_interleave_x" },
    { SAPPHIRE_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &pix_transpose,               0,   "gfx_transpose" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &hs_fade,                     0,   "gfx_hsfade" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &v_fade,                      0,   "gfx_vfade" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &dimmer_curve,                0,   "gfx_dimmer_curve" },
    { SAPPHIRE_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &sat_curve,                   0,   "gfx_sat_curve" },
    
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &virtual_array_start,         0,   "gfx_varray_start" },
    { SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &virtual_array_length,        0,   "gfx_varray_length" },
};
#endif


static void compute_dimmer_lookup( void ){

    float curve_exp = (float)dimmer_curve / 64.0;

    for( uint32_t i = 0; i < DIMMER_LOOKUP_SIZE; i++ ){

        float input = ( (float)( i << 8 ) ) / 65535.0;

        dimmer_lookup[i] = (uint16_t)( pow( input, curve_exp ) * 65535.0 );
    }
}

static void compute_sat_lookup( void ){

    float curve_exp = (float)sat_curve / 64.0;

    for( uint32_t i = 0; i < DIMMER_LOOKUP_SIZE; i++ ){

        float input = ( (float)( i << 8 ) ) / 65535.0;

        sat_lookup[i] = (uint16_t)( pow( input, curve_exp ) * 65535.0 );
    }
}

static void setup_master_array( void ){

    // check if pixel arrays are configured
    if( pix_arrays == 0 ){

        return;
    }

    pix_arrays[0].reverse = FALSE;
    pix_arrays[0].index = 0;
    pix_arrays[0].mirror = -1;
    pix_arrays[0].count = pix_count;
    pix_arrays[0].size_x = pix_size_x;
    pix_arrays[0].size_y = pix_size_y;    
}

static void update_master_fader( void ){

    uint16_t fade_steps = global_v_fade / FADER_RATE;

    if( fade_steps <= 1 ){

        fade_steps = 2;
    }

    target_dimmer = ( (uint32_t)pix_master_dimmer * (uint32_t)pix_sub_dimmer ) / 65536;

    int32_t diff = (int32_t)target_dimmer - (int32_t)current_dimmer;
    int32_t step = diff / fade_steps;

    if( step > 32768 ){

        step = 32768;
    }
    else if( step < -32767 ){

        step = -32767;
    }
    else if( step == 0 ){

        if( diff >= 0 ){

            step = 1;
        }
        else{

            step = -1;
        }
    }

    dimmer_step = step;
}

// static void sync_db( void ){

//     kvdb_i8_add( __KV__pix_count,                   CATBUS_TYPE_UINT16, 1, &pix_count,                 sizeof(pix_count)            );
//     kvdb_i8_add( __KV__pix_size_x,                  CATBUS_TYPE_UINT16, 1, &pix_size_x,                sizeof(pix_size_x)           );
//     kvdb_i8_add( __KV__pix_size_y,                  CATBUS_TYPE_UINT16, 1, &pix_size_y,                sizeof(pix_size_y)           );
//     kvdb_i8_add( __KV__pix_mode,                    CATBUS_TYPE_UINT8,  1, &pix_mode,                  sizeof(pix_mode)             );
//     kvdb_i8_add( __KV__gfx_hsfade,                  CATBUS_TYPE_UINT16, 1, &global_hs_fade,            sizeof(global_hs_fade)       );
//     kvdb_i8_add( __KV__gfx_vfade,                   CATBUS_TYPE_UINT16, 1, &global_v_fade,             sizeof(global_v_fade)        );
//     kvdb_i8_add( __KV__gfx_interleave_x,            CATBUS_TYPE_BOOL,   1, &pix_interleave_x,          sizeof(pix_interleave_x)     );
//     kvdb_i8_add( __KV__gfx_transpose,               CATBUS_TYPE_BOOL,   1, &pix_transpose,             sizeof(pix_transpose)        );
//     kvdb_i8_add( __KV__gfx_master_dimmer,           CATBUS_TYPE_UINT16, 1, &pix_master_dimmer,         sizeof(pix_master_dimmer)    );
//     kvdb_i8_add( __KV__gfx_sub_dimmer,              CATBUS_TYPE_UINT16, 1, &pix_sub_dimmer,            sizeof(pix_sub_dimmer)       );
//     kvdb_i8_add( __KV__gfx_frame_rate,              CATBUS_TYPE_UINT16, 1, &gfx_frame_rate,            sizeof(gfx_frame_rate)       );
//     kvdb_i8_add( __KV__gfx_virtual_array_start,     CATBUS_TYPE_UINT16, 1, &virtual_array_start,       sizeof(virtual_array_start)  );
//     kvdb_i8_add( __KV__gfx_virtual_array_length,    CATBUS_TYPE_UINT16, 1, &virtual_array_length,      sizeof(virtual_array_length) );
// }

static void param_error_check( void ){

    // error check
    if( pix_count > MAX_PIXELS ){

        pix_count = MAX_PIXELS;
    }
    else if( pix_count == 0 ){

        // this is necessary to prevent divide by 0 errors.
        // also, 0 pixels doesn't really make sense in a
        // pixel graphics library, does it?
        pix_count = 1;
    }

    if( ( (uint32_t)pix_size_x * (uint32_t)pix_size_y ) > pix_count ){
        
        pix_size_x = pix_count;
        pix_size_y = 1;
    }

    if( pix_size_x > pix_count ){

        pix_size_x = 1;
    }

    if( pix_size_y > pix_count ){

        pix_size_y = 1;
    }

    if( pix_count > 0 ){

        if( pix_size_x == 0 ){

            pix_size_x = 1;
        }

        if( pix_size_y == 0 ){

            pix_size_y = 1;
        }
    }

    if( gfx_frame_rate < 10 ){

        gfx_frame_rate = 10;
    }

    if( dimmer_curve < 8 ){

        dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
    }

    if( sat_curve < 8 ){

        sat_curve = GFX_SAT_CURVE_DEFAULT;
    }
}

void gfx_v_set_vm_frame_rate( uint16_t frame_rate ){

    gfx_frame_rate = frame_rate;

    param_error_check();
}

uint16_t gfx_u16_get_vm_frame_rate( void ){

    return gfx_frame_rate;
}

void gfx_v_set_params( gfx_params_t *params ){

    // version check
    if( params->version != GFX_VERSION ){

        return;
    }

    uint8_t old_dimmer_curve = dimmer_curve;
    uint8_t old_sat_curve = sat_curve;

    dimmer_curve            = params->dimmer_curve;
    sat_curve               = params->sat_curve;
    pix_count               = params->pix_count;
    pix_size_x              = params->pix_size_x;
    pix_size_y              = params->pix_size_y;
    pix_interleave_x        = params->interleave_x;
    pix_transpose           = params->transpose;
    pix_mode                = params->pix_mode;
    global_hs_fade          = params->hs_fade;
    global_v_fade           = params->v_fade;
    pix_master_dimmer       = params->master_dimmer;
    pix_sub_dimmer          = params->sub_dimmer;
    gfx_frame_rate          = params->frame_rate;
    virtual_array_start     = params->virtual_array_start;
    virtual_array_length    = params->virtual_array_length;

    param_error_check();

    // only run if dimmer curve is changing
    if( old_dimmer_curve != dimmer_curve ){
        
        compute_dimmer_lookup();
    }

    // only run if sat curve is changing
    if( old_sat_curve != sat_curve ){
        
        compute_sat_lookup();
    }

    update_master_fader();

    // sync_db();

    virtual_array_sub_position      = virtual_array_start / pix_count;
    scaled_pix_count                = (uint32_t)pix_count * 65536;
    scaled_virtual_array_length     = (uint32_t)virtual_array_length * 65536;
}

void gfx_v_get_params( gfx_params_t *params ){

    params->version                 = GFX_VERSION;
    params->pix_count               = pix_count;
    params->pix_size_x              = pix_size_x;
    params->pix_size_y              = pix_size_y;
    params->interleave_x            = pix_interleave_x;
    params->transpose               = pix_transpose;
    params->pix_mode                = pix_mode;
    params->hs_fade                 = global_hs_fade;
    params->v_fade                  = global_v_fade;
    params->master_dimmer           = pix_master_dimmer;
    params->sub_dimmer              = pix_sub_dimmer;
    params->frame_rate              = gfx_frame_rate;
    params->dimmer_curve            = dimmer_curve;
    params->sat_curve               = sat_curve;
    params->virtual_array_start     = virtual_array_start;
    params->virtual_array_length    = virtual_array_length;
}

uint16_t urand( int32_t *params, uint16_t param_len ){

    int32_t temp0, temp1;

    if( param_len == 0 ){

       return rnd_u16_get_int();      
    }
    else if( param_len == 1 ){

        temp0 = params[params[0]];
        temp1 = 0;
    }
    else{

        temp0 = params[params[1]] - params[params[0]];
        temp1 = params[params[0]];
    }

    // check for divide by 0, or negative spread
    if( temp0 <= 0 ){

        // just return the offset
        return temp1;
        
    }
    else{
    
        return temp1 + ( rnd_u16_get_int() % temp0 );
    }
}

int32_t gfx_i32_lib_call( catbus_hash_t32 func_hash, int32_t *params, uint16_t param_len ){

    switch( func_hash ){
        // unique random
        // this works the same as the rand() call
        // in the FX VM, however, it always uses
        // the system rng seed instead of the VM
        // seed.
        // this allows scripts to get random numbers
        // unique to themselves when doing a frame sync
        // with other nodes (which syncs the VM rng)
        case __KV__urand:
            return urand( params, param_len );
    
            break;

        case __KV__noise:
            return gfx_u16_noise( params[0] % 65536 );
            break;

        default:
            break;
    }    

    return 0;
}

void gfx_v_set_pix_count( uint16_t setting ){

    pix_count = setting;
}

uint16_t gfx_u16_get_pix_count( void ){

    return pix_count;
}

void gfx_v_set_master_dimmer( uint16_t setting ){

    pix_master_dimmer = setting;
    update_master_fader();
}

uint16_t gfx_u16_get_master_dimmer( void ){

    return pix_master_dimmer;
}

void gfx_v_set_submaster_dimmer( uint16_t setting ){

    pix_sub_dimmer = setting;
    update_master_fader();
}

uint16_t gfx_u16_get_submaster_dimmer( void ){

    return pix_sub_dimmer;
}

void gfx_v_set_size_x( uint16_t size ){

    pix_size_x = size;
}

uint16_t gfx_u16_get_size_x( void ){

    return pix_size_x;
}

void gfx_v_set_size_y( uint16_t size ){

    pix_size_y = size;
}

uint16_t gfx_u16_get_size_y( void ){

    return pix_size_y;
}

void gfx_v_set_interleave_x( bool setting ){

    pix_interleave_x = setting;
}

bool gfx_b_get_interleave_x( void ){

    return pix_interleave_x;
}

void gfx_v_set_transpose( bool setting ){

    pix_transpose = setting;
}

bool gfx_b_get_transpose( void ){

    return pix_transpose;
}

void gfx_v_set_hsfade( uint16_t setting ){

    global_hs_fade = setting;
}

uint16_t gfx_u16_get_hsfade( void ){

    return global_hs_fade;
}

void gfx_v_set_vfade( uint16_t setting ){

    global_v_fade = setting;
}

uint16_t gfx_u16_get_vfade( void ){

    return global_v_fade;
}



static inline void _gfx_v_set_hue_1d( uint16_t h, uint16_t index ){

    // bounds check
    // optimization note:
    // possibly can remove this, ensure all functions that call
    // it have already wrapped index (which is the normal case)
    if( index >= MAX_PIXELS ){

        return;
    }

    target_hue[index] = h;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    hue_step[index] = 0;
}

static inline void _gfx_v_set_sat_1d( uint16_t s, uint16_t index ){

    // bounds check
    if( index >= MAX_PIXELS ){

        return;
    }

    target_sat[index] = s;
    
    // reset fader, this will trigger the fader process to recalculate the fader steps.
    sat_step[index] = 0;
}

static inline void _gfx_v_set_val_1d( uint16_t v, uint16_t index ){

    // bounds check
    if( index >= MAX_PIXELS ){

        return;
    }

    target_val[index] = v;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    val_step[index] = 0;
}

static inline void _gfx_v_set_hs_fade_1d( uint16_t a, uint16_t index ){

    // bounds check
    if( index >= MAX_PIXELS ){

        return;
    }

    hs_fade[index] = a;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    hue_step[index] = 0;
    sat_step[index] = 0;
}

static inline void _gfx_v_set_v_fade_1d( uint16_t a, uint16_t index ){

    // bounds check
    if( index >= MAX_PIXELS ){

        return;
    }

    v_fade[index] = a;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    val_step[index] = 0;
}



static uint16_t* _gfx_u16p_get_array_ptr( uint8_t attr ){

    uint16_t *ptr = target_val;

    if( attr == PIX_ATTR_HUE ){

        ptr = target_hue;
    }
    else if( attr == PIX_ATTR_SAT ){

        ptr = target_sat;
    }
    else if( attr == PIX_ATTR_VAL ){

        ptr = target_val;
    }
    else if( attr == PIX_ATTR_HS_FADE ){

        ptr = hs_fade;
    }
    else if( attr == PIX_ATTR_V_FADE ){

        ptr = v_fade;
    }

    return ptr;
}

void gfx_v_array_move( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    // possible optimization:
    // void ( *array_func )( uint16_t a, uint16_t i );

    // if( attr == PIX_ATTR_HUE ){

    //     array_func = _gfx_v_set_hue_1d;
    // }
    // else if( attr == PIX_ATTR_SAT ){

    //     array_func = _gfx_v_set_sat_1d;
    // }
    // else if( attr == PIX_ATTR_HS_FADE ){

    //     array_func = _gfx_v_set_hs_fade_1d;
    // }
    // else if( attr == PIX_ATTR_V_FADE ){

    //     array_func = _gfx_v_set_v_fade_1d;
    // }   
    // else{

    //     array_func = _gfx_v_set_val_1d;
    // }

    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }
        // possible optimization:
        // array_func( a, index );
        
        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}

void gfx_v_array_add( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    uint16_t *ptr = _gfx_u16p_get_array_ptr( attr );
    
    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = *( ptr + index );

        a += src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }

        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}


void gfx_v_array_sub( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    uint16_t *ptr = _gfx_u16p_get_array_ptr( attr );
    
    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = *( ptr + index );

        a -= src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }

        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}


void gfx_v_array_mul( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    uint16_t *ptr = _gfx_u16p_get_array_ptr( attr );
    
    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = *( ptr + index );

        a *= src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }

        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}


void gfx_v_array_div( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    uint16_t *ptr = _gfx_u16p_get_array_ptr( attr );
    
    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = *( ptr + index );

        a /= src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }

        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}


void gfx_v_array_mod( uint8_t obj, uint8_t attr, int32_t src ){

    if( obj >= pix_array_count ){

        return;
    }

    uint16_t *ptr = _gfx_u16p_get_array_ptr( attr );
    
    for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

        uint16_t index = i + pix_arrays[obj].index;

        index %= pix_count;

        int32_t a = *( ptr + index );

        a %= src;

        if( attr == PIX_ATTR_HUE ){

            a %= 65536;
        }
        else{

            if( a > 65535 ){

                a = 65535;
            }
            else if( a < 0 ){

                a = 0;
            }
        }

        if( attr == PIX_ATTR_HUE ){

            _gfx_v_set_hue_1d( a, index );
        }
        else if( attr == PIX_ATTR_SAT ){

            _gfx_v_set_sat_1d( a, index );
        }
        else if( attr == PIX_ATTR_HS_FADE ){

            _gfx_v_set_hs_fade_1d( a, index );
        }
        else if( attr == PIX_ATTR_V_FADE ){

            _gfx_v_set_v_fade_1d( a, index );
        }   
        else{

            _gfx_v_set_val_1d( a, index );
        }
    }
}


uint16_t *gfx_u16p_get_hue( void ){

    return hue;
}

uint16_t *gfx_u16p_get_sat( void ){

    return sat;
}

uint16_t *gfx_u16p_get_val( void ){

    return val;
}
#ifndef USE_HSV_BRIDGE
uint8_t *gfx_u8p_get_red( void ){

    return array_red;
}

uint8_t *gfx_u8p_get_green( void ){

    return array_green;
}

uint8_t *gfx_u8p_get_blue( void ){

    return array_blue;
}

uint8_t *gfx_u8p_get_dither( void ){

    return array_misc;
}

uint16_t gfx_u16_get_pix0_red( void ){

    return pix0_16bit_red;
}

uint16_t gfx_u16_get_pix0_green( void ){

    return pix0_16bit_green;
}

uint16_t gfx_u16_get_pix0_blue( void ){

    return pix0_16bit_blue;
}
#endif

void gfx_v_set_background_hsv( int32_t h, int32_t s, int32_t v ){

    for( uint16_t i = 0; i < MAX_PIXELS; i++ ){

        gfx_v_set_hsv( h, s, v, i );
    }
}

static uint16_t calc_index( uint8_t obj, uint16_t x, uint16_t y ){

    if( obj >= pix_array_count ){

        return 0xffff;
    }

    if( pix_transpose ){

        uint16_t temp = y;
        y = x;
        x = temp;
    }

    // interleaving only works on 2D access.
    // 1D will address array linearly along its physical axis.
    if( pix_interleave_x && ( y < 65535 ) ){

        if( y & 1 ){

            x = ( pix_arrays[obj].size_x - 1 ) - x;
        }
    }

    int16_t i = 0;

    if( y == 65535 ){

        // check virtual array
        // note this only works in one dimension
        if( virtual_array_length > 0 ){

            uint16_t sub_array_offset = ( pix_count - pix_arrays[obj].count ) * virtual_array_sub_position;
            uint16_t adjusted_virtual_array_start = virtual_array_start - sub_array_offset;

            uint32_t sub_len = scaled_pix_count / pix_arrays[obj].count;
            uint16_t adjusted_virtual_array_len = scaled_virtual_array_length / sub_len;

            i = x % adjusted_virtual_array_len;

            // check if this index is within our local array
            if( ( i < adjusted_virtual_array_start ) ||
                ( i >= ( adjusted_virtual_array_start + pix_arrays[obj].count ) ) ){

                // return invalid index
                return 0xffff;
            }

            // adjust index to local array
            i -= adjusted_virtual_array_start;

            i %= pix_arrays[obj].count;
        }
        else{
            
            i = x % pix_arrays[obj].count;
        }
    }
    else{

        x %= pix_arrays[obj].size_x;
        y %= pix_arrays[obj].size_y;

        i = x + ( y * pix_arrays[obj].size_x );
    }

    if( pix_arrays[obj].reverse ){

        i = ( pix_arrays[obj].count - 1 ) - i;

        if( i < 0 ){

            return 0xffff;
        }
    }

    uint16_t index = ( i + pix_arrays[obj].index ) % pix_count;

    return index;
}



void gfx_v_set_hsv( int32_t h, int32_t s, int32_t v, uint16_t index ){

    if( h >= 0 ){

        _gfx_v_set_hue_1d( h, index );
    }

    if( s >= 0 ){
        
        _gfx_v_set_sat_1d( s, index );
    }

    if( v >= 0 ){

        _gfx_v_set_val_1d( v, index );
    }
}

void gfx_v_set_hsv_2d( int32_t h, int32_t s, int32_t v, uint16_t x, uint16_t y ){

    uint16_t index = calc_index( 0, x, y );

    if( index >= MAX_PIXELS ){
        return;
    }

    gfx_v_set_hsv( h, s, v, index );
}

void gfx_v_set_hue( uint16_t h, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    if( index >= MAX_PIXELS ){
        return;
    }

    _gfx_v_set_hue_1d( h, index );
}


uint16_t gfx_u16_get_hue( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return target_hue[index];
}

void gfx_v_set_sat( uint16_t s, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    if( index >= MAX_PIXELS ){
        return;
    }

    _gfx_v_set_sat_1d( s, index );
}

uint16_t gfx_u16_get_sat( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return target_sat[index];
}

void gfx_v_set_val( uint16_t v, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    if( index >= MAX_PIXELS ){
        return;
    }

    _gfx_v_set_val_1d( v, index );
}

uint16_t gfx_u16_get_val( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return target_val[index];
}


void gfx_v_set_pval( uint16_t v, uint16_t x, uint16_t y, uint8_t obj, gfx_palette_t *palette ){

    uint16_t index = calc_index( obj, x, y );
    
    if( index >= MAX_PIXELS ){
        return;
    }

    pval[index] = v;

    // skip to first entry to interpolate
    while( palette->pval < v ){

        palette++;
    }

    gfx_palette_t *palette2 = palette + 1;

    if( palette->hue >= 0 ){

        uint16_t hue = util_u16_linear_interp( v, palette->pval, palette->hue, palette2->pval, palette2->hue );
        _gfx_v_set_hue_1d( hue, index );
    }

    if( palette->sat >= 0 ){

        uint16_t sat = util_u16_linear_interp( v, palette->pval, palette->sat, palette2->pval, palette2->sat );
        _gfx_v_set_hue_1d( sat, index );
    }

    if( palette->val >= 0 ){

        uint16_t val = util_u16_linear_interp( v, palette->pval, palette->val, palette2->pval, palette2->val );
        _gfx_v_set_hue_1d( val, index );
    }
}

uint16_t gfx_u16_get_pval( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return pval[index];
}

void gfx_v_set_hs_fade( uint16_t a, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );

    if( index >= MAX_PIXELS ){
        return;
    }

    hs_fade[index] = a;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    hue_step[index] = 0;
    sat_step[index] = 0;
}

uint16_t gfx_u16_get_hs_fade( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return hs_fade[index];
}

void gfx_v_set_v_fade( uint16_t a, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );

    if( index >= MAX_PIXELS ){
        return;
    }

    v_fade[index] = a;

    // reset fader, this will trigger the fader process to recalculate the fader steps.
    val_step[index] = 0;
}

uint16_t gfx_u16_get_v_fade( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;
        
    return v_fade[index];
}


uint16_t gfx_u16_get_is_v_fading( uint16_t x, uint16_t y, uint8_t obj ){

    if( ( x == 65535 ) && ( y == 65535 ) ){

        for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

            uint16_t index = i + pix_arrays[obj].index;

            index %= pix_count;

            if( target_val[i] != val[i] ){

                return 1;
            }
        }

        return 0;
    }
    else{

        uint16_t i = calc_index( obj, x, y );

        if( i < MAX_PIXELS ){        

            if( target_val[i] == val[i] ){

                return 0;
            }
        }

        return 1;    
    }
}


uint16_t gfx_u16_get_is_hs_fading( uint16_t x, uint16_t y, uint8_t obj ){

    if( ( x == 65535 ) && ( y == 65535 ) ){

        for( uint16_t i = 0; i < pix_arrays[obj].count; i++ ){

            uint16_t index = i + pix_arrays[obj].index;

            index %= pix_count;

            if( ( target_hue[i] != hue[i] ) ||
                ( target_sat[i] != sat[i] ) ){

                return 1;
            }
        }

        return 0;
    }
    else{

        uint16_t i = calc_index( obj, x, y );

        if( i < MAX_PIXELS ){        
            if( ( target_hue[i] == hue[i] ) &&
                ( target_sat[i] == sat[i] ) ){

                return 0;
            }
        }

        return 1;    
    }
}


void gfx_v_clear( void ){

    for( uint16_t i = 0; i < pix_count; i++ ){

        gfx_v_set_hsv( -1, -1, 0, i );
    }
}

void gfx_v_reset( void ){

    // initialize pixel arrays to defaults    
    // we do this on the entire array, regardless of pix_count.
    for( uint16_t i = 0; i < MAX_PIXELS; i++ ){

        hue[i] = 0;
        sat[i] = 65535;
        val[i] = 0;

        target_hue[i] = 0;
        target_sat[i] = 65535;
        target_val[i] = 0;

        hue_step[i] = 0;
        sat_step[i] = 0;
        val_step[i] = 0;

        hs_fade[i] = global_hs_fade;
        v_fade[i]  = global_v_fade;
    }

    // reset pixel objects
    pix_array_count = 0;

    gfx_v_init_noise();
}

void gfx_v_init_pixel_arrays( gfx_pixel_array_t *array_ptr, uint8_t count ){

    pix_arrays = array_ptr;
    pix_array_count = count;

    // first array is always the global array, we override with our data
    setup_master_array();  

    for( uint8_t p = 1; p < pix_array_count; p++ ){

        // check if arrays have a 2D grid specified.
        // if not, apply the 1D count to the size_x.

        if( pix_arrays[p].size_y == 0 ){

            pix_arrays[p].size_x = pix_arrays[p].count;
            pix_arrays[p].size_y = 1;
        }
    }
}

void gfx_v_delete_pixel_arrays( void ){

    // process mirrors
    for( uint8_t p = 1; p < pix_array_count; p++ ){

        // check if mirror is set
        if( pix_arrays[p].mirror < 0 ){

            continue;
        }

        uint8_t mirror = pix_arrays[p].mirror;


        int32_t offset = pix_arrays[p].offset;

        // adjust offset if negative
        if( offset < 0 ){

            offset = pix_arrays[mirror].size_x + offset;
        }

        // array "p" is mirroring the array specified by mirror

        for( uint16_t x = 0; x < pix_arrays[p].size_x; x++ ){
            for( uint16_t y = 0; y < pix_arrays[p].size_y; y++ ){
                
                uint16_t index_src = calc_index( mirror, x + offset, y );
                uint16_t index_dst = calc_index( p, x, y );

                if( ( index_src >= pix_count ) || ( index_dst >= pix_count ) ){

                    continue;
                }

                _gfx_v_set_hue_1d( target_hue[index_src], index_dst );
                _gfx_v_set_sat_1d( target_sat[index_src], index_dst );
                _gfx_v_set_val_1d( target_val[index_src], index_dst );

                _gfx_v_set_hs_fade_1d( hs_fade[index_src], index_dst );
                _gfx_v_set_v_fade_1d( v_fade[index_src], index_dst );
            }
        }
    }

    // clear arrays
    pix_arrays = 0;
    pix_array_count = 0;
}

static uint16_t linterp_table_lookup( uint16_t x, uint16_t *table ){

    uint8_t index = x >> 8;

    uint16_t y0 = table[index];
    uint16_t y1;
    uint16_t x0 = index << 8;
    uint16_t x1;

    if( index < 255 ){

        y1 = table[index + 1];
        x1 = ( index + 1 ) << 8;
    }
    else{

        y1 = 65535;
        x1 = 65535;
    }

    uint16_t y = util_u16_linear_interp( x, x0, y0, x1, y1 );

    return y;
}

uint16_t gfx_u16_get_dimmed_val( uint16_t _val ){

    uint16_t x = ( (uint32_t)_val * current_dimmer ) / 65536;

    return linterp_table_lookup( x, dimmer_lookup );
}

uint16_t gfx_u16_get_curved_sat( uint16_t _sat ){

    return linterp_table_lookup( _sat, sat_lookup );
}

void gfx_v_process_faders( void ){

    // update master dimmer
    if( dimmer_step != 0 ){

        int32_t diff = (int32_t)target_dimmer - (int32_t)current_dimmer;

        if( abs32( diff ) < abs16( dimmer_step ) ){

            current_dimmer = target_dimmer;
            dimmer_step = 0;
        }
        else{

            current_dimmer += dimmer_step;
        }
    }

    for( uint16_t i = 0; i < pix_count; i++ ){

        // check if fader step needs to be updated
        if( ( hue_step[i] == 0 ) && ( target_hue[i] != hue[i] ) ){

            int32_t diff, step;

            uint16_t hs_fade_steps = hs_fade[i] / FADER_RATE;

            if( hs_fade_steps <= 1 ){

                hs_fade_steps = 2;
            }

            diff = (int32_t)target_hue[i] - (int32_t)hue[i];

            // adjust to shortest distance and allow the fade to wrap around
            // the hue circle
            if( abs32(diff) > 32768 ){

                if( diff > 0 ){

                    diff -= 65536;
                }
                else{

                    diff += 65536;
                }
            }

            step = diff / hs_fade_steps;

            if( step > 32768 ){

                step = 32768;
            }
            else if( step < -32767 ){

                step = -32767;
            }
            else if( step == 0 ){

                if( diff >= 0 ){

                    step = 1;
                }
                else{

                    step = -1;
                }
            }

            hue_step[i] = step;
        }

        if( hue_step[i] != 0 ){

            uint16_t h = hue[i];
            uint16_t th = target_hue[i];
            int16_t step_h = hue_step[i];

            int32_t diff = (int32_t)th - (int32_t)h;

            if( abs32( diff ) < abs16( step_h ) ){

                hue[i] = th;
                hue_step[i] = 0;
            }
            else{

                hue[i] += step_h;
            }
        }

        // check if fader step needs to be updated
        if( ( sat_step[i] == 0 ) && ( target_sat[i] != sat[i] ) ){

            int32_t diff, step;

            uint16_t hs_fade_steps = hs_fade[i] / FADER_RATE;

            if( hs_fade_steps <= 1 ){

                hs_fade_steps = 2;
            }

            diff = (int32_t)target_sat[i] - (int32_t)sat[i];
            step = diff / hs_fade_steps;

            if( step > 32768 ){

                step = 32768;
            }
            else if( step < -32767 ){

                step = -32767;
            }
            else if( step == 0 ){

                if( diff >= 0 ){

                    step = 1;
                }
                else{

                    step = -1;
                }
            }

            sat_step[i] = step;
        }

        if( sat_step[i] != 0 ){

            uint16_t s = sat[i];
            uint16_t ts = target_sat[i];
            int16_t step_s = sat_step[i];

            int32_t diff = (int32_t)ts - (int32_t)s;

            if( abs32( diff ) < abs16( step_s ) ){

                sat[i] = ts;
                sat_step[i] = 0;
            }
            else{

                sat[i] += step_s;
            }
        }

        // check if fader step needs to be updated
        if( ( val_step[i] == 0 ) && ( target_val[i] != val[i] ) ){

            int32_t diff, step;

            uint16_t v_fade_steps = v_fade[i] / FADER_RATE;

            if( v_fade_steps <= 1 ){

                v_fade_steps = 2;
            }

            diff = (int32_t)target_val[i] - (int32_t)val[i];
            step = diff / v_fade_steps;

            if( step > 32768 ){

                step = 32768;
            }
            else if( step < -32767 ){

                step = -32767;
            }
            else if( step == 0 ){

                if( diff >= 0 ){

                    step = 1;
                }
                else{

                    step = -1;
                }
            }

            val_step[i] = step;   
        }

        if( val_step[i] != 0 ){

            uint16_t v = val[i];
            uint16_t tv = target_val[i];
            int16_t step_v = val_step[i];

            int32_t diff = (int32_t)tv - (int32_t)v;

            if( abs32( diff ) < abs16( step_v ) ){

                val[i] = tv;
                val_step[i] = 0;
            }
            else{

                val[i] += step_v;
            }
        }
    }
}

void gfxlib_v_init( void ){

    param_error_check();

    compute_dimmer_lookup();
    compute_sat_lookup();

    // initialize pixel arrays to defaults
    gfx_v_reset();

    update_master_fader();
}

// convert all HSV to RGB
void gfx_v_sync_array( void ){

    #ifdef USE_HSV_BRIDGE

    #else

    uint16_t r, g, b, w;
    uint8_t dither;
    uint16_t dimmed_val;
    uint16_t curved_sat;

    // PWM modes will use pixel 0 and need 16 bits.
    // for simplicity's sake, and to avoid a compare-branch in the
    // HSV converversion loop, we'll just always compute the 16 bit values
    // here, and then go on with the 8 bit arrays.

    dimmed_val = gfx_u16_get_dimmed_val( val[0] );
    curved_sat = gfx_u16_get_curved_sat( sat[0] );

    gfx_v_hsv_to_rgb(
        hue[0],
        sat[0],
        dimmed_val,
        &pix0_16bit_red,
        &pix0_16bit_green,
        &pix0_16bit_blue
    );

    if( pix_mode == PIX_MODE_SK6812_RGBW ){

        for( uint16_t i = 0; i < pix_count; i++ ){

            // process master dimmer
            dimmed_val = gfx_u16_get_dimmed_val( val[i] );
            curved_sat = gfx_u16_get_curved_sat( sat[i] );

            gfx_v_hsv_to_rgbw(
                hue[i],
                curved_sat,
                dimmed_val,
                &r,
                &g,
                &b,
                &w
            );
      
            r /= 256;
            g /= 256;
            b /= 256;
            w /= 256;
        
            array_red[i] = r;
            array_green[i] = g;
            array_blue[i] = b;
            array_misc[i] = w;
        }
    }
    else{

        for( uint16_t i = 0; i < pix_count; i++ ){

            // process master dimmer
            dimmed_val = gfx_u16_get_dimmed_val( val[i] );
            curved_sat = gfx_u16_get_curved_sat( sat[i] );

            gfx_v_hsv_to_rgb(
                hue[i],
                curved_sat,
                dimmed_val,
                &r,
                &g,
                &b
            );
      
            r /= 64;
            g /= 64;
            b /= 64;

            dither =  ( r & 0x0003 ) << 4;
            dither |= ( g & 0x0003 ) << 2;
            dither |= ( b & 0x0003 );

            r /= 4;
            g /= 4;
            b /= 4;
        
            array_red[i] = r;
            array_green[i] = g;
            array_blue[i] = b;
            array_misc[i] = dither;
        }
    }

    #endif
}


// Value noise implementation

void gfx_v_init_noise( void ){

    for( uint32_t i = 0; i < NOISE_TABLE_SIZE; i++ ){

        noise_table[i] = rnd_u8_get_int();
    }
}

static uint16_t lerp( uint8_t low, uint8_t high, uint8_t t ){

    return ( (uint16_t)low * ( 256 - t ) + (uint16_t)high * t );
}

uint16_t gfx_u16_noise( uint16_t x ){

    // get floor
    uint8_t x_min = x >> 8;
        
    // get fractional part
    uint8_t t = x & 0xff;

    t = smootherstep_lookup[t];

    return lerp( noise_table[x_min], noise_table[x_min + 1], t );
}




/*
Other HSV to RGB code for reference
*/

//
// void uint16_hsv_to_rgb( uint16_t h, uint16_t s, uint16_t v, uint16_t *r, uint16_t *g, uint16_t *b ){
//
// // 883 cycles = 27.59 uS
//
//     uint16_t fpart, p, q, t;
//
//     if( h > 65531 ){
//
//         h = 0;
//     }
//
//     if( ( s == 0 ) || ( v == 0 ) ){
//
//         // color is grayscale
//         *r = *g = *b = v;
//
//         return;
//     }
//
//     // 77 cycles
//
//     // make hue 0-5
//     uint8_t region = h / ( 65536 / 6 );
//
//     // 282 delta = 205 (approx 6 us)
//
//     // find remainder part, make it from 0-65535
//     fpart = ( h - ( region * ( 65536 / 6 ) ) ) * 6;
//
//     // 351 delta = 69
//
//     // calculate temp vars, doing integer multiplication
//     p = ( (uint32_t)v * ( 65536 - s ) ) >> 16;
//
//     // 478 delta = 127
//
//     q = ( (uint32_t)v * ( 65536 - ( ( (uint32_t)s * fpart ) >> 16 ) ) ) >> 16;
//
//     // 605 delta = 127
//
//     t = ( (uint32_t)v * ( 65536 - ( ( (uint32_t)s * ( 65536 - fpart ) ) >> 16 ) ) ) >> 16;
//
//     // 785 delta = 180
//
//
//     // assign temp vars based on color cone region
//     switch(region) {
//
//         case 0:
//             *r = v;
//             *g = t;
//             *b = p;
//             break;
//
//         case 1:
//             *r = q;
//             *g = v;
//             *b = p;
//             break;
//
//         case 2:
//             *r = p;
//             *g = v;
//             *b = t;
//             break;
//
//         case 3:
//             *r = p;
//             *g = q;
//             *b = v;
//             break;
//
//         case 4:
//             *r = t;
//             *g = p;
//             *b = v;
//             break;
//
//         default:
//             *r = v;
//             *g = p;
//             *b = q;
//             break;
//     }
// }
//
//
// void uint16_hsv_to_rgb2( uint16_t h, uint8_t s, uint8_t v, uint16_t *r, uint16_t *g, uint16_t *b ){
//
//     // 225 cycles = 7.03 uS
//
//     uint16_t temp_r, temp_g, temp_b, temp_s;
//
//     temp_s = 65535 - ( s * 257 );
//
//     if( h <= 32767 ){
//
//         if( h <= 10922 ){
//
//             temp_r = 65535;
//             temp_g = h * 6;
//             temp_b = 0;
//         }
//         else if( h <= 21845 ){
//
//             temp_r = ( 21845 - h ) * 6;
//             temp_g = 65535;
//             temp_b = 0;
//         }
//         else{
//
//             temp_r = 0;
//             temp_g = 65535;
//             temp_b = ( h - 21845 ) * 6;
//         }
//     }
//     else{
//
//         if( h <= 43690 ){
//
//             temp_r = 0;
//             temp_g = ( 43690 - h ) * 6;
//             temp_b = 65535;
//         }
//         else if( h <= 54612 ){
//
//             temp_r = ( h - 43690 ) * 6;
//             temp_g = 0;
//             temp_b = 65535;
//         }
//         else{
//
//             temp_r = 65535;
//             temp_g = 0;
//             temp_b = ( 65535 - h ) * 6;
//         }
//
//     }
//
//     // floor saturation
//     if( temp_r < temp_s ){
//
//         temp_r = temp_s;
//     }
//
//     if( temp_g < temp_s ){
//
//         temp_g = temp_s;
//     }
//
//     if( temp_b < temp_s ){
//
//         temp_b = temp_s;
//     }
//
//     if( v < 255 ){
//
//         // v++;
//
//         *r = ( (uint32_t)temp_r * v ) / 256;
//         *g = ( (uint32_t)temp_g * v ) / 256;
//         *b = ( (uint32_t)temp_b * v ) / 256;
//     }
//     else{
//
//         *r = temp_r;
//         *g = temp_g;
//         *b = temp_b;
//     }
// }
//
// void uint16_hsv_to_rgb3( uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b ){
//
//     // approx 150-160 cycles = 5 uS
//
//     uint8_t temp_s = 255 - s;
//
//     if( h <= 32767 ){
//
//         if( h <= 10922 ){
//
//             *r = 255;
//             *g = ( h * 6 ) / 256;
//             *b = 0;
//         }
//         else if( h <= 21845 ){
//
//             *r = ( ( 21845 - h ) * 6 ) / 256;
//             *g = 255;
//             *b = 0;
//         }
//         else{
//
//             *r = 0;
//             *g = 255;
//             *b = ( ( h - 21845 ) * 6 ) / 256;
//         }
//     }
//     else{ // if( h & 0x8000 )
//
//         if( h <= 43690 ){
//
//             *r = 0;
//             *g = ( ( 43690 - h ) * 6 ) / 256;
//             *b = 255;
//         }
//         else if( h <= 54612 ){
//
//             *r = ( ( h - 43690 ) * 6 ) / 256;
//             *g = 0;
//             *b = 255;
//         }
//         else{
//
//             *r = 255;
//             *g = 0;
//             *b = ( ( 65535 - h ) * 6 ) / 256;
//         }
//
//     }
//
//     // floor saturation
//     if( *r < temp_s ){
//
//         *r = temp_s;
//     }
//
//     if( *g < temp_s ){
//
//         *g = temp_s;
//     }
//
//     if( *b < temp_s ){
//
//         *b = temp_s;
//     }
//
//     *r = ( (uint16_t)*r * v ) / 256;
//     *g = ( (uint16_t)*g * v ) / 256;
//     *b = ( (uint16_t)*b * v ) / 256;
// }
//
// // same as uint16_hsv_to_rgb3, but skips the value computation.
// // RGB will return at full brightness
// void uint16_hsv_to_rgb4( uint16_t h, uint8_t s, uint8_t *r, uint8_t *g, uint8_t *b ){
//
//     uint8_t temp_s = 255 - s;
//
//     if( h <= 32767 ){
//
//         if( h <= 10922 ){
//
//             *r = 255;
//             *g = ( h * 6 ) / 256;
//             *b = 0;
//         }
//         else if( h <= 21845 ){
//
//             *r = ( ( 21845 - h ) * 6 ) / 256;
//             *g = 255;
//             *b = 0;
//         }
//         else{
//
//             *r = 0;
//             *g = 255;
//             *b = ( ( h - 21845 ) * 6 ) / 256;
//         }
//     }
//     else{ // if( h & 0x8000 )
//
//         if( h <= 43690 ){
//
//             *r = 0;
//             *g = ( ( 43690 - h ) * 6 ) / 256;
//             *b = 255;
//         }
//         else if( h <= 54612 ){
//
//             *r = ( ( h - 43690 ) * 6 ) / 256;
//             *g = 0;
//             *b = 255;
//         }
//         else{
//
//             *r = 255;
//             *g = 0;
//             *b = ( ( 65535 - h ) * 6 ) / 256;
//         }
//
//     }
//
//     // floor saturation
//     if( *r < temp_s ){
//
//         *r = temp_s;
//     }
//
//     if( *g < temp_s ){
//
//         *g = temp_s;
//     }
//
//     if( *b < temp_s ){
//
//         *b = temp_s;
//     }
// }
//
//
// // hue (h) is 0 to 359 degrees mapped onto 0.0 to 1.0
// // saturation (s) is 0.0 to 1.0
// // value (v) is 0.0 to 1.0
// void pixel_v_hsv_to_rgb( uint16_t h, uint16_t s, uint16_t v, uint16_t *r, uint16_t *g, uint16_t *b ){
//
// // 2789 cycles = 87.16 uS
//
//     float hf = (float)h / 65536.0;
//     float sf = (float)s / 65536.0;
//     float vf = (float)v / 65536.0;
//     float rf, gf, bf;
//
//     // hue indicates which sextant of the color cylinder we're in
//     hf *= 6;
//
//     // this gets the integer sextant
//     uint8_t i = floor( hf );
//
//     float f = hf - i;
//
//     float p = vf * ( 1.0 - sf );
//
//     float q = vf * ( 1.0 - f * sf );
//
//     float t = vf * ( 1.0 - ( 1.0 - f ) * sf );
//
//     switch( i ){
//
//         case 0:
//             rf = vf;
//             gf = t;
//             bf = p;
//             break;
//
//         case 1:
//             rf = q;
//             gf = vf;
//             bf = p;
//             break;
//
//         case 2:
//             rf = p;
//             gf = vf;
//             bf = t;
//             break;
//
//         case 3:
//             rf = p;
//             gf = q;
//             bf = vf;
//             break;
//
//         case 4:
//             rf = t;
//             gf = p;
//             bf = vf;
//             break;
//
//         case 5:
//             rf = vf;
//             gf = p;
//             bf = q;
//             break;
//     }
//
//     // scale to 16 bits
//     *r = (uint16_t)( rf * 65535.0 );
//     *g = (uint16_t)( gf * 65535.0 );
//     *b = (uint16_t)( bf * 65535.0 );
// }


// old/experimental graphics stuff:

// static uint8_t pen_mode;
// #define H_ON 0x01
// #define S_ON 0x02
// #define V_ON 0x04
//
// #define PEN_DOWN 0x80

// static uint8_t coord_mode;
// #define COORD_MODE_ABS 0
// #define COORD_MODE_REL 1

// static uint16_t pen_hue, pen_sat, pen_val;
// static bool wrap;


// void gfx_v_set_fader_rate( uint8_t setting ){
//
//     pix_fader_rate = setting;
// }
//
// uint8_t gfx_u8_get_fader_rate( void ){
//
//     return pix_fader_rate;
// }


// void gfx_v_set_pen( int32_t h, int32_t s, int32_t v ){
//
//     pen_mode &= PEN_DOWN;
//
//     if( h >= 0 ){
//
//         pen_hue = h;
//         pen_mode |= H_ON;
//     }
//
//     if( s >= 0 ){
//
//         pen_sat = s;
//         pen_mode |= S_ON;
//     }
//
//     if( v >= 0 ){
//
//         pen_val = v;
//         pen_mode |= V_ON;
//     }
// }
//
// void gfx_v_pen_down( void ){
//
//     pen_mode |= PEN_DOWN;
// }
//
// void gfx_v_pen_up( void ){
//
//     pen_mode &= ~PEN_DOWN;
// }
//
// void gfx_v_coord_abs( void ){
//
//     coord_mode = COORD_MODE_ABS;
// }
//
// void gfx_v_coord_rel( void ){
//
//     coord_mode = COORD_MODE_REL;
// }
//
// void gfx_v_wrap_on( void ){
//
//     wrap = TRUE;
// }
//
// void gfx_v_wrap_off( void ){
//
//     wrap = FALSE;
// }




// void gfx_v_drawpoint( uint16_t x, uint16_t y ){
//
//     if( ( pen_mode & PEN_DOWN ) == 0 ){
//
//         return;
//     }
//
//     // bounds check
//     if( x >= pix_size_x ){
//
//         if( wrap ){
//
//             x %= pix_size_x;
//         }
//         else{
//
//             return;
//         }
//     }
//
//     if( y >= pix_size_y ){
//
//         if( wrap ){
//
//             y %= pix_size_y;
//         }
//         else{
//
//             return;
//         }
//     }
//
//     if( pen_mode & H_ON ){
//
//         gfx_v_set_hue( pen_hue, x, y );
//     }
//
//     if( pen_mode & S_ON ){
//
//         gfx_v_set_sat( pen_sat, x, y );
//     }
//
//     if( pen_mode & V_ON ){
//
//         gfx_v_set_val( pen_val, x, y );
//     }
// }
//
// static void line_kernel( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1 ){
//
//     bool steep = abs16( y1 - y0 ) > abs16( x1 - x0 );
//     uint16_t temp;
//
//     if( steep ){
//
//         temp = x0;
//         x0 = y0;
//         y0 = temp;
//
//         temp = x1;
//         x1 = y1;
//         y1 = temp;
//     }
//
//     if( x0 > x1 ){
//
//         temp = x1;
//         x1 = x0;
//         x0 = temp;
//
//         temp = y1;
//         y1 = y0;
//         y0 = temp;
//     }
//
//     // Bresenham's algorithm
//     uint16_t dx = x1 - x0;
//     uint16_t dy = abs16( (int16_t)y1 - (int16_t)y0 );
//
//     int16_t e = dx / 2;
//     uint16_t y_step = -1;
//
//     if( y0 < y1 ){
//
//         y_step = 1;
//     }
//
//     uint16_t count = 0;
//
//     while( x0 <= x1 ){
//
//         if( steep ){
//
//             gfx_v_drawpoint( y0, x0 );
//         }
//         else{
//
//             gfx_v_drawpoint( x0, y0 );
//         }
//
//         e -= dy;
//
//         if( e < 0 ){
//
//             y0 += y_step;
//             e += dx;
//         }
//
//         x0++;
//
//         count++;
//     }
// }
//
// void gfx_v_drawline( uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1 ){
//
//     if( coord_mode == COORD_MODE_REL ){
//
//         // convert to pixel coordinates
//         x0 = ( (uint32_t)x0 * pix_size_x ) / 65536;
//         y0 = ( (uint32_t)y0 * pix_size_y ) / 65536;
//         x1 = ( (uint32_t)x1 * pix_size_x ) / 65536;
//         y1 = ( (uint32_t)y1 * pix_size_y ) / 65536;
//     }
//
//     line_kernel( x0, y0, x1, y1 );
// }
//
// void gfx_v_drawbox( int32_t x0, int32_t y0, int32_t x1, int32_t y1 ){
//
//     if( coord_mode == COORD_MODE_REL ){
//
//         // convert to pixel coordinates
//         x0 = ( x0 * pix_size_x ) / 65536;
//         y0 = ( y0 * pix_size_y ) / 65536;
//         x1 = ( x1 * pix_size_x ) / 65536;
//         y1 = ( y1 * pix_size_y ) / 65536;
//     }
//
//     while( x0 < 0 ){
//
//         x0 += pix_size_x;
//     }
//
//     while( x1 < 0 ){
//
//         x1 += pix_size_x;
//     }
//
//     while( y0 < 0 ){
//
//         y0 += pix_size_y;
//     }
//
//     while( y1 < 0 ){
//
//         y1 += pix_size_y;
//     }
//
//     int16_t dx = ( x1 - x0 ) + 1;
//     int16_t dy;
//     uint16_t x = x0;
//     uint16_t y;
//
//     while( dx != 0 ){
//
//         y = y0;
//         dy = ( (int16_t)y1 - (int16_t)y0 ) + 1;
//
//         while( dy != 0 ){
//
//             gfx_v_drawpoint( x, y );
//
//             if( dy > 0 ){
//
//                 y++;
//                 dy--;
//             }
//             else{
//
//                 y--;
//                 dy++;
//             }
//         }
//
//         if( dx > 0 ){
//
//             x++;
//             dx--;
//         }
//         else{
//
//             x--;
//             dx++;
//         }
//     }
// }
//
// void gfx_v_drawcircle( uint16_t x0, uint16_t y0, uint16_t radius ){
//
//     if( coord_mode == COORD_MODE_REL ){
//
//         // convert to pixel coordinates
//         x0 = ( (uint32_t)x0 * pix_size_x ) / 65536;
//         y0 = ( (uint32_t)y0 * pix_size_y ) / 65536;
//
//         radius = ( ( (uint32_t)radius + 1 ) * pix_size_x ) / 65536;
//     }
//
//     int16_t x = radius;
//     int16_t y = 0;
//     int16_t err = 0;
//
//     while (x >= y)
//     {
//         gfx_v_drawpoint( x0 + x, y0 + y );
//         gfx_v_drawpoint( x0 + y, y0 + x );
//         gfx_v_drawpoint( x0 - y, y0 + x );
//         gfx_v_drawpoint( x0 - x, y0 + y );
//         gfx_v_drawpoint( x0 - x, y0 - y );
//         gfx_v_drawpoint( x0 - y, y0 - x );
//         gfx_v_drawpoint( x0 + y, y0 - x );
//         gfx_v_drawpoint( x0 + x, y0 - y );
//
//         y += 1;
//         err += 1 + 2 * y;
//
//         if( 2 * ( err - x ) + 1 > 0 ){
//
//             x -= 1;
//             err += 1 - 2 * x;
//         }
//     }
// }
//
// // calculate a move along an angle and draw a connecting line
// void gfx_v_move(
//     uint16_t in_x,
//     uint16_t in_y,
//     uint16_t distance,
//     uint16_t angle,
//     uint16_t *out_x,
//     uint16_t *out_y ){
//
//     int32_t dest_x, dest_y;
//     int32_t dist_x, dist_y;
//
//     dist_x = ( (int32_t)distance * cosine( angle ) ) / 32768;
//     dist_y = ( (int32_t)distance * sine( angle ) ) / 32768;
//     dest_x = dist_x + in_x;
//     dest_y = dist_y + in_y;
//
//     *out_x = dest_x;
//     *out_y = dest_y;
//
//     // check if pen is up, if so, bail out
//     if( ( pen_mode & PEN_DOWN ) == 0 ){
//
//         return;
//     }
//
//     int16_t dist_x_px, dist_y_px;
//
//     if( coord_mode == COORD_MODE_REL ){
//
//         dist_x_px = ( dist_x * pix_size_x ) / 65536;
//         dist_y_px = ( dist_y * pix_size_y ) / 65536;
//     }
//     else{
//
//         dist_x_px = dist_x;
//         dist_y_px = dist_y;
//     }
//
//     int16_t x0_px, y0_px, x1_px, y1_px;
//
//     if( coord_mode == COORD_MODE_REL ){
//
//         x0_px = ( (uint32_t)in_x * pix_size_x ) / 65536;
//         y0_px = ( (uint32_t)in_y * pix_size_y ) / 65536;
//         x1_px = x0_px + dist_x_px;
//         y1_px = y0_px + dist_y_px;
//     }
//     else{
//
//         x0_px = in_x;
//         y0_px = in_y;
//         x1_px = x0_px + dist_x_px;
//         y1_px = y0_px + dist_y_px;
//     }
//
//     while( x0_px < 0 ){
//
//         x0_px += pix_size_x;
//     }
//
//     while( x1_px < 0 ){
//
//         x1_px += pix_size_x;
//     }
//
//     while( y0_px < 0 ){
//
//         y0_px += pix_size_y;
//     }
//
//     while( y1_px < 0 ){
//
//         y1_px += pix_size_y;
//     }
//
//     line_kernel( x0_px, y0_px, x1_px, y1_px );
// }
//
// void gfx_v_fill( void ){
//
//     for( uint8_t x = 0; x < pix_size_x; x++ ){
//
//         for( uint8_t y = 0; y < pix_size_y; y++ ){
//
//             gfx_v_drawpoint( x, y );
//         }
//     }
// }

#endif

