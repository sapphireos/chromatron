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
#include "battery.h"

#include "bq25895.h"



static int16_t solar_array_tilt_angle;
static int16_t solar_array_target_angle;
static int16_t solar_array_target_delta;


static bool pause_motors = TRUE; // DEBUG default to paused!


KV_SECTION_META kv_meta_t solar_tilt_info_kv[] = {
    
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_PERSIST,  0,             0,  "solar_enable_tilt" },
};

KV_SECTION_OPT kv_meta_t solar_tilt_opt_kv[] = {
    
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_READ_ONLY,  &solar_array_tilt_angle,    0,  "solar_tilt_angle" },
    { CATBUS_TYPE_INT16,    0, 0,  &solar_array_target_angle,	0,  "solar_target_angle" },
    { CATBUS_TYPE_INT16,    0, KV_FLAGS_READ_ONLY,  &solar_array_target_delta,	0,  "solar_target_delta" },
    { CATBUS_TYPE_BOOL,     0, 0,  					&pause_motors,              0,  "solar_pause_motors" },
};


PT_THREAD( solar_tilt_thread( pt_t *pt, void *state ) );


void solar_v_init( void ){

	if( kv_b_get_boolean( __KV__solar_enable_tilt ) ){

		kv_v_add_db_info( solar_tilt_opt_kv, sizeof(solar_tilt_opt_kv) );

		thread_t_create( solar_tilt_thread,
                     PSTR("solar_tilt"),
                     0,
                     0 );
	}
}


static uint16_t read_tilt_sensor_raw( void ){

	uint16_t mv = 0;

	for( int i = 0; i < SOLAR_TILT_ADC_SAMPLES; i++ ){

		mv += adc_u16_read_mv( SOLAR_TILT_SENSOR_IO );
	}

	return mv / SOLAR_TILT_ADC_SAMPLES;
}

static uint8_t filter_index;
static int16_t tilt_filter[SOLAR_TILT_FILTER];

static uint16_t read_tilt_sensor( void ){

	// read new value
	tilt_filter[filter_index] = read_tilt_sensor_raw();
	filter_index++;

	if( filter_index >= cnt_of_array(tilt_filter) ){

		filter_index = 0;
	}

	int16_t angle = 0;

	// filter:
	for( uint8_t i = 0; i < cnt_of_array(tilt_filter); i++ ){

		angle += tilt_filter[i];
	}

	return angle / cnt_of_array(tilt_filter);
}

static void set_motor_pwm( uint8_t channel, uint16_t pwm ){

	pwm_v_write( channel, pwm );
}

static void motor_up( uint16_t pwm ){

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, pwm );
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, 0 );
}

static void motor_down( uint16_t pwm ){

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, pwm );
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, 0 );
}

static void motors_off( void ){

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, 0 );
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, 0 );
}

PT_THREAD( solar_tilt_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	pwm_v_init_channel( SOLAR_TILT_MOTOR_IO_0, 20000 );
	pwm_v_init_channel( SOLAR_TILT_MOTOR_IO_1, 20000 );


	motors_off();	

	// init tilt sensor
	for( uint8_t i = 0; i < cnt_of_array(tilt_filter); i++ ){

		tilt_filter[i] = read_tilt_sensor_raw();
	}

	solar_array_tilt_angle = read_tilt_sensor();

	
	while( 1 ){

		thread_v_set_alarm( tmr_u32_get_system_time_ms() + SOLAR_MOTOR_RATE );               
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );


        int16_t prev_tilt = solar_array_tilt_angle;

        solar_array_tilt_angle = read_tilt_sensor();

        // delta of actual tilt angle since last iteration
        int16_t tilt_delta = solar_array_tilt_angle - prev_tilt;

        // delta of actual tilt angle from target tilt angle on current iteration
        int16_t target_delta = solar_array_tilt_angle - solar_array_target_angle;

        solar_array_target_delta = target_delta;

        /*

		!!!!!!!!!!!!!!!!!!!!!!!!!!

		Test this code with gear train disconnected!

        */

        if( pause_motors ){

        	motors_off();

			bq25895_v_set_boost_mode( FALSE ); // DEBUG!
        }
        else{

        	bq25895_v_set_boost_mode( TRUE );

	        if( target_delta < -100 ){

	        	motor_up( 768 );

	        	log_v_debug_P( PSTR("%d"), solar_array_tilt_angle );
	        }
	        else if( target_delta > 100 ){

	        	motor_down( 768 );

	        	log_v_debug_P( PSTR("%d"), solar_array_tilt_angle );
	        }
	        else{

	        	motors_off();
	        }
	    }
	}

PT_END( pt );
}

