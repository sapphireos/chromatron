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

#ifdef ENABLE_PATCH_BOARD

#include "hal_boards.h"
#include "patch_board.h"



static bool gate_enabled;

KV_SECTION_OPT kv_meta_t patch_board_opt_kv[] = {    
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_PERSIST,    0,               0,  "solar_patch_invert_gate" },
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &gate_enabled,   0,  "solar_patch_gate_enabled" },
};


void patchboard_v_init( void ){

    #ifdef ESP32
    kv_v_add_db_info( patch_board_opt_kv, sizeof(patch_board_opt_kv) );

    io_v_set_mode( ELITE_SOLAR_EN_IO, IO_MODE_OUTPUT );    
    io_v_set_mode( ELITE_DC_DETECT_IO, IO_MODE_INPUT );    
    io_v_set_mode( ELITE_PANEL_VOLTS_IO, IO_MODE_INPUT );    
    #endif

    patchboard_v_set_solar_en( FALSE );
}

bool patchboard_b_read_dc_detect( void ){

    #ifdef ESP32
    
    return io_b_digital_read( ELITE_DC_DETECT_IO );

    #else

    return FALSE;

    #endif
}

void patchboard_v_set_solar_en( bool enable ){

    gate_enabled = enable;

    #ifdef ESP32

    if( kv_b_get_boolean( __KV__solar_patch_invert_gate ) ){

        enable = !enable;
    }

    if( enable ){

        io_v_digital_write( ELITE_SOLAR_EN_IO, 1 );
    }
    else{

        io_v_digital_write( ELITE_SOLAR_EN_IO, 0 );
    }

    #endif
}

uint16_t patchboard_u16_read_solar_volts( void ){

    #ifdef ESP32

    uint32_t mv = adc_u16_read_mv( ELITE_PANEL_VOLTS_IO );

    // // adjust for voltage divider
    return ( mv * ( 10 + 22 ) ) / 10;

    #else

    return 0;
    #endif
}
#endif