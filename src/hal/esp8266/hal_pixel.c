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

// ESP8266

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"
#include "os_irq.h"

#include "logging.h"

#ifdef ENABLE_COPROCESSOR

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
   
}


void hal_pixel_v_init( void ){

}


#else

static uint16_t pix_counts[1];

KV_SECTION_META kv_meta_t hal_pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST, &pix_counts[0],        0,    "pix_count" },
};



uint8_t hal_pixel_u8_driver_count( void ){

    return N_PIXEL_OUTPUTS;
}

uint16_t hal_pixel_u16_driver_pixels( uint8_t driver ){

    ASSERT( driver < N_PIXEL_OUTPUTS );
    
    return pix_counts[driver];
}

uint16_t hal_pixel_u16_driver_offset( uint8_t driver ){

    ASSERT( driver < N_PIXEL_OUTPUTS );

    uint16_t offset = 0;

    for( uint8_t i = 0; i < driver; i++ ){

        offset += pix_counts[i];
    }
    
    return offset;
}

uint16_t hal_pixel_u16_get_pix_count( void ){

    uint16_t count = 0;

    for( uint8_t i = 0; i < N_PIXEL_OUTPUTS; i++ ){

        count += pix_counts[i];
    }

    return count;
}

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
   
}


void hal_pixel_v_init( void ){

    coproc_config();    
}

#endif
