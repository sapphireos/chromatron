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


/*


Need to improve charge validation in both solar and DC.  Automatically switch over
from bad input, with delays and logging.  Battery charging is just not simple.


*/

#include "sapphire.h"

#include "solar.h"
#include "mppt.h"
#include "buttons.h"
#include "thermal.h"
#include "battery.h"
#include "patch_board.h"
#include "charger2.h"
#include "pixel_power.h"
#include "led_detect.h"
#include "fuel_gauge.h"
#include "energy.h"
#include "light_sensor.h"

#include "hal_boards.h"


/*

TODO

Add a fault state for when a power source is available but the battery
charger is reporting a fault.


*/


// config parameters:
static bool patch_board_installed;
static bool charger2_board_installed;
static bool enable_dc_charge = TRUE;
static bool enable_solar_charge;
static bool mppt_enabled;

static uint8_t solar_state;

static catbus_string_t state_name;


static uint16_t charge_timer;
#define MAX_CHARGE_TIME		  			( 12 * 3600 )	// control loop runs at 1 hz
#define STOPPED_TIME					( 30 * 60 ) // time to remain in stopped state
#define DISCHARGE_HOLD_TIME				( 4 ) // time to remain in discharge before allowing a switch back to charge
#define CHARGE_HOLD_TIME				( 4 )  // time to remain in charge before allowing a switch back to discharge or full
#define FAULT_HOLD_TIME					( 10 )  // minimum time to remain in fault state

#define RECHARGE_THRESHOLD   ( batt_u16_get_charge_voltage() - BATT_RECHARGE_THRESHOLD )


static bool dc_detect;
static uint8_t dc_detect_filter[SOLAR_DC_FILTER_DEPTH];
static uint8_t dc_detect_filter_index;

static uint16_t solar_volts;
static uint16_t solar_volts_filter[SOLAR_VOLTS_FILTER_DEPTH];
static uint8_t solar_volts_filter_index;

static uint32_t charge_minimum_light = SOLAR_MIN_CHARGE_LIGHT_DEFAULT;

KV_SECTION_OPT kv_meta_t solar_control_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_state,				0,  "solar_control_state" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&state_name,				0,  "solar_control_state_text" },

	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&patch_board_installed, 	0,  "solar_enable_patch_board" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&charger2_board_installed, 	0,  "solar_enable_charger2" },

	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_dc_charge, 			0,  "solar_enable_dc_charge" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_solar_charge, 		0,  "solar_enable_solar_charge" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    0,                          0,  "solar_enable_led_detect" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &mppt_enabled,              0,  "solar_enable_mppt" },

	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&charge_minimum_light,  	0,  "solar_charge_minimum_light" },

	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_READ_ONLY,  &dc_detect,                 0,  "solar_dc_detect" },
	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &solar_volts,               0,  "solar_panel_volts" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY, 	&charge_timer,				0,  "solar_charge_timer" },
};




PT_THREAD( solar_sensor_thread( pt_t *pt, void *state ) );
PT_THREAD( solar_control_thread( pt_t *pt, void *state ) );
PT_THREAD( solar_cycle_thread( pt_t *pt, void *state ) );


#include "bq25895.h"


void solar_v_init( void ){

	kv_v_add_db_info( solar_control_opt_kv, sizeof(solar_control_opt_kv) );

	energy_v_init();

	fuel_v_init();

	thermal_v_init();

	light_sensor_v_init();

	if( kv_b_get_boolean( __KV__solar_enable_led_detect ) ){

		led_detect_v_init();
	}

	pixelpower_v_init();

	mppt_v_init();

	if( patch_board_installed && charger2_board_installed ){

		log_v_error_P( PSTR("Cannot enable patch board and charger2 on the same system") );

		patch_board_installed = FALSE;
		charger2_board_installed = FALSE;
	}

	if( patch_board_installed ){

		enable_solar_charge = TRUE; // solar is always enabled with patch board, that's the point of having it
		
		patchboard_v_init();
	}

	if( charger2_board_installed ){
		
		charger2_v_init();
	}


	thread_t_create( solar_control_thread,
                     PSTR("solar_control"),
                     0,
                     0 );

	thread_t_create( solar_sensor_thread,
                     PSTR("solar_sensor"),
                     0,
                     0 );

	thread_t_create( solar_cycle_thread,
                     PSTR("solar_cycle"),
                     0,
                     0 );
}

