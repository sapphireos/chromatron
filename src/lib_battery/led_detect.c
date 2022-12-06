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


#include "sapphire.h"

#include "onewire.h"

#include "led_detect.h"

#include "hal_boards.h"

#if defined(ESP32)

static bool led_detected;
static uint64_t led_id;

KV_SECTION_OPT kv_meta_t led_detect_opt_kv[] = {
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &led_detected,               0,  "led_detected" },
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  &led_id,                     0,  "led_id"},
};


PT_THREAD( led_detect_thread( pt_t *pt, void *state ) );

void led_detect_v_init( void ){

    kv_v_add_db_info( led_detect_opt_kv, sizeof(led_detect_opt_kv) );

    thread_t_create( led_detect_thread,
                     PSTR("led_detect"),
                     0,
                     0 );
}

bool led_detect_b_led_connected( void ){

    return led_detected;
}


PT_THREAD( led_detect_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 2000 );

        onewire_v_init( ELITE_LED_ID_IO );

        TMR_WAIT( pt, 10 );

        bool device_present = onewire_b_reset();

        bool detected = FALSE;
        uint64_t id = 0;
        uint8_t family = 0;

        if( device_present ){        
            
            bool rom_valid = onewire_b_read_rom_id( &family, &id );

            if( rom_valid ){

                detected = TRUE;
            }    
        }


        // LED unit removed
        if( led_detected && !detected ){

            log_v_info_P( PSTR("LED disconnected") );
        }
        else if( led_detected && ( led_id != id ) ){

            led_id = id;

            // changed LED units!
            log_v_info_P( PSTR("LED unit installed: 0x%08x"), led_id );
        }

        if( !detected ){

            led_id = 0;
        }

        led_detected = detected;

        onewire_v_deinit();
    }

PT_END( pt );
}


#else

void led_detect_v_init( void ){

}

#endif