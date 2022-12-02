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

#include "pwm.h"
#include "keyvalue.h"

#include "solar.h"
#include "solar_tilt.h"
#include "battery.h"

#include "bq25895.h"


PT_THREAD( solar_control_thread( pt_t *pt, void *state ) );


// config parameters:
static bool patch_board_installed;
static bool enable_dc_charge = TRUE;
static bool enable_solar_charge;


static uint8_t solar_state;
#define SOLAR_MODE_UNKNOWN				0
#define SOLAR_MODE_DISCHARGE_IDLE		1
#define SOLAR_MODE_DISCHARGE_PIXELS		2
#define SOLAR_MODE_CHARGE_DC			3
#define SOLAR_MODE_CHARGE_SOLAR			4
#define SOLAR_MODE_FULL_CHARGE			5


KV_SECTION_OPT kv_meta_t solar_control_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_state,			0,  "solar_control_state" },

	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&patch_board_installed, 0,  "solar_enable_patch_board" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_dc_charge, 		0,  "solar_enable_dc_charge" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_solar_charge, 	0,  "solar_enable_solar_charge" },
};

void solar_v_init( void ){

	solar_tilt_v_init();

	kv_v_add_db_info( solar_control_opt_kv, sizeof(solar_control_opt_kv) );

	thread_t_create( solar_control_thread,
                     PSTR("solar_control"),
                     0,
                     0 );
}



PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){

		TMR_WAIT( pt, 1000 );


		if( solar_state == SOLAR_MODE_UNKNOWN ){


		}
		else if( solar_state == SOLAR_MODE_DISCHARGE_IDLE ){

			
		}
		else if( solar_state == SOLAR_MODE_DISCHARGE_PIXELS ){

			
		}
		else if( solar_state == SOLAR_MODE_CHARGE_DC ){

			
		}
		else if( solar_state == SOLAR_MODE_CHARGE_SOLAR ){

			
		}
		else if( solar_state == SOLAR_MODE_FULL_CHARGE ){

			
		}
		else{

			ASSERT( FALSE );
		}
	}

PT_END( pt );
}

