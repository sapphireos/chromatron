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

#include "hal_boards.h"

#include "pwm.h"
#include "keyvalue.h"

#include "solar_tilt.h"
#include "patch_board.h"
#include "battery.h"

#include "bq25895.h"


static uint16_t solar_tilt_motor_pwm = 768;

static uint16_t solar_array_tilt_sensor;
static uint16_t solar_array_target_sensor;

static uint8_t solar_array_tilt_angle;
static uint8_t solar_array_target_angle;

static uint32_t solar_tilt_total_travel;

static bool unlock_panel;
static bool enable_manual_tilt;
static bool pause_motors = TRUE; // DEBUG default to paused!

static uint8_t motor_state;
#define MOTOR_STATE_IDLE 	0
#define MOTOR_STATE_UP		1
#define MOTOR_STATE_DOWN	2
#define MOTOR_STATE_LOCK	3


void set_tilt_target( uint8_t angle );

int8_t _tilt_target_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

    }
    else if( op == KV_OP_SET ){

    	set_tilt_target( solar_array_target_angle );
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

KV_SECTION_META kv_meta_t solar_tilt_info_kv[] = {
    
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_PERSIST,  0,             0,  "solar_enable_tilt" },
};

KV_SECTION_OPT kv_meta_t solar_tilt_opt_kv[] = {
    { CATBUS_TYPE_UINT16,    0, KV_FLAGS_PERSIST, 	&solar_tilt_motor_pwm,		0,  "solar_tilt_motor_pwm" },

    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &solar_array_tilt_sensor,   0,  "solar_tilt_sensor" },
    { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,	&solar_array_target_sensor, 0,  "solar_tilt_target_sensor" },
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  &solar_array_tilt_angle,    0,  "solar_tilt_angle" },

    { CATBUS_TYPE_UINT8,    0, 0, 	 				&solar_array_target_angle,  _tilt_target_kv_handler,  "solar_tilt_target_angle" },

    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_PERSIST,    &solar_tilt_total_travel,   0,  "solar_tilt_total_travel" },

    { CATBUS_TYPE_BOOL,     0, 0,  					&pause_motors,              0,  "solar_tilt_pause_motor" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &enable_manual_tilt,        0,  "solar_tilt_manual" },
    { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&unlock_panel,              0,  "solar_tilt_unlock_panel" },
    { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  &motor_state,               0,  "solar_tilt_motor_state" },
};


PT_THREAD( solar_tilt_thread( pt_t *pt, void *state ) );

static void motors_off( void );

void solar_tilt_v_init( void ){

	if( kv_b_get_boolean( __KV__solar_enable_tilt ) ){

		pwm_v_init_channel( SOLAR_TILT_MOTOR_IO_0, 20000 );
		pwm_v_init_channel( SOLAR_TILT_MOTOR_IO_1, 20000 );


		io_v_set_mode( ELITE_FAN_IO, IO_MODE_OUTPUT );    
    	io_v_digital_write( ELITE_FAN_IO, 0 );

		motors_off();

		kv_v_add_db_info( solar_tilt_opt_kv, sizeof(solar_tilt_opt_kv) );

		thread_t_create( solar_tilt_thread,
                     PSTR("solar_tilt"),
                     0,
                     0 );
	}
}


static uint8_t convert_tilt_mv_to_angle( uint16_t mv ){

	return util_u16_linear_interp( mv, SOLAR_TILT_SENSOR_MIN, SOLAR_ANGLE_POS_MIN, SOLAR_TILT_SENSOR_MAX, SOLAR_ANGLE_POS_MAX );
}

static uint16_t convert_angle_to_tilt_mv( uint8_t angle ){

	return util_u16_linear_interp( angle, SOLAR_ANGLE_POS_MIN, SOLAR_TILT_SENSOR_MIN, SOLAR_ANGLE_POS_MAX, SOLAR_TILT_SENSOR_MAX );
}



uint8_t solar_tilt_u8_get_tilt_angle( void ){

	return solar_array_tilt_angle;
}

uint8_t solar_tilt_u8_get_target_angle( void ){

	return solar_array_target_angle;
}

void set_tilt_target( uint8_t angle ){

	if( angle > SOLAR_ANGLE_POS_MAX ){

		angle = SOLAR_ANGLE_POS_MAX;
	}
	// else if( angle < SOLAR_ANGLE_POS_MIN ){

	// 	angle = SOLAR_ANGLE_POS_MIN;
	// }

	solar_array_target_angle = angle;

	solar_array_target_sensor = convert_angle_to_tilt_mv( angle );
}

