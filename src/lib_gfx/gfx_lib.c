// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include <inttypes.h>
#include "bool.h"
#include "trig.h"
#include "util.h"
#include "random.h"
#include "pix_modes.h"
#include "kvdb.h"
#include "hsv_to_rgb.h"
#include "keyvalue.h"
#include "gfx_lib.h"
#include "vm.h"
#include "battery.h"
#include "pixel_vars.h"

#ifdef PIXEL_USE_MALLOC
static uint8_t *array_red __attribute__((aligned(4)));
static uint8_t *array_green __attribute__((aligned(4)));
static uint8_t *array_blue __attribute__((aligned(4)));
static uint8_t *array_misc __attribute__((aligned(4)));
#else
static uint8_t array_red[MAX_PIXELS] __attribute__((aligned(4)));
static uint8_t array_green[MAX_PIXELS] __attribute__((aligned(4)));
static uint8_t array_blue[MAX_PIXELS] __attribute__((aligned(4)));
static uint8_t array_misc[MAX_PIXELS] __attribute__((aligned(4)));
#endif

static uint16_t pix0_16bit_red;
static uint16_t pix0_16bit_green;
static uint16_t pix0_16bit_blue;

static uint16_t hue[MAX_PIXELS];
static uint16_t sat[MAX_PIXELS];
static uint16_t val[MAX_PIXELS];
static uint16_t pval[MAX_PIXELS];

static uint16_t target_hue[MAX_PIXELS];
static uint16_t target_sat[MAX_PIXELS];
static uint16_t target_val[MAX_PIXELS];

static uint16_t global_hs_fade = 1000;
static uint16_t global_v_fade = 1000;
static uint16_t dimmer_fade = 1000;
static uint16_t hs_fade[MAX_PIXELS];
static uint16_t v_fade[MAX_PIXELS];

static int16_t hue_step[MAX_PIXELS];
static int16_t sat_step[MAX_PIXELS];
static int16_t val_step[MAX_PIXELS];

static bool gfx_enable = TRUE;
static bool sys_enable = TRUE; // internal system control
static uint16_t pix_master_dimmer = 0;
static uint16_t pix_sub_dimmer = 0;
static uint16_t target_dimmer = 0;
static uint16_t current_dimmer = 0;
static int16_t dimmer_step = 0;

static uint16_t pix_size_x;
static uint16_t pix_size_y;
static bool gfx_interleave_x;
static bool gfx_invert_x;
static bool gfx_transpose;

static uint8_t pix_array_count;
static gfx_pixel_array_t *pix_arrays;

// #define ENABLE_VIRTUAL_ARRAY

#ifdef ENABLE_VIRTUAL_ARRAY
static uint16_t virtual_array_start;
static uint16_t virtual_array_length;
static uint8_t virtual_array_sub_position;
static uint32_t scaled_pix_count;
static uint32_t scaled_virtual_array_length;
#endif

static uint16_t gfx_frame_rate = 100;

static uint8_t dimmer_curve = GFX_DIMMER_CURVE_DEFAULT;
static uint8_t sat_curve = GFX_SAT_CURVE_DEFAULT;

// #define ENABLE_CHANNEL_MASK

#ifdef ENABLE_CHANNEL_MASK
static uint8_t channel_mask;
#endif


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


static uint16_t pix_counts[N_PIXEL_OUTPUTS];
static uint16_t pix_count;

static bool zero_output;

static void update_pix_count( void ){

    pix_count = 0;

    for( uint8_t i = 0; i < N_PIXEL_OUTPUTS; i++ ){

        pix_count += pix_counts[i];
    }
}

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


