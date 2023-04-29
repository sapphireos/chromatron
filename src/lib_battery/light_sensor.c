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


#include "sapphire.h"

#include "light_sensor.h"

#include "veml7700.h"


#define FILTER_RATIO 8

// long term average of filtered ALS input
static uint32_t filtered_light;
static int32_t current_delta;

KV_SECTION_META kv_meta_t light_sensor_kv[] = {
    {CATBUS_TYPE_UINT32,     0, KV_FLAGS_READ_ONLY, &filtered_light,	0, "light_level"},   
    {CATBUS_TYPE_INT32,      0, KV_FLAGS_READ_ONLY, &current_delta,		0, "light_delta"},   
};

PT_THREAD( light_sensor_thread( pt_t *pt, void *state ) );


void light_sensor_v_init( void ){

	// veml7700 is initialized separately!
	
	// veml7700_v_init();

    thread_t_create( light_sensor_thread,
                 PSTR("light_sensor"),
                 0,
                 0 );
}

uint32_t light_sensor_u32_read( void ){

	return filtered_light;
}


uint32_t light_sensor_u32_read_delta( void ){

	return current_delta;
}


PT_THREAD( light_sensor_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );

	static uint8_t counter;
	counter = 0;

	// init filter
	filtered_light = veml7700_u32_read_als();

	
	while(1){

		TMR_WAIT( pt, 4000 );

		uint32_t light = veml7700_u32_read_als();

		uint32_t temp = util_u32_ewma( light, filtered_light, FILTER_RATIO );

		counter++;

		if( counter >= 15 ){ // approx 1 minute at 4 second rate

			counter = 0;

			current_delta = temp - (int32_t)filtered_light;
		}

		filtered_light = temp;
	}

PT_END( pt );	
}