void solar_tilt_v_set_tilt_angle( uint8_t angle ){

	if( enable_manual_tilt ){

		// automatic tilt control bypassed
		return;
	}
	else if( unlock_panel ){

		// panel unlocked, disable automatic control

		return;
	}

	set_tilt_target( angle );
}

static uint16_t read_tilt_sensor_raw( void ){

	uint16_t mv = 0;

	for( int i = 0; i < SOLAR_TILT_ADC_SAMPLES; i++ ){

		// mv += adc_u16_read_mv( SOLAR_TILT_SENSOR_IO );
		mv += patchboard_u16_read_tilt_volts();

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

	io_v_digital_write( ELITE_FAN_IO, 1 );

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, pwm );
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, 0 );
}

static void motor_down( uint16_t pwm ){

	io_v_digital_write( ELITE_FAN_IO, 1 );

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, pwm );	
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, 0 );
}

static void motors_off( void ){

	io_v_digital_write( ELITE_FAN_IO, 0 );

	set_motor_pwm( SOLAR_TILT_MOTOR_IO_0, 0 );
	set_motor_pwm( SOLAR_TILT_MOTOR_IO_1, 0 );
}


static int16_t motor_timeout;

static uint8_t motor_1sec_count;


static bool is_motor_running( void ){

	return ( motor_state == MOTOR_STATE_UP ) || ( motor_state == MOTOR_STATE_DOWN );
}


/*

Mechanical calibration procedure:

Move panel to full open with gears disengaged.

Set tilt sensor target to 3200 mV and wait for drive train to reach position.

Engage the tilt gear.

Set tilt sensor target to 3000 mV.  Gears should engage and lock in.  Position should be approx 60 degrees.

Set tilt sensor target to 1650 mV.  This should be a 30 degreee tilt angle.

Set tilt sensor target to 300 mV.  This should lower the panel to 0 degrees and stop.



Tilt max, 60 degrees at 3000 mV sensor.
Tilt min, 0 degrees, at 300 mV sensor.





*/