static void param_error_check( void ){

    // update pix count
    update_pix_count();

    // error check
    if( pix_count > MAX_PIXELS ){

        pix_count = MAX_PIXELS;

        memset( pix_counts, 0, sizeof(pix_counts) );
        pix_counts[0] = MAX_PIXELS;
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


static uint16_t calc_dimmer( void ){

    return ( (uint32_t)pix_master_dimmer * (uint32_t)pix_sub_dimmer ) / 65536;    
}

static void update_master_fader( void ){

    uint16_t fade_steps = dimmer_fade / FADER_RATE;

    if( fade_steps <= 1 ){

        fade_steps = 2;
    }

    if( gfx_enable && sys_enable ){

        target_dimmer = calc_dimmer();
    }
    else{

        target_dimmer = 0;
    }

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


int8_t pix_i8_count_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_SET ){

        param_error_check();
    }

    return 0;
}

KV_SECTION_META kv_meta_t hal_pixel_info_kv[] = {
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[0],        pix_i8_count_handler,    "pix_count" },
    #if N_PIXEL_OUTPUTS > 1
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[1],        pix_i8_count_handler,    "pix_count_1" },
    #endif
    #if N_PIXEL_OUTPUTS > 2
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[2],        pix_i8_count_handler,    "pix_count_2" },
    #endif
    #if N_PIXEL_OUTPUTS > 3
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[3],        pix_i8_count_handler,    "pix_count_3" },
    #endif
    #if N_PIXEL_OUTPUTS > 4
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[4],        pix_i8_count_handler,    "pix_count_4" },
    #endif
    #if N_PIXEL_OUTPUTS > 5
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[5],        pix_i8_count_handler,    "pix_count_5" },
    #endif
};


int8_t gfx_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){

    if( op == KV_OP_SET ){

        if( hash == __KV__gfx_frame_rate ){

            param_error_check();   

            // signal new frame rate to VM
            gfx_vm_v_update_frame_rate( gfx_frame_rate );
        }
        else if( hash == __KV__gfx_hsfade ){

            for( uint16_t i = 0; i < MAX_PIXELS; i++ ){

                hs_fade[i] = global_hs_fade;   
            }
        }
        else if( hash == __KV__gfx_vfade ){

            for( uint16_t i = 0; i < MAX_PIXELS; i++ ){

                v_fade[i] = global_v_fade;   
            }
        }
        else if( hash == __KV__gfx_dimmer_fade ){

            update_master_fader();
        }
        else if( hash == __KV__gfx_dimmer_curve ){

            compute_dimmer_lookup();
        }
        else if( hash == __KV__gfx_sat_curve ){

            compute_sat_lookup();
        }
    }

    return 0;
}

KV_SECTION_META kv_meta_t gfx_lib_info_kv[] = {
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_enable,                  0,                   "gfx_enable" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_sub_dimmer,              0,                   "gfx_sub_dimmer" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_master_dimmer,           0,                   "gfx_master_dimmer" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_x,                  0,                   "pix_size_x" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &pix_size_y,                  0,                   "pix_size_y" },
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_interleave_x,            0,                   "gfx_interleave_x" },
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_invert_x,                0,                   "gfx_invert_x" },
    { CATBUS_TYPE_BOOL,       0, KV_FLAGS_PERSIST, &gfx_transpose,               0,                   "gfx_transpose" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &global_hs_fade,              gfx_i8_kv_handler,   "gfx_hsfade" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &global_v_fade,               gfx_i8_kv_handler,   "gfx_vfade" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &dimmer_fade,                 gfx_i8_kv_handler,   "gfx_dimmer_fade" },
    { CATBUS_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &dimmer_curve,                gfx_i8_kv_handler,   "gfx_dimmer_curve" },
    { CATBUS_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &sat_curve,                   gfx_i8_kv_handler,   "gfx_sat_curve" },
        
    #ifdef ENABLE_VIRTUAL_ARRAY
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &virtual_array_start,         0,                   "gfx_varray_start" },
    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &virtual_array_length,        0,                   "gfx_varray_length" },
    #endif

    { CATBUS_TYPE_UINT16,     0, KV_FLAGS_PERSIST, &gfx_frame_rate,              gfx_i8_kv_handler,   "gfx_frame_rate" },
    
    #ifdef ENABLE_CHANNEL_MASK
    { CATBUS_TYPE_UINT8,      0, KV_FLAGS_PERSIST, &channel_mask,                0,                   "gfx_channel_mask" },
    #endif
};

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

