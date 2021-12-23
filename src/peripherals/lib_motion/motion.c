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

#define N_SENSORS 1
	
static bool motion_detected;
static uint32_t activity;

KV_SECTION_META kv_meta_t motion_kv[] = {
    { CATBUS_TYPE_BOOL,   0,  KV_FLAGS_READ_ONLY,     &motion_detected,	0, "motion_detected" },
    { CATBUS_TYPE_UINT32, 0,  KV_FLAGS_READ_ONLY,     &activity,     		0, "motion_activity" },
};

static int8_t motion_io[N_SENSORS] = {
	-1,
};

// static bool motion_detected_array[N_SENSORS];

static uint16_t current_activity[N_SENSORS];
static uint16_t activity_index[N_SENSORS];
static uint16_t activity_history[N_SENSORS][MOTION_ACTIVITY_TIME];


PT_THREAD( motion_io_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
	
	while( 1 ){

		TMR_WAIT( pt, MOTION_SCAN_RATE );

		// bool motion_or = FALSE;
		motion_detected = FALSE;

		for( uint8_t i = 0; i < N_SENSORS; i++ ){

			if( motion_io[i] < 0 ){

				continue;
			}

			// high when motion is detected
			if( io_b_digital_read( motion_io[i] ) ){

				// check if we are on an edge, if so, push to KV
				// if( !motion_detected_array[i] ){

				// 	motion_detected_array[i] = TRUE;
				// }

				motion_detected = TRUE;
				// motion_detected_array[i] = TRUE;
				current_activity[i]++;
			}
			else{

				// same, but for falling edge
				// if( motion_detected_array[i] ){

				// 	motion_detected_array[i] = FALSE;
				// }

				// motion_detected_array[i] = FALSE;
			}
		}

		// if( motion_detected != motion_or ){

		// motion_detected = motion_or;
		// }
	}

PT_END( pt );	
}

PT_THREAD( motion_activity_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
	
	while( 1 ){

		TMR_WAIT( pt, 1000 );

		for( uint8_t i = 0; i < N_SENSORS; i++ ){

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

bool motion_b_get_motion( void ){

    return motion_detected;
}

void motion_v_enable_channel( uint8_t channel, uint8_t gpio ){

	motion_io[channel] = gpio;

	io_v_set_mode( (uint8_t)motion_io[channel], IO_MODE_INPUT_PULLUP );

}

void motion_v_init( void ){

	#if defined(ESP32)
	motion_v_enable_channel( 0, IO_PIN_13_A12 );


    thread_t_create( motion_io_thread,
                     PSTR("motion_io"),
                     0,
                     0 );

   	thread_t_create( motion_activity_thread,
                     PSTR("motion_activity"),
                     0,
                     0 );

   	#endif
}

