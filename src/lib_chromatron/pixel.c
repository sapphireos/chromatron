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


#include "sapphire.h"

#include "config.h"
#include "pixel.h"
#include "graphics.h"
#include "pwm.h"

#include "logging.h"
#include "hal_pixel.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

static bool pix_dither;
static uint8_t pix_mode;
static uint8_t pix_clock;
static uint8_t pix_rgb_order;

static uint8_t pix_apa102_dimmer = 31;


int8_t pix_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__pix_apa102_dimmer ){

            // bounds check the apa102 dimmer
            if( pix_apa102_dimmer > 31 ){

                pix_apa102_dimmer = 31;
            }
        }

        #ifdef ENABLE_COPROCESSOR
        coproc_i32_call1( OPCODE_PIX_SET_RGB_ORDER, pix_rgb_order );
        coproc_i32_call1( OPCODE_PIX_SET_CLOCK, pix_clock );
        coproc_i32_call1( OPCODE_PIX_SET_DITHER, pix_dither );
        coproc_i32_call1( OPCODE_PIX_SET_MODE, pix_mode );
        coproc_i32_call1( OPCODE_PIX_SET_APA102_DIM, pix_apa102_dimmer );
        #endif
    }

    return 0;
}


KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_rgb_order,       pix_i8_kv_handler,    "pix_rgb_order" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST, &pix_dither,          pix_i8_kv_handler,    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST, &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

void hal_pixel_v_transfer_complete_callback( uint8_t driver ){

}


PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        THREAD_WAIT_WHILE( pt, pix_mode == PIX_MODE_OFF );

        #ifdef ENABLE_COPROCESSOR
        
        uint16_t pix_count = gfx_u16_get_pix_count();
        uint8_t *r = gfx_u8p_get_red();
        uint8_t *g = gfx_u8p_get_green();
        uint8_t *b = gfx_u8p_get_blue();
        uint8_t *d = gfx_u8p_get_dither();

        uint16_t index = 0;
        while( pix_count > 0 ){

            uint32_t buf[17];

            uint8_t copy_len = cnt_of_array(buf) - 1;
            
            if( copy_len > pix_count ){

                copy_len = pix_count;
            }

            buf[0] = index;

            for( uint8_t i = 0; i < copy_len; i++ ){

                buf[i + 1]  = ( (uint32_t)*r << 24 );
                buf[i + 1] |= ( (uint32_t)*g << 16 );
                buf[i + 1] |= ( (uint32_t)*b << 8  );
                buf[i + 1] |= ( (uint32_t)*d << 0  );

                r++;
                g++;
                b++;
                d++;
            }

            coproc_u8_issue( OPCODE_PIX_LOAD, (uint8_t *)buf, ( copy_len + 1 ) * sizeof(uint32_t) );

            index += copy_len;
            pix_count -= copy_len;
        }

        
        #endif        
    }

PT_END( pt );
}

void pixel_v_init( void ){

    hal_pixel_v_init();

    thread_t_create( pixel_thread,
                     PSTR("pixel"),
                     0,
                     0 );
}