void gfx_v_set_vm_frame_rate( uint16_t frame_rate ){

    gfx_frame_rate = frame_rate;

    param_error_check();
}

uint16_t gfx_u16_get_vm_frame_rate( void ){

    return gfx_frame_rate;
}

// void gfx_v_set_params( gfx_params_t *params ){

//     // version check
//     if( params->version != GFX_VERSION ){

//         return;
//     }

//     uint8_t old_dimmer_curve = dimmer_curve;
//     uint8_t old_sat_curve = sat_curve;

//     dimmer_curve            = params->dimmer_curve;
//     sat_curve               = params->sat_curve;
//     pix_count               = params->pix_count;
//     pix_size_x              = params->pix_size_x;
//     pix_size_y              = params->pix_size_y;
//     gfx_interleave_x        = params->interleave_x;
//     gfx_invert_x            = params->invert_x;
//     gfx_transpose           = params->transpose;
//     pix_mode                = params->pix_mode;
//     global_hs_fade          = params->hs_fade;
//     global_v_fade           = params->v_fade;
//     pix_master_dimmer       = params->master_dimmer;
//     pix_sub_dimmer          = params->sub_dimmer;
//     gfx_frame_rate          = params->frame_rate;

//     #ifdef ENABLE_VIRTUAL_ARRAY
//     virtual_array_start     = params->virtual_array_start;
//     virtual_array_length    = params->virtual_array_length;
//     #endif

//     param_error_check();

//     // only run if dimmer curve is changing
//     if( old_dimmer_curve != dimmer_curve ){
        
//         compute_dimmer_lookup();
//     }

//     // only run if sat curve is changing
//     if( old_sat_curve != sat_curve ){
        
//         compute_sat_lookup();
//     }

//     update_master_fader();

//     // sync_db();

//     #ifdef ENABLE_VIRTUAL_ARRAY
//     virtual_array_sub_position      = virtual_array_start / pix_count;
//     scaled_pix_count                = (uint32_t)pix_count * 65536;
//     scaled_virtual_array_length     = (uint32_t)virtual_array_length * 65536;
//     #endif
// }

// void gfx_v_get_params( gfx_params_t *params ){

//     params->version                 = GFX_VERSION;
//     params->pix_count               = pix_count;
//     params->pix_size_x              = pix_size_x;
//     params->pix_size_y              = pix_size_y;
//     params->interleave_x            = gfx_interleave_x;
//     params->invert_x                = gfx_invert_x;
//     params->transpose               = gfx_transpose;
//     params->pix_mode                = pix_mode;
//     params->hs_fade                 = global_hs_fade;
//     params->v_fade                  = global_v_fade;
//     params->master_dimmer           = pix_master_dimmer;
//     params->sub_dimmer              = pix_sub_dimmer;
//     params->frame_rate              = gfx_frame_rate;
//     params->dimmer_curve            = dimmer_curve;
//     params->sat_curve               = sat_curve;

//     #ifdef ENABLE_VIRTUAL_ARRAY
//     params->virtual_array_start     = virtual_array_start;
//     params->virtual_array_length    = virtual_array_length;
//     #endif
// }

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

        case __KV__sine:
            if( param_len != 1 ){

                break;
            }

            return sine( params[0] );

            break;

        case __KV__cosine:
            if( param_len != 1 ){

                break;
            }

            return cosine( params[0] );

            break;

        case __KV__triangle:
            if( param_len != 1 ){

                break;
            }

            return triangle( params[0] );

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

uint16_t gfx_u16_get_pix_driver_count( uint8_t output ){

    ASSERT( output < N_PIXEL_OUTPUTS );

    return pix_counts[output];
}