PT_THREAD( solar_tilt_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	motors_off();	

	// init tilt sensor
	for( uint8_t i = 0; i < cnt_of_array(tilt_filter); i++ ){

		tilt_filter[i] = read_tilt_sensor_raw();
	}

	solar_array_tilt_sensor = read_tilt_sensor();

	solar_array_target_sensor = solar_array_tilt_sensor; // init so we don't immediately move

	solar_array_tilt_angle = convert_tilt_mv_to_angle( solar_array_tilt_sensor );
	solar_array_target_angle = convert_tilt_mv_to_angle( solar_array_target_sensor );

	
	thread_v_set_alarm( tmr_u32_get_system_time_ms() + SOLAR_MOTOR_RATE );

	
	while( 1 ){

		thread_v_set_alarm( thread_u32_get_alarm() + SOLAR_MOTOR_RATE );               
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        // record previous angle
        int16_t prev_tilt_sensor = solar_array_tilt_sensor;

        solar_array_tilt_sensor = read_tilt_sensor();

        solar_array_tilt_angle = convert_tilt_mv_to_angle( solar_array_tilt_sensor );

        if( sys_b_is_shutting_down() ){

			motors_off();

			// record total travel
			kv_i8_persist( __KV__solar_tilt_total_travel );

			THREAD_EXIT( pt );        	
        }


        if( pause_motors ){

        	motors_off();

			// bq25895_v_set_boost_mode( FALSE ); // DEBUG!

			motor_state = MOTOR_STATE_IDLE;

			continue;
        }

        if( unlock_panel ){

        	solar_array_target_sensor = SOLAR_TILT_SENSOR_OPEN;
        }
        else if( solar_array_tilt_sensor > ( SOLAR_TILT_SENSOR_OPEN - 100 ) ){

        	solar_array_target_sensor = SOLAR_TILT_SENSOR_MAX;
        }


        bq25895_v_set_boost_mode( TRUE ); // DEBUG!

        motor_1sec_count++;

        if( motor_1sec_count >= ( 1000 / SOLAR_MOTOR_RATE) ){

        	motor_1sec_count = 0;


        	/*

			***********************************************

			ONE SECOND TASKS

			***********************************************

        	*/


	        // check if motor is running
	        if( is_motor_running() ){

	        	// delta of actual tilt angle since last iteration
	        	int16_t tilt_delta = (int16_t)solar_array_tilt_sensor - prev_tilt_sensor;

	        	// if array is in motion, reset timeout
	        	if( abs16( tilt_delta ) > 0 ){

	        		motor_timeout = SOLAR_MOTOR_MOVE_TIMEOUT;


	        		// record delta to total amount of travel recorded
	        		// solar_tilt_total_travel += abs16( tilt_delta );
	        	}
	        	else{

		        	// process timeout,
		        	// if we time out, turn off the motors and reset state
		        	motor_timeout -= SOLAR_MOTOR_RATE;

		        	if( motor_timeout < 0 ){

		        		/*
						!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

						TIMEOUT

						!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		

						Movement timeout: a move was started, but the sensor failed to register
						any movement within the timeout.  This is a possible jam in the
						mechanical system.

						Immediately disengage the motors!
						
						Change state to LOCK, which locks the motors off until the timeout
						expires.

		        		*/

		        		motors_off();

		        		motor_state = MOTOR_STATE_LOCK;
		        		motor_timeout = SOLAR_MOTOR_LOCK_TIMEOUT;

		        		continue;
		        	}
		        }
		    }

        }


        // delta of actual tilt angle from target tilt angle on current iteration
        int16_t target_delta = (int16_t)solar_array_target_sensor - (int16_t)solar_array_tilt_sensor;

        if( motor_state == MOTOR_STATE_IDLE ){

        	// check if target angle is far enough away from current angle:
        	if( abs16( target_delta ) >= SOLAR_TILT_SENSOR_MOVE_THRESHOLD ){

        		// initiate a movement

        		// select direction to move:
        		if( target_delta > 0 ){

					motor_state = MOTOR_STATE_UP;        			
        		}
        		else if( target_delta < 0 ){

        			motor_state = MOTOR_STATE_DOWN;
        		}

        		// if we've decided to move, reset the movement timeout
        		if( motor_state != MOTOR_STATE_IDLE ){

        			motor_timeout = SOLAR_MOTOR_MOVE_TIMEOUT;
        		}
        	} 
        }
        // if traveling up:
        else if( motor_state == MOTOR_STATE_UP ){


        	// check if target position has been reached:
        	if( ( abs16( target_delta ) < SOLAR_TILT_SENSOR_MOVE_THRESHOLD ) ||
         	    ( solar_array_tilt_sensor > solar_array_target_sensor ) ){

        		// stop movement and reset state
        		motors_off();

        		motor_state = MOTOR_STATE_IDLE;
        	}
        	else{

        		// movement in progress, engage motors

        		motor_up( solar_tilt_motor_pwm );
        	}
        }
        // traveling down:
        else if( motor_state == MOTOR_STATE_DOWN ){
			
			if( ( abs16( target_delta ) < SOLAR_TILT_SENSOR_MOVE_THRESHOLD ) ||
        		( solar_array_tilt_sensor < solar_array_target_sensor ) ){

        		motors_off();

        		motor_state = MOTOR_STATE_IDLE;
        	}
        	else{

        		motor_down( solar_tilt_motor_pwm );
        	}
        }
        else if( motor_state == MOTOR_STATE_LOCK ){

			motors_off();        		

			motor_timeout -= SOLAR_MOTOR_RATE;

			if( motor_timeout < 0 ){

				motor_state = MOTOR_STATE_IDLE;
			}
        }
        else{

        	// invalid state, make sure motors are off before asserting!

        	motors_off();

        	ASSERT( FALSE );
        }






        // OLD!


        /*

		!!!!!!!!!!!!!!!!!!!!!!!!!!

		Test this code with gear train disconnected!

        */

        // if( pause_motors ){

        // 	motors_off();

		// 	bq25895_v_set_boost_mode( FALSE ); // DEBUG!
        // }
        // else{

        // 	bq25895_v_set_boost_mode( TRUE );

	    //     if( target_delta < -50 ){

	    //     	motor_up( 1023 );

	    //     	log_v_debug_P( PSTR("%d"), solar_array_tilt_angle );
	    //     }
	    //     else if( target_delta > 50 ){

	    //     	motor_down( 1023 );

	    //     	log_v_debug_P( PSTR("%d"), solar_array_tilt_angle );
	    //     }
	    //     else{

	    //     	motors_off();
	    //     }
	    // }
	}

PT_END( pt );
}

