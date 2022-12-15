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
#include "pix_modes.h"

#if defined(ESP32)

static bool led_detected = TRUE;
static uint64_t led_id;
static uint8_t current_profile;
static uint8_t force_profile;

KV_SECTION_OPT kv_meta_t led_detect_opt_kv[] = {    
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &led_detected,               0,  "led_detected" },
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  &led_id,                     0,  "led_id"},
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &current_profile,            0,  "led_profile"},

    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_PERSIST,    &force_profile,              0,  "led_force_profile" },
};


static const led_profile_t led_profiles[] = {
    {
        LED_UNIT_TYPE_NONE,
        0, // led type
        0, // pix count
        0, // pix size x
        0, // pix size y
    },
    {
        LED_UNIT_TYPE_STRAND50,
        PIX_MODE_WS2811, // led type
        50, // pix count
        50, // pix size x
        1, // pix size y
    },
};

// known units
static const led_unit_t led_units[] = {
    {
        0,
        LED_UNIT_TYPE_NONE,
    },
    {
        1146042388,
        LED_UNIT_TYPE_STRAND50,
    }
};

PT_THREAD( led_detect_thread( pt_t *pt, void *state ) );

void led_detect_v_init( void ){

    led_detected = FALSE;

    kv_v_add_db_info( led_detect_opt_kv, sizeof(led_detect_opt_kv) );

    thread_t_create( led_detect_thread,
                     PSTR("led_detect"),
                     0,
                     0 );
}

bool led_detect_b_led_connected( void ){

    if( force_profile != 0 ){

        return TRUE;
    }

    return led_detected;
}

static const led_profile_t* get_profile_by_type( uint8_t type ){

    // search for profile
    for( int i = 0; i < cnt_of_array(led_profiles); i++ ){

        if( led_profiles[i].unit_type == type ){

            return &led_profiles[i];
        }
    }

    return 0;
}

static const led_profile_t* get_profile_by_id( uint64_t id ){

    // search for unit
    for( int i = 0; i < cnt_of_array(led_units); i++ ){

        if( led_units[i].unit_id == id ){

            // try to load a matching profile
            return get_profile_by_type( led_units[i].unit_type );
        }
    }

    return 0;
}

static void load_profile( uint8_t type ){

    const led_profile_t *profile = get_profile_by_type( type );

    if( profile == 0 ){

        return;
    }

    current_profile = profile->unit_type;


    // set up pixel profile

}


PT_THREAD( led_detect_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 2000 );

        if( force_profile ){

            // if a profile is forced, skip detection and load

            load_profile( force_profile );

            continue;
        }



        onewire_v_init( ELITE_LED_ID_IO );

        TMR_WAIT( pt, 10 );

        bool device_present = onewire_b_reset();

        bool detected = FALSE;
        uint64_t id = 0;
        uint8_t family = 0;

        if( device_present ){        

            bool rom_valid = FALSE;

            rom_valid = onewire_b_read_rom_id( &family, &id );

            if( rom_valid ){

                detected = TRUE;
            }
            else{

                log_v_warn_P( PSTR("Presence detect, but ROM invalid") );
            }
        }

        /*
    
        LED data signal is corrupting the 1 wire signal... need to schedule detection for bus idle periods.

        */


        // LED unit removed
        if( led_detected && !detected ){

            log_v_info_P( PSTR("LED disconnected") );
        }
        else if( led_detected && ( led_id != id ) ){

            led_id = id;

            // changed LED units!
            log_v_info_P( PSTR("LED unit installed: %u"), led_id );

            // get profile
            const led_profile_t *profile = get_profile_by_id( led_id );

            if( profile != 0 ){

                log_v_warn_P( PSTR("Loading profile: %u"), profile->unit_type );

                load_profile( profile->unit_type );
            }
            else{

                log_v_warn_P( PSTR("No profile found!") );
            }
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