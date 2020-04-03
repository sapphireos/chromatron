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
    }

    return 0;
}


KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_rgb_order,       0,                    "pix_rgb_order" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_clock,           0,                    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST,                 &pix_dither,          0,                    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode,            0,                    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

static volatile uint16_t channels_complete;

void hal_pixel_v_transfer_complete_callback( uint8_t driver ){

    ATOMIC;
    channels_complete |= ( 1 << driver );
    END_ATOMIC;

    thread_v_signal( PIX_SIGNAL_0 );
}


PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // signal transfer thread to start
    thread_v_signal( PIX_SIGNAL_0 );

    ATOMIC;
    channels_complete = 0xffff;
    END_ATOMIC;

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        THREAD_WAIT_WHILE( pt, pix_mode == PIX_MODE_OFF );

        ATOMIC;
        uint16_t temp_channels_complete = channels_complete;
        channels_complete = 0;
        END_ATOMIC;

        
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