uint16_t gfx_u16_get_pix_driver_offset( uint8_t output ){

    ASSERT( output < N_PIXEL_OUTPUTS );

    uint16_t offset = 0;

    for( uint8_t i = 0; i < output; i++ ){

        offset += pix_counts[i];
    }
    
    return offset;
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

void gfx_v_set_hue_1d( uint16_t a, uint16_t index ){

    _gfx_v_set_hue_1d( a, index );
}

uint16_t gfx_u16_get_hue_1d( uint16_t index ){

    index %= pix_count;

    return target_hue[index];
}

void gfx_v_set_sat_1d( uint16_t a, uint16_t index ){

    _gfx_v_set_sat_1d( a, index );
}

uint16_t gfx_u16_get_sat_1d( uint16_t index ){

    index %= pix_count;

    return target_sat[index];
}

void gfx_v_set_val_1d( uint16_t a, uint16_t index ){

    _gfx_v_set_val_1d( a, index );
}

uint16_t gfx_u16_get_val_1d( uint16_t index ){

    index %= pix_count;

    return target_val[index];
}

void gfx_v_set_hs_fade_1d( uint16_t a, uint16_t index ){

    _gfx_v_set_hs_fade_1d( a, index );
}

void gfx_v_set_v_fade_1d( uint16_t a, uint16_t index ){

    _gfx_v_set_v_fade_1d( a, index );
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

static uint16_t calc_index( uint8_t obj, uint16_t x, uint16_t y ){

    if( obj >= pix_array_count ){

        return 0xffff;
    }

    // avoid divide by 0 exceptions:
    if( pix_arrays[obj].count == 0 ){

        return 0;
    }

    if( gfx_transpose ){

        uint16_t temp = y;
        y = x;
        x = temp;
    }

    // x interleave
    // note pix_size_x and pix_size_y must be set correctly for this to work properly
    if( gfx_interleave_x ){

        // if y is not given, we need to figure out which row we're in
        if( y == 65535 ){

            uint16_t row = x / pix_arrays[obj].size_x;
            
            if( row & 1 ){

                uint16_t temp_x = x % pix_arrays[obj].size_x;

                // flip x around
                temp_x = ( pix_arrays[obj].size_x - 1 ) - temp_x;
                x = ( row * pix_arrays[obj].size_x ) + temp_x;
            }
        }   
        // 2D access, this is easier because we already know y
        else{

            if( y & 1 ){

                x = ( pix_arrays[obj].size_x - 1 ) - ( x % pix_arrays[obj].size_x );
            }
        }        
    }

    // invert x axis
    if( gfx_invert_x ){

        // if y is not given, we need to figure out which row we're in
        if( y == 65535 ){

            uint16_t row = x / pix_arrays[obj].size_x;
            uint16_t temp_x = x % pix_arrays[obj].size_x;

            // flip x around
            temp_x = ( pix_arrays[obj].size_x - 1 ) - temp_x;

            x = ( row * pix_arrays[obj].size_x ) + temp_x;
        }   
        else{

            x = ( pix_arrays[obj].size_x - 1 ) - ( x % pix_arrays[obj].size_x );
        }        
    }

    int16_t i = 0;

    if( y == 65535 ){

        #ifdef ENABLE_VIRTUAL_ARRAY
        // check if virtual array enabled
        if( virtual_array_length == 0 ){
        #endif

            // normal mode

            i = x % pix_arrays[obj].count;

        #ifdef ENABLE_VIRTUAL_ARRAY
        }
        else{ 

            // virtual array enabled
            // note this only works in one dimension

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
        #endif
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

uint16_t gfx_u16_calc_index( uint8_t obj, uint16_t x, uint16_t y ){

    return calc_index( obj, x, y );
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

    _gfx_v_set_hs_fade_1d( a, index );
}

uint16_t gfx_u16_get_hs_fade( uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );
    
    index %= MAX_PIXELS;

    return hs_fade[index];
}

void gfx_v_set_v_fade( uint16_t a, uint16_t x, uint16_t y, uint8_t obj ){

    uint16_t index = calc_index( obj, x, y );

    _gfx_v_set_v_fade_1d( a, index );
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

void gfx_v_shutdown_graphic( void ){

    memset( array_red, 0, MAX_PIXELS );
    memset( array_green, 0, MAX_PIXELS );
    memset( array_blue, 0, MAX_PIXELS );
    memset( array_misc, 0, MAX_PIXELS );

    array_red[0] = 16;
    array_green[0] = 16;
    array_blue[0] = 16;    
}

void gfx_v_power_limiter_graphic( void ){

    memset( array_red, 0, MAX_PIXELS );
    memset( array_green, 0, MAX_PIXELS );
    memset( array_blue, 0, MAX_PIXELS );
    memset( array_misc, 0, MAX_PIXELS );

    array_red[0] = 64;
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

int8_t gfx_i8_get_pixel_array( uint8_t obj, gfx_pixel_array_t **array_ptr ){

    if( obj >= pix_array_count ){

        return -1;
   }

   *array_ptr = &pix_arrays[obj];

   return 0;
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

void gfx_v_set_system_enable( bool enable ){

    sys_enable = enable;
}

bool gfx_b_is_system_enabled( void ){

    return sys_enable;
}

void gfx_v_process_faders( void ){

    update_master_fader();

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

    #ifdef PIXEL_USE_MALLOC

    array_red = malloc( MAX_PIXELS );
    array_green = malloc( MAX_PIXELS );
    array_blue = malloc( MAX_PIXELS );
    array_misc = malloc( MAX_PIXELS );

    ASSERT( array_red != 0 );
    ASSERT( array_green != 0 );
    ASSERT( array_blue != 0 );
    ASSERT( array_misc != 0 );

    #endif

    param_error_check();

    compute_dimmer_lookup();
    compute_sat_lookup();

    // initialize pixel arrays to defaults
    gfx_v_reset();

    update_master_fader();
}

bool gfx_b_is_output_zero( void ){

    return zero_output;
}

// convert all HSV to RGB
void gfx_v_sync_array( void ){

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

    zero_output = TRUE;

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

            if( ( r != 0 ) ||
                ( g != 0 ) ||
                ( b != 0 ) ||
                ( w != 0 ) ){

                zero_output = FALSE;     
            }

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

            if( ( r != 0 ) ||
                ( g != 0 ) ||
                ( b != 0 ) ){

                zero_output = FALSE;     
            }
        
            array_red[i] = r;
            array_green[i] = g;
            array_blue[i] = b;
            array_misc[i] = dither;
        }
    }
    
    #ifdef ENABLE_CHANNEL_MASK
    if( channel_mask & 1 ){

        memset( array_red, 0, MAX_PIXELS );
    }

    if( channel_mask & 2 ){

        memset( array_green, 0, MAX_PIXELS );
    }

    if( channel_mask & 4 ){

        memset( array_blue, 0, MAX_PIXELS );
    }

    if( channel_mask & 8 ){

        memset( array_misc, 0, MAX_PIXELS );
    }
    #endif
}


// Value noise implementation

void gfx_v_init_noise( void ){

    // use a fixed seed for the noise table.
    // this ensures synced programs to have synced noise without
    // needing to exchange another RNG seed.
    uint64_t seed = 0x3145761340987315;

    for( uint32_t i = 0; i < NOISE_TABLE_SIZE; i++ ){

        noise_table[i] = rnd_u8_get_int_with_seed( &seed );      
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


uint32_t gfx_u32_get_pixel_r( void ){

    uint32_t total = 0;

    for( uint16_t i = 0; i < pix_count; i++ ){

        total += array_red[i];
    }

    return total;
}

uint32_t gfx_u32_get_pixel_g( void ){

    uint32_t total = 0;

    for( uint16_t i = 0; i < pix_count; i++ ){

        total += array_green[i];
    }

    return total;
}

uint32_t gfx_u32_get_pixel_b( void ){

    uint32_t total = 0;

    for( uint16_t i = 0; i < pix_count; i++ ){

        total += array_blue[i];
    }

    return total;
}


uint32_t gfx_u32_get_pixel_w( void ){

    uint32_t total = 0;

    for( uint16_t i = 0; i < pix_count; i++ ){

        total += array_misc[i];
    }

    return total;
}



#include "fs.h"
PT_THREAD( gfx_v_log_value_curve_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    pix_master_dimmer = 65535;
    pix_sub_dimmer = 65535;
    target_dimmer = 65535;
    current_dimmer = 65535;

    // init output arrays
    gfx_v_reset();

    static gfx_pixel_array_t pix_array;
    gfx_v_init_pixel_arrays( &pix_array, 1 );

    hue[0] = 0;
    sat[0] = 0;
    val[0] = 65535;

    // set fades to something slow
    _gfx_v_set_hs_fade_1d( 4000, 0 );
    _gfx_v_set_v_fade_1d( 4000, 0 );

    gfx_v_sync_array();

    // set full white at zero brightness on index 0
    gfx_v_set_hsv( 0, 0, 0, 0 );

    trace_printf("GFX log start\r\n");

    // static file_t f;
    // f = fs_f_open_P( PSTR("fader_log"), FS_MODE_CREATE_IF_NOT_FOUND );

    // static uint16_t max_iter;
    // max_iter = 256;
    // static uint16_t count;
    // count = 0;



    // while( gfx_u16_get_is_v_fading( 0, 0, 0 ) && ( max_iter > 0 ) ){

    //     trace_printf("FADE: %d: %d,%d,%d,%d\r\n", count, val[0], array_red[0], array_green[0], array_blue[0] );

    //     count++;
    //     max_iter--;

    //     gfx_v_process_faders();
    //     gfx_v_sync_array();

    //     char buf[64];

    //     int16_t len = snprintf_P( buf, sizeof(buf), PSTR("%d,%d,%d,%d,%d\n"), val[0], target_val[0], array_red[0], array_green[0], array_blue[0] );

    //     if( fs_i16_write( f, buf, len ) < len ){

    //         trace_printf("GFX log out of space!\r\n");

    //         break;
    //     }

    //     THREAD_YIELD( pt );
    //     THREAD_YIELD( pt );
    //     THREAD_YIELD( pt );
    //     THREAD_YIELD( pt );
    // }

    // f = fs_f_close( f );

    for( uint16_t i = 0 ; i < cnt_of_array(dimmer_lookup); i++ ){

        trace_printf("dimmer: %d -> %d\r\n", i, dimmer_lookup[i]);
    }

    trace_printf("GFX log end\r\n");

PT_END( pt );
}

void gfx_v_log_value_curve( void ){

    thread_t_create( gfx_v_log_value_curve_thread,
                PSTR("gfx_v_log_value_curve_thread"),
                0,
                0 );
}

/*
dimmer lookup:

0 -> 0
1 -> 1
2 -> 4
3 -> 9
4 -> 16
5 -> 25
6 -> 36
7 -> 49
8 -> 64
9 -> 81
10 -> 100
11 -> 121
12 -> 144
13 -> 169
14 -> 196
15 -> 225
16 -> 256
17 -> 289
18 -> 324
19 -> 361
20 -> 400
21 -> 441
22 -> 484
23 -> 529
24 -> 576
25 -> 625
26 -> 676
27 -> 729
28 -> 784
29 -> 841
30 -> 900
31 -> 961
32 -> 1024
33 -> 1089
34 -> 1156
35 -> 1225
36 -> 1296
37 -> 1369
38 -> 1444
39 -> 1521
40 -> 1600
41 -> 1681
42 -> 1764
43 -> 1849
44 -> 1936
45 -> 2025
46 -> 2116
47 -> 2209
48 -> 2304
49 -> 2401
50 -> 2500
51 -> 2601
52 -> 2704
53 -> 2809
54 -> 2916
55 -> 3025
56 -> 3136
57 -> 3249
58 -> 3364
59 -> 3481
60 -> 3600
61 -> 3721
62 -> 3844
63 -> 3969
64 -> 4096
65 -> 4225
66 -> 4356
67 -> 4489
68 -> 4624
69 -> 4761
70 -> 4900
71 -> 5041
72 -> 5184
73 -> 5329
74 -> 5476
75 -> 5625
76 -> 5776
77 -> 5929
78 -> 6084
79 -> 6241
80 -> 6400
81 -> 6561
82 -> 6724
83 -> 6889
84 -> 7056
85 -> 7225
86 -> 7396
87 -> 7569
88 -> 7744
89 -> 7921
90 -> 8100
91 -> 8281
92 -> 8464
93 -> 8649
94 -> 8836
95 -> 9025
96 -> 9216
97 -> 9409
98 -> 9604
99 -> 9801
100 -> 10000
101 -> 10201
102 -> 10404
103 -> 10609
104 -> 10816
105 -> 11025
106 -> 11236
107 -> 11449
108 -> 11664
109 -> 11881
110 -> 12100
111 -> 12321
112 -> 12544
113 -> 12769
114 -> 12996
115 -> 13225
116 -> 13456
117 -> 13689
118 -> 13924
119 -> 14161
120 -> 14400
121 -> 14641
122 -> 14884
123 -> 15129
124 -> 15376
125 -> 15625
126 -> 15876
127 -> 16129
128 -> 16384
129 -> 16641
130 -> 16900
131 -> 17161
132 -> 17424
133 -> 17689
134 -> 17956
135 -> 18225
136 -> 18496
137 -> 18769
138 -> 19044
139 -> 19321
140 -> 19600
141 -> 19881
142 -> 20164
143 -> 20449
144 -> 20736
145 -> 21025
146 -> 21316
147 -> 21609
148 -> 21904
149 -> 22201
150 -> 22500
151 -> 22801
152 -> 23104
153 -> 23409
154 -> 23716
155 -> 24025
156 -> 24336
157 -> 24649
158 -> 24964
159 -> 25281
160 -> 25600
161 -> 25921
162 -> 26244
163 -> 26569
164 -> 26896
165 -> 27225
166 -> 27556
167 -> 27889
168 -> 28224
169 -> 28561
170 -> 28900
171 -> 29241
172 -> 29584
173 -> 29929
174 -> 30276
175 -> 30625
176 -> 30976
177 -> 31329
178 -> 31684
179 -> 32041
180 -> 32400
181 -> 32761
182 -> 33124
183 -> 33489
184 -> 33856
185 -> 34225
186 -> 34596
187 -> 34969
188 -> 35344
189 -> 35721
190 -> 36100
191 -> 36481
192 -> 36864
193 -> 37249
194 -> 37636
195 -> 38025
196 -> 38416
197 -> 38809
198 -> 39204
199 -> 39601
200 -> 40000
201 -> 40401
202 -> 40804
203 -> 41209
204 -> 41616
205 -> 42025
206 -> 42436
207 -> 42849
208 -> 43264
209 -> 43681
210 -> 44100
211 -> 44521
212 -> 44944
213 -> 45369
214 -> 45796
215 -> 46225
216 -> 46656
217 -> 47089
218 -> 47524
219 -> 47961
220 -> 48400
221 -> 48841
222 -> 49284
223 -> 49729
224 -> 50176
225 -> 50625
226 -> 51076
227 -> 51529
228 -> 51984
229 -> 52441
230 -> 52900
231 -> 53361
232 -> 53824
233 -> 54289
234 -> 54756
235 -> 55225
236 -> 55696
237 -> 56169
238 -> 56644
239 -> 57121
240 -> 57600
241 -> 58081
242 -> 58564
243 -> 59049
244 -> 59536
245 -> 60025
246 -> 60516
247 -> 61009
248 -> 61504
249 -> 62001
250 -> 62500
251 -> 63001
252 -> 63504
253 -> 64009
254 -> 64516
255 -> 65025

*/

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

