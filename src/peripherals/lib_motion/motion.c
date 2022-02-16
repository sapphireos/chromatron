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

#include "motion.h"

#define MAX_SENSORS 2

static bool motion_detected;
static uint8_t n_sensors;
static uint32_t activity;

KV_SECTION_META kv_meta_t motion_kv[] = {
	{CATBUS_TYPE_UINT8,  0, KV_FLAGS_PERSIST, 		 &n_sensors,    	0, "motion_n_sensors"},   
    {CATBUS_TYPE_BOOL,   0,  KV_FLAGS_READ_ONLY,     &motion_detected,	0, "motion_detected" },
    {CATBUS_TYPE_UINT32, 0,  KV_FLAGS_READ_ONLY,     &activity,     	0, "motion_activity" },
};

static int8_t motion_io[MAX_SENSORS] = {
	-1,
	-1,
};

static uint16_t current_activity[MAX_SENSORS];
static uint16_t activity_index[MAX_SENSORS];
static uint16_t activity_history[MAX_SENSORS][MOTION_ACTIVITY_TIME];


PT_THREAD( motion_io_thread( pt_t *pt, uint8_t *channel ) )
{       	
PT_BEGIN( pt );  

	if( motion_io[*channel] < 0 ){

		THREAD_EXIT( pt );
	}

	while( 1 ){

		TMR_WAIT( pt, MOTION_SCAN_RATE );

		// high when motion is detected
		if( io_b_digital_read( motion_io[*channel] ) ){

			// validate signal with 2nd sample
			if( !motion_detected ){

				TMR_WAIT( pt, MOTION_SCAN_RATE );

				if( !io_b_digital_read( motion_io[*channel] ) ){

					// signal was probably noise

					continue;
				}
			}
		
			motion_detected = TRUE;
			current_activity[*channel]++;
		}
		else{

			motion_detected = FALSE;
		}
	}

PT_END( pt );	
}


PT_THREAD( motion_activity_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
	
	while( 1 ){

		TMR_WAIT( pt, 1000 );

		for( uint8_t i = 0; i < n_sensors; i++ ){

			if( motion_io[i] < 0 ){

				continue;
			}

			activity_history[i][activity_index[i]] = current_activity[i];
			current_activity[i] = 0;
			activity_index[i]++;

			if( activity_index[i] >= cnt_of_array(activity_history[i]) ){

				activity_index[i] = 0;
			}	

			activity = 0;
			for( uint16_t j = 0; j < cnt_of_array(activity_history[i]); j++ ){

				activity += activity_history[i][j];
			}
		}
	}

PT_END( pt );	
}


void motion_v_enable_channel( uint8_t channel, uint8_t gpio ){

	motion_io[channel] = gpio;

	io_v_set_mode( (uint8_t)motion_io[channel], IO_MODE_INPUT_PULLUP );

}

void motion_v_init( void ){

	if( ( n_sensors == 0 ) || ( n_sensors > MAX_SENSORS ) ){

		return;
	}
	
	if( n_sensors >= 1 ){

		uint8_t channel = 0;

		motion_v_enable_channel( 0, IO_PIN_13_A12 );

	    thread_t_create( THREAD_CAST(motion_io_thread),
	                     PSTR("motion_io"),
	                     &channel,
	                     sizeof(channel) );
	}
	
	if( n_sensors >= 2 ){

		log_v_error_P( PSTR("2nd sensor channel is not enabled!") );

		// motion_v_enable_channel( 0, IO_PIN_13_A12 );
	}

   	thread_t_create( motion_activity_thread,
                     PSTR("motion_activity"),
                     0,
                     0 );
}