bool solar_b_has_patch_board( void ){

	return patch_board_installed;
}

bool solar_b_has_charger2_board( void ){

	return charger2_board_installed;
}

uint8_t solar_u8_get_state( void ){

	return solar_state;
}

// bool solar_b_is_dc_power( void ){

// 	return solar_state == SOLAR_MODE_CHARGE_DC;	
// }

// bool solar_b_is_solar_power( void ){

// 	return solar_state == SOLAR_MODE_CHARGE_SOLAR;
// }

static PGM_P get_state_name( uint8_t state ){

	if( state == SOLAR_MODE_STOPPED ){

		return PSTR("stopped");
	}
	else if( state == SOLAR_MODE_DISCHARGE ){

		return PSTR("discharge");
	}
	else if( state == SOLAR_MODE_CHARGE_DC ){

		return PSTR("charge_dc");
	}
	else if( state == SOLAR_MODE_CHARGE_SOLAR ){

		return PSTR("charge_solar");
	}
	else if( state == SOLAR_MODE_FULL_CHARGE ){

		return PSTR("full_charge");
	}
	else if( state == SOLAR_MODE_SHUTDOWN ){

		return PSTR("shutdown");
	}
	else if( state == SOLAR_MODE_FAULT ){

		return PSTR("fault");
	}
	else{

		return PSTR("unknown");
	}
}

static void apply_state_name( void ){

	strncpy_P( state_name.str, get_state_name( solar_state ), sizeof(state_name.str) );
}


static bool is_charging( void ){

	return ( solar_state == SOLAR_MODE_CHARGE_DC ) || 
		   ( solar_state == SOLAR_MODE_CHARGE_SOLAR );
}

bool solar_b_is_charging( void ){

	return is_charging();
}


static void enable_charge( uint8_t target_state ){

	batt_v_enable_charge();

	/*
	
	Move the BQ25895 specific stuff to a lower layer!
	After confirming it works!

	*/

	if( target_state == SOLAR_MODE_CHARGE_SOLAR ){

		if( mppt_enabled ){

			mppt_v_enable();	
		}
		else{

			// debug!
			bq25895_v_set_vindpm( 5800 );
		}
	}
	else if( target_state == SOLAR_MODE_CHARGE_DC ){

		bq25895_v_set_vindpm( 0 );

		// turn on ICO
    	bq25895_v_set_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );   
	}
	else{

		log_v_warn_P( PSTR("This is not a valid charge state!") );
	}
}

static void disable_charge( void ){

	mppt_v_disable();
	
	// BQ25895: we don't actually want to turn the charger off, 
	// this messes with BATFET Q4 and there's not really any reason
	// to do it.

	// batt_v_disable_charge();	
}

static void enable_solar_vbus( void ){

	if( patch_board_installed ){

		patchboard_v_set_solar_en( TRUE );					
	}	
}

static void disable_solar_vbus( void ){

	if( patch_board_installed ){

		patchboard_v_set_solar_en( FALSE );					
	}	
}

static bool is_solar_enable_threshold( void ){

	if( ( solar_volts >= SOLAR_MIN_CHARGE_VOLTS ) &&
		( light_sensor_u32_read() >= charge_minimum_light ) ){

		return TRUE;
	}

	return FALSE;
}


PT_THREAD( solar_sensor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	if( !patch_board_installed ){

		THREAD_EXIT( pt );
	}

	while(1){

		TMR_WAIT( pt, SOLAR_SENSOR_POLLING_RATE );

		if( patch_board_installed ){

			dc_detect_filter[dc_detect_filter_index] = patchboard_b_read_dc_detect();
			dc_detect_filter_index++;

			if( dc_detect_filter_index >= cnt_of_array(dc_detect_filter) ){

				dc_detect_filter_index = 0;					
			}

			// dc detect needs the entire filter to read true

			dc_detect = TRUE;

			for( uint8_t i = 0; i < cnt_of_array(dc_detect_filter); i++ ){

				if( dc_detect_filter[i] == FALSE ){

					dc_detect = FALSE;

					break;
				}
			}


			solar_volts_filter[solar_volts_filter_index] = patchboard_u16_read_solar_volts();	
			solar_volts_filter_index++;

			if( solar_volts_filter_index >= cnt_of_array(solar_volts_filter) ){

				solar_volts_filter_index = 0;	
			}

			uint32_t temp = 0;

			for( uint8_t i = 0; i < cnt_of_array(solar_volts_filter); i++ ){

				temp += solar_volts_filter[i];
			}	

			solar_volts = temp / cnt_of_array(solar_volts_filter);
		}
	}

PT_END( pt );
}


PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	// wait if battery is not connected (this could also be a charge system fault)
	THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

	// check if above solar threshold
	if( is_solar_enable_threshold() ){

		// if so, start in discharge state, so it will then switch to solar charge
		// and enable the panel connection.
		// this is done before the full charge check, so that if it is fully charged,
		// we will still be able to power off the panel instead of the battery.
		solar_state = SOLAR_MODE_DISCHARGE;		
	}
	// check if fully charged:
	else if( batt_u16_get_batt_volts() >= batt_u16_get_charge_voltage() ){

		solar_state = SOLAR_MODE_FULL_CHARGE;
	}
	else{

		solar_state = SOLAR_MODE_DISCHARGE;
	}


	apply_state_name();


	while(1){

		TMR_WAIT( pt, SOLAR_CONTROL_POLLING_RATE );

		static uint8_t next_state;
		next_state = solar_state;

		if( solar_state == SOLAR_MODE_SHUTDOWN ){

			sys_v_initiate_shutdown( 5 );

			THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

			batt_v_shutdown_power();
			// if on battery power, this should not return
			// as the power will be cut off.
			// if an external power source was plugged in during
			// the shutdown, then this will return.

			// we will delay here and wait
			// for the reboot thread to reboot the system.
			TMR_WAIT( pt, 120000 ); 

			log_v_debug_P( PSTR("Shutdown failed to complete, system is still powered") );
		}
		// check for cut off
		// also wait for at least some time to allow charge sources to initialize
		// before shutting down.  this helps get into charge mode from cutoff.
		else if( ( batt_u16_get_batt_volts() < batt_u16_get_min_discharge_voltage() ) &&
				 !batt_b_is_vbus_connected() &&
				 tmr_u64_get_system_time_ms() > 20000 ){
			
			log_v_warn_P( PSTR("Battery at discharge cutoff") );
				
			next_state = SOLAR_MODE_SHUTDOWN;
		}
		else if( solar_state == SOLAR_MODE_STOPPED ){

			charge_timer++;

			// charge timer exceeded
			if( charge_timer >= STOPPED_TIME ){

				// signal charger control to stop

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_FAULT ){

			if( charge_timer < FAULT_HOLD_TIME ){

				charge_timer++;
			}
			else if( !batt_b_is_batt_fault() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_DISCHARGE ){

			// check charge timer, do not allow a switch to a charge mode
			// until the minimum discharge time is reached.
			// this is to prevent bouncing around charge and discharge with 
			// poor VBUS input (low light or bad adapter)
			if( charge_timer < DISCHARGE_HOLD_TIME ){

				charge_timer++;
			}

			// check for fault?
			// go to fault state?
			else if( batt_b_is_batt_fault() ){

				next_state = SOLAR_MODE_FAULT;
			}
			else{

				// minimum discharge time reached

				if( patch_board_installed ){

					// patch board has a dedicated DC detect signal:
					// also validate that VBUS sees it
					// and no charger faults
					if( dc_detect && batt_b_is_vbus_connected() ){

						next_state = SOLAR_MODE_CHARGE_DC;
					}
					// check solar enable threshold AND
					// that there are no charger faults reported.
					else if( is_solar_enable_threshold() ){

						log_v_debug_P( PSTR("entering solar charge: %u mV %u lux"), solar_volts, light_sensor_u32_read() );

						next_state = SOLAR_MODE_CHARGE_SOLAR;
					}
				}
				else if( charger2_board_installed ){

					// charger2 board is USB powered
					if( batt_b_is_vbus_connected() ){

						next_state = SOLAR_MODE_CHARGE_DC;
					}
				}
				else{

					// generic board
					// no dedicated DC detection.
					// we make an assumption based on configuration here.

					if( batt_b_is_vbus_connected() ){

						if( enable_solar_charge ){

							// if solar is enabled, assume charging on solar power
							next_state = SOLAR_MODE_CHARGE_SOLAR;
						}
						else if( enable_dc_charge ){

							next_state = SOLAR_MODE_CHARGE_DC;	
						}
					}	
				}
			}
		}
		else if( solar_state == SOLAR_MODE_CHARGE_DC ){

			if( !enable_dc_charge ){					

				log_v_error_P( PSTR("DC charge is not enabled!") );

				next_state = SOLAR_MODE_DISCHARGE;
			}
			else if( charge_timer < CHARGE_HOLD_TIME ){

				charge_timer++;
			}
			else if( batt_b_is_batt_fault() ){

				next_state = SOLAR_MODE_FAULT;
			}
			// check if no longer charging:
			else if( batt_b_is_charge_complete() ){

				next_state = SOLAR_MODE_FULL_CHARGE;
			}
			else if( !batt_b_is_charging() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_CHARGE_SOLAR ){

			if( !enable_solar_charge ){						

				next_state = SOLAR_MODE_DISCHARGE;
			}
			// check if no longer charging:
			else if( batt_b_is_batt_fault() ){

				next_state = SOLAR_MODE_FAULT;
			}
			else if( batt_b_is_charge_complete() ){

				next_state = SOLAR_MODE_FULL_CHARGE;
			}
			else if( !batt_b_is_charging() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_FULL_CHARGE ){

			// we do not leave full charge state until we start discharging,
			// IE battery voltage drops below a threshold
			if( batt_u16_get_batt_volts() < RECHARGE_THRESHOLD ){

				// switch to discharge state
				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else{

			ASSERT( FALSE );
		}


		// check if a shutdown was requested
		if( button_b_is_shutdown_requested() ){

			log_v_debug_P( PSTR("Shutdown request from button module") );

			next_state = SOLAR_MODE_SHUTDOWN;
		}
		else if( is_charging() ){

			charge_timer++;

			// charge timer exceeded
			if( charge_timer >= MAX_CHARGE_TIME ){

				// signal charger control to stop
				log_v_info_P( PSTR("Charge time limit reached") );

				next_state = SOLAR_MODE_STOPPED;
			}
		}



		// if state is changing:

		if( next_state != solar_state ){

			charge_timer = 0;

			// set up any init conditions for entry to next state
			if( next_state == SOLAR_MODE_SHUTDOWN ){

				// don't change GFX state, just leave it
				// on whatever it was set to when shutting down.
				// if it was off, there is no reason to turn
				// graphics back on for a few seconds.
				// gfx_v_set_system_enable( TRUE );
			}
			else if( next_state == SOLAR_MODE_STOPPED ){

				charge_timer = STOPPED_TIME;

				gfx_v_set_system_enable( TRUE );
			}
			else if( next_state == SOLAR_MODE_DISCHARGE ){

				gfx_v_set_system_enable( TRUE );	
			}
			else if( next_state == SOLAR_MODE_FULL_CHARGE ){

				gfx_v_set_system_enable( TRUE );	
			}
			else if( next_state == SOLAR_MODE_CHARGE_DC ){

				// !!!
				// on DC charge, might want to leave gfx enabled
				// so FX patterns can display charge status.
				gfx_v_set_system_enable( FALSE );

				enable_charge( next_state );
			}
			else if( next_state == SOLAR_MODE_CHARGE_SOLAR ){

				// disable graphics when on solar charging.
				// can't really see them anyway!
				gfx_v_set_system_enable( FALSE );

				// wait until pixel power shuts off
				THREAD_WAIT_WHILE( pt, pixelpower_b_pixels_enabled() );

				enable_charge( next_state );

				// starting solar charge

				// enable the solar panel connection
				enable_solar_vbus();
			}


			// check if leaving solar charge mode
			if( ( solar_state == SOLAR_MODE_CHARGE_SOLAR ) &&
				( next_state != SOLAR_MODE_CHARGE_SOLAR ) ){

				// check if something other than full charge:
				if( next_state != SOLAR_MODE_FULL_CHARGE ){

					// disable the solar panel connection.
					disable_solar_vbus();
				}

				TMR_WAIT( pt, 100 );

				disable_charge();
			}
			// check if leaving DC charge mode
			else if( ( solar_state == SOLAR_MODE_CHARGE_DC ) &&
					 ( next_state != SOLAR_MODE_CHARGE_DC ) ){

				disable_charge();	
			}

			log_v_debug_P( PSTR("Changing states from %s to %s"), get_state_name( solar_state ), get_state_name( next_state ) );


			// switch states for next cycle
			solar_state = next_state;
			apply_state_name();
		}
	}

PT_END( pt );
}




static uint8_t solar_cycle;
static catbus_string_t cycle_name;


static PGM_P get_cycle_name( uint8_t state ){

	if( state == SOLAR_CYCLE_UNKNOWN ){

		return PSTR("unknown");
	}
	else if( state == SOLAR_CYCLE_DAY ){

		return PSTR("day");
	}
	else if( state == SOLAR_CYCLE_DUSK ){

		return PSTR("dusk");
	}
	else if( state == SOLAR_CYCLE_TWILIGHT ){

		return PSTR("twilight");
	}
	else if( state == SOLAR_CYCLE_NIGHT ){

		return PSTR("night");
	}
	else if( state == SOLAR_CYCLE_DAWN ){

		return PSTR("dawn");
	}
	else{

		return PSTR("invalid");
	}
}

static void apply_cycle_name( void ){

	strncpy_P( cycle_name.str, get_cycle_name( solar_state ), sizeof(cycle_name.str) );
}



static uint8_t cycle_threshold_counter;
static uint16_t cycle_countdown;

static uint32_t day_threshold = 1000 * 1000;
static uint32_t dusk_threshold = 100 * 1000;
static uint32_t twilight_threshold = 10 * 1000;
static uint32_t dawn_threshold = 10 * 1000;

KV_SECTION_OPT kv_meta_t solar_cycle_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_cycle,			0,  "solar_cycle" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&cycle_name,			0,  "solar_cycle_name" },

	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&day_threshold,			0,  "solar_day_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&dusk_threshold,		0,  "solar_dusk_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&twilight_threshold,	0,  "solar_twilight_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&dawn_threshold,		0,  "solar_dawn_threshold" },
};

/*

State ordering:

day
dusk
twilight
night
dawn

repeat


*/

PT_THREAD( solar_cycle_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	kv_v_add_db_info( solar_cycle_opt_kv, sizeof(solar_cycle_opt_kv) );



	solar_cycle = SOLAR_CYCLE_UNKNOWN;
	// candidate_next_cycle = SOLAR_CYCLE_UNKNOWN;

	TMR_WAIT( pt, 10000 );

	while(1){

		TMR_WAIT( pt, SOLAR_CYCLE_POLLING_RATE );

		uint32_t light_level = light_sensor_u32_read();

		if( solar_cycle == SOLAR_CYCLE_DAY ){

			if( light_level < dusk_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					solar_cycle = SOLAR_CYCLE_DUSK;
					cycle_threshold_counter = 0;
				}
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_DUSK ){

			if( light_level < twilight_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					solar_cycle = SOLAR_CYCLE_TWILIGHT;
					cycle_countdown = SOLAR_CYCLE_TWILIGHT_TIME;
					cycle_threshold_counter = 0;
				}
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_TWILIGHT ){

			cycle_countdown--;

			if( cycle_countdown == 0 ){

				solar_cycle = SOLAR_CYCLE_NIGHT;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_NIGHT ){

			if( light_level > dawn_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					solar_cycle = SOLAR_CYCLE_DAWN;
					cycle_threshold_counter = 0;
				}
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_DAWN ){

			if( light_level > day_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					solar_cycle = SOLAR_CYCLE_DAY;
					cycle_threshold_counter = 0;
				}
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else{ // unknown state

			if( light_level > day_threshold ){

				solar_cycle = SOLAR_CYCLE_DAY;
			}
			else if( light_level < twilight_threshold ){

				solar_cycle = SOLAR_CYCLE_NIGHT;
			}
			else if( light_level < dusk_threshold ){

				solar_cycle = SOLAR_CYCLE_DUSK;
			}
		}

		apply_cycle_name();
	}

PT_END( pt );
}


