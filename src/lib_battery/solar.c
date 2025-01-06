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

Solar control rework:

Solar charging is now relatively independent from the main charger
control.  The solar charger only connects to the batteries, it 
does not power anything and its VBUS only connects to solar panels.

The solar power system has a different architecture than the battery
only systems for shutdown: it uses a double pole switch to physically
disconnect the battery entirely (from all electronics) and also the 
solar panels to prevent powering the aux charger into a disconnected
battery pack (which is probably fine, but why worry about it).

This means the traditional software shutdown feature is not available
if the solar control is enabled.

Finally, there is dark start.  If the chargers are in cut off,
the aux charger will still try and power up off solar and charge
the battery pack.  However, the main charger will stay in cut off
until a voltage appears on its VBUS.

The system in its current form doesn't really need to be able 
to do a fully unattended dark start.  This is just an art project,
not a remote telemetry installation.


State machine design:

OFF: Solar module is not enabled.  This is a "virtual" state.
DISCHARGE: No charge sources are connected and the battery is discharging.
CHARGE_DC: The DC charger is active.  The aux charger is stopped.
CHARGE_SOLAR: The aux charger is charging (in solar mode).  The main charger is stopped.
FULL_CHARGE: Battery pack is topped off and a charge source is plugged in.
FAULT: Either charger has some kind of fault reported.  Charging is stopped.






*/

#include "sapphire.h"

#ifdef ENABLE_SOLAR
    

#include "solar.h"
#include "mppt.h"
#include "buttons.h"
#include "thermal.h"
#include "battery.h"
// #include "patch_board.h"
#include "pixel_power.h"
#include "fuel_gauge.h"
#include "energy.h"
#include "light_sensor.h"

#include "bq25895_aux.h"

#include "hal_boards.h"


/*

TODO

Add a fault state for when a power source is available but the battery
charger is reporting a fault.


*/


// config parameters:
// #ifdef ENABLE_PATCH_BOARD
// static bool patch_board_installed;
// #endif

// static bool charger2_board_installed;
// static bool enable_dc_charge = TRUE;
static bool enable_solar;
// static bool mppt_enabled;

static uint8_t solar_state;
static catbus_string_t state_name;


static uint16_t charge_timer;
// #define MAX_CHARGE_TIME		  			( 12 * 3600 )	// control loop runs at 1 hz
// #define STOPPED_TIME					( 30 * 60 ) // time to remain in stopped state
#define DISCHARGE_HOLD_TIME				( 4 ) // time to remain in discharge before allowing a switch back to charge
#define CHARGE_HOLD_TIME				( 4 )  // time to remain in charge before allowing a switch back to discharge or full
#define SOLAR_HOLD_TIME					( 20 )  // time to remain in charge before allowing a switch back to discharge or full
#define FAULT_HOLD_TIME					( 10 )  // minimum time to remain in fault state

#define RECHARGE_THRESHOLD   ( batt_u16_get_charge_voltage() - BATT_RECHARGE_THRESHOLD )


static uint16_t solar_vindpm = 5800;


// #ifdef ENABLE_PATCH_BOARD
// static bool dc_detect;
// static uint8_t dc_detect_filter[SOLAR_DC_FILTER_DEPTH];
// static uint8_t dc_detect_filter_index;

// static uint16_t solar_volts;
// static uint16_t solar_volts_filter[SOLAR_VOLTS_FILTER_DEPTH];
// static uint8_t solar_volts_filter_index;
// #endif

KV_SECTION_META kv_meta_t solar_enable_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &enable_solar,               0,  "solar_enable" },
};

// static uint32_t charge_minimum_light = SOLAR_MIN_CHARGE_LIGHT_DEFAULT;

KV_SECTION_OPT kv_meta_t solar_control_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_state,				0,  "solar_control_state" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&state_name,				0,  "solar_control_state_text" },

	// #ifdef ENABLE_PATCH_BOARD
	// { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &solar_volts,               0,  "solar_panel_volts" },
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&patch_board_installed, 	0,  "solar_enable_patch_board" },
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_READ_ONLY,  &dc_detect,                 0,  "solar_dc_detect" },
	// #endif
	
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&charger2_board_installed, 	0,  "solar_enable_charger2" },

	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_dc_charge, 			0,  "solar_enable_dc_charge" },
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_solar_charge, 		0,  "solar_enable_solar_charge" },
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    0,                          0,  "solar_enable_led_detect" },
	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &mppt_enabled,              0,  "solar_enable_mppt" },

	// { CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&charge_minimum_light,  	0,  "solar_charge_minimum_light" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&solar_vindpm,  			0,  "solar_vindpm" },

	// { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY, 	&charge_timer,				0,  "solar_charge_timer" },
};



// #ifdef ENABLE_PATCH_BOARD
// PT_THREAD( solar_sensor_thread( pt_t *pt, void *state ) );
// #endif

PT_THREAD( solar_control_thread( pt_t *pt, void *state ) );
PT_THREAD( solar_cycle_thread( pt_t *pt, void *state ) );


void solar_v_init( void ){

	thermal_v_init();

	// mppt_v_init();

	if( enable_solar ){

		kv_v_add_db_info( solar_control_opt_kv, sizeof(solar_control_opt_kv) );

		light_sensor_v_init();

		thread_t_create( solar_control_thread,
                     PSTR("solar_control"),
                     0,
                     0 );

		thread_t_create( solar_cycle_thread,
	                     PSTR("solar_cycle"),
	                     0,
	                     0 );
	}



		// #ifdef ENABLE_PATCH_BOARD
	// if( patch_board_installed && charger2_board_installed ){

	// 	log_v_error_P( PSTR("Cannot enable patch board and charger2 on the same system") );

	// 	patch_board_installed = FALSE;
	// 	charger2_board_installed = FALSE;
	// }

	// if( patch_board_installed ){

	// 	enable_solar_charge = TRUE; // solar is always enabled with patch board, that's the point of having it
		
	// 	patchboard_v_init();
	// }
	// #endif

	// if( charger2_board_installed ){
		
	// 	charger2_v_init();
	// }

	// #ifdef ENABLE_PATCH_BOARD
	// thread_t_create( solar_sensor_thread,
    //                  PSTR("solar_sensor"),
    //                  0,
    //                  0 );
	// #endif

}

// #ifdef ENABLE_PATCH_BOARD
// bool solar_b_has_patch_board( void ){

// 	return patch_board_installed;
// }
// #endif

// bool solar_b_has_charger2_board( void ){

// 	return charger2_board_installed;
// }

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

	// if( state == SOLAR_MODE_STOPPED ){

	// 	return PSTR("stopped");
	// }
	// else 
	if( state == SOLAR_MODE_DISCHARGE ){

		return PSTR("discharge");
	}
	else if( state == SOLAR_MODE_CHARGE_DC ){

		return PSTR("charge_dc");
	}
	else if( state == SOLAR_MODE_CHARGE_SOLAR ){

		return PSTR("charge_solar");
	}
	else if( state == SOLAR_MODE_LOW_SOLAR ){

		return PSTR("low_solar");
	}
	else if( state == SOLAR_MODE_FULL_CHARGE ){

		return PSTR("full_charge");
	}
	// else if( state == SOLAR_MODE_SHUTDOWN ){

	// 	return PSTR("shutdown");
	// }
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

// static bool is_recharge_threshold( void ){

// 	return batt_u16_get_batt_volts() < RECHARGE_THRESHOLD;
// }


// static void enable_charge( uint8_t target_state ){

// 	// batt_v_enable_charge();

// 	/*
	
// 	Move the BQ25895 specific stuff to a lower layer!
// 	After confirming it works!

// 	*/

// 	// if( target_state == SOLAR_MODE_CHARGE_SOLAR ){

// 	// 	if( mppt_enabled ){

// 	// 		mppt_v_enable();	
// 	// 	}
// 	// 	else{

// 	// 		// debug!
// 	// 		bq25895_v_set_vindpm( solar_vindpm );
// 	// 	}
// 	// }
// 	// // else if( target_state == SOLAR_MODE_CHARGE_DC ){

// 	// // 	bq25895_v_set_vindpm( 0 );

// 	// // 	// turn on ICO
//     // // 	// bq25895_v_set_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );   
// 	// // }
// 	// else{

// 	// 	log_v_warn_P( PSTR("This is not a valid charge state!") );
// 	// }
// }

// static void disable_charge( void ){

// 	// mppt_v_disable();
	
// 	// BQ25895: we don't actually want to turn the charger off, 
// 	// this messes with BATFET Q4 and there's not really any reason
// 	// to do it.

// 	// batt_v_disable_charge();	
// }

// static void enable_solar_vbus( void ){

	// #ifdef ENABLE_PATCH_BOARD
	// if( patch_board_installed ){

	// 	patchboard_v_set_solar_en( TRUE );					
	// }	
	// #endif
// }

// static void disable_solar_vbus( void ){

	// #ifdef ENABLE_PATCH_BOARD
	// if( patch_board_installed ){

	// 	patchboard_v_set_solar_en( FALSE );					
	// }	
	// #endif
// }

// static bool is_solar_enable_threshold( void ){
// 	// #ifdef ENABLE_PATCH_BOARD
// 	// if( ( solar_volts >= SOLAR_MIN_CHARGE_VOLTS ) &&
// 	// 	( light_sensor_u32_read() >= charge_minimum_light ) ){

// 	// 	return TRUE;
// 	// }
// 	// #else
// 	if( light_sensor_u32_read() >= charge_minimum_light ){

// 		return TRUE;
// 	}
// 	// #endif

// 	return FALSE;
// }

// #ifdef ENABLE_PATCH_BOARD
// PT_THREAD( solar_sensor_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

// 	if( !patch_board_installed ){

// 		THREAD_EXIT( pt );
// 	}

// 	while(1){

// 		TMR_WAIT( pt, SOLAR_SENSOR_POLLING_RATE );

// 		if( patch_board_installed ){

// 			dc_detect_filter[dc_detect_filter_index] = patchboard_b_read_dc_detect();
// 			dc_detect_filter_index++;

// 			if( dc_detect_filter_index >= cnt_of_array(dc_detect_filter) ){

// 				dc_detect_filter_index = 0;					
// 			}

// 			// dc detect needs the entire filter to read true

// 			dc_detect = TRUE;

// 			for( uint8_t i = 0; i < cnt_of_array(dc_detect_filter); i++ ){

// 				if( dc_detect_filter[i] == FALSE ){

// 					dc_detect = FALSE;

// 					break;
// 				}
// 			}


// 			solar_volts_filter[solar_volts_filter_index] = patchboard_u16_read_solar_volts();	
// 			solar_volts_filter_index++;

// 			if( solar_volts_filter_index >= cnt_of_array(solar_volts_filter) ){

// 				solar_volts_filter_index = 0;	
// 			}

// 			uint32_t temp = 0;

// 			for( uint8_t i = 0; i < cnt_of_array(solar_volts_filter); i++ ){

// 				temp += solar_volts_filter[i];
// 			}	

// 			solar_volts = temp / cnt_of_array(solar_volts_filter);
// 		}
// 	}

// PT_END( pt );
// }
// #endif





PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static uint8_t seconds_counter;
	seconds_counter = 10;

	// wait if battery is not connected (this could also be a charge system fault)
	THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

	solar_state = SOLAR_MODE_DISCHARGE;

	apply_state_name();

	bq25895_aux_v_enable_charger();
	batt_v_enable_charge();


	while(1){

		TMR_WAIT( pt, SOLAR_CONTROL_POLLING_RATE ); // 100 ms

		if( seconds_counter == 0 ){

			seconds_counter = 10; // reload counter
		}

		seconds_counter--;

		static uint8_t next_state;
		next_state = solar_state;

		// if( solar_state == SOLAR_MODE_SHUTDOWN ){

		// 	sys_v_initiate_shutdown( 3 );

		// 	THREAD_WAIT_WHILE( pt, !sys_b_shutdown_complete() );

		// 	log_v_debug_P( PSTR("Power off") );

		// 	pixelpower_v_system_shutdown();

		// 	_delay_ms( 50 );			

		// 	batt_v_shutdown_power();
		// 	// if on battery power, this should not return
		// 	// as the power will be cut off.
		// 	// if an external power source was plugged in during
		// 	// the shutdown, then this will return.

		// 	// we will delay here and wait
		// 	// for the reboot thread to reboot the system.
		// 	TMR_WAIT( pt, 120000 ); 

		// 	log_v_debug_P( PSTR("Shutdown failed to complete, system is still powered") );
		// }
		// // check for cut off
		// // also wait for at least some time to allow charge sources to initialize
		// // before shutting down.  this helps get into charge mode from cutoff.
		// else if( ( batt_u16_get_batt_volts() < batt_u16_get_min_discharge_voltage() ) &&
		// 		 !batt_b_is_vbus_connected() &&
		// 		 tmr_u64_get_system_time_ms() > 20000 ){
			
		// 	log_v_warn_P( PSTR("Battery at discharge cutoff") );
				
		// 	next_state = SOLAR_MODE_SHUTDOWN;
		// }
		// else if( solar_state == SOLAR_MODE_STOPPED ){

			// if( seconds_counter == 0 ){

			// 	charge_timer++;	
			// }

			// // charge timer exceeded
			// if( charge_timer >= STOPPED_TIME ){

			// 	// signal charger control to stop

			// 	next_state = SOLAR_MODE_DISCHARGE;
			// }
		// }
		// else 

		if( solar_state == SOLAR_MODE_FAULT ){

			if( charge_timer < FAULT_HOLD_TIME ){

				if( seconds_counter == 0 ){

					charge_timer++;	
				}
			}
			else if( !batt_b_is_batt_fault() &&
				     !bq25895_aux_b_is_batt_fault() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		// check if battery module is reporting a fault:
		else if( batt_b_is_batt_fault() ||
				 bq25895_aux_b_is_batt_fault() ){

			if( batt_b_is_batt_fault() ){

				log_v_warn_P( PSTR("Fault mode: main charger") );
			}

			if( bq25895_aux_b_is_batt_fault() ){

				log_v_warn_P( PSTR("Fault mode: aux charger") );
			}

			next_state = SOLAR_MODE_FAULT;
		}
		else if( solar_state == SOLAR_MODE_DISCHARGE ){

			// check charge timer, do not allow a switch to a charge mode
			// until the minimum discharge time is reached.
			// this is to prevent bouncing around charge and discharge with 
			// poor VBUS input (low light or bad adapter)
			if( charge_timer < DISCHARGE_HOLD_TIME ){

				if( seconds_counter == 0 ){

					charge_timer++;	
				}
			}
			// check if one of the chargers is connected
			else if( batt_b_is_vbus_connected() ||
					( batt_u16_get_charge_current() > 0 ) ){

				next_state = SOLAR_MODE_CHARGE_DC;	
			}
			else if( bq25895_aux_u16_get_charge_current() > 450 ){

				// starting out with a strong solar charge current

				next_state = SOLAR_MODE_CHARGE_SOLAR;	
			}
			else if( bq25895_aux_b_is_vbus_connected() ){

				// if solar VBUS is connected, but 
				// not enough charge current:

				next_state = SOLAR_MODE_LOW_SOLAR;	
			}
		}
		else if( solar_state == SOLAR_MODE_CHARGE_DC ){

			// make sure aux charger is disabled!
			bq25895_aux_v_disable_charger();

			// make sure we hit the minimum charge time before changing states
			if( charge_timer < CHARGE_HOLD_TIME ){

				if( seconds_counter == 0 ){

					charge_timer++;	
				}
			}
			// check if finished charging:
			else if( batt_b_is_charge_complete() ){

				next_state = SOLAR_MODE_FULL_CHARGE;
			}
			// or otherwise not charging:
			else if( !batt_b_is_charging() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_LOW_SOLAR ){

			// make sure main charger is disabled!
			batt_v_disable_charge();

			// check if the DC charger has connected
			if( batt_b_is_vbus_connected() ||
			   ( batt_u16_get_charge_current() > 0 ) ){

				next_state = SOLAR_MODE_CHARGE_DC;	
			}
			else if( charge_timer < SOLAR_HOLD_TIME ){

				if( seconds_counter == 0 ){

					charge_timer++;	
				}
			}
			// check if solar VBUS is not present
			else if( !bq25895_aux_b_is_vbus_connected() ){

				// switch to discharge
				next_state = SOLAR_MODE_DISCHARGE;
			}

			// check if charge current is too low
			else if( bq25895_aux_u16_get_charge_current() < 150 ){

				// we have VBUS, but almost no current
				// stay in low solar state
			}
			else{

				// charge timer has expired and we have had
				// a sustained charge for the duration
				// switch states
				next_state = SOLAR_MODE_CHARGE_SOLAR;
			}
		}
		else if( solar_state == SOLAR_MODE_CHARGE_SOLAR ){

			// make sure main charger is disabled!
			batt_v_disable_charge();

			// make sure we hit the minimum charge time before changing states
			if( charge_timer < CHARGE_HOLD_TIME ){

				if( seconds_counter == 0 ){

					charge_timer++;	
				}
			}
			// check if finished charging:
			else if( bq25895_aux_b_is_charge_complete() ){

				next_state = SOLAR_MODE_FULL_CHARGE;
			}
			// or otherwise not charging:
			else if( !bq25895_aux_b_is_charging() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_FULL_CHARGE ){


			// full charge condition:
			// battery voltage is over the recharge threshold AND
			// we have a VBUS power source on either charger


			// battery voltage below threshold:
			if( batt_u16_get_batt_volts() < RECHARGE_THRESHOLD ){

				// switch to discharge state
				next_state = SOLAR_MODE_DISCHARGE;
			}
			// neither VBUS source connected
			else if( !batt_b_is_vbus_connected() &&
					 !bq25895_aux_b_is_vbus_connected() ){

				// switch to discharge state
				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else{

			ASSERT( FALSE );
		}


		// // check if a shutdown was requested
		// if( button_b_is_shutdown_requested() ){

		// 	log_v_debug_P( PSTR("Shutdown request from button module") );

		// 	next_state = SOLAR_MODE_SHUTDOWN;
		// }
		// else if( is_charging() ){

		// 	if( seconds_counter == 0 ){

		// 		charge_timer++;	
		// 	}

		// 	// charge timer exceeded
		// 	if( charge_timer >= MAX_CHARGE_TIME ){

		// 		// signal charger control to stop
		// 		log_v_info_P( PSTR("Charge time limit reached") );

		// 		next_state = SOLAR_MODE_STOPPED;
		// 	}
		// }



		// if state is changing:

		if( next_state != solar_state ){

			charge_timer = 0;

			if( next_state == SOLAR_MODE_FAULT ){

				bq25895_aux_v_disable_charger();
				batt_v_disable_charge();
			}
			else if( next_state == SOLAR_MODE_DISCHARGE ){

				bq25895_aux_v_enable_charger();
				batt_v_enable_charge();

				gfx_v_set_system_enable( TRUE );
			}
			else if( next_state == SOLAR_MODE_CHARGE_DC ){

				gfx_v_set_system_enable( FALSE );

				bq25895_aux_v_disable_charger();
				batt_v_enable_charge();
			}
			else if( next_state == SOLAR_MODE_LOW_SOLAR ){

				gfx_v_set_system_enable( FALSE );

				batt_v_disable_charge();
				bq25895_aux_v_enable_charger();
			}
			else if( next_state == SOLAR_MODE_CHARGE_SOLAR ){

				gfx_v_set_system_enable( FALSE );

				batt_v_disable_charge();
				bq25895_aux_v_enable_charger();
			}
			else if( next_state == SOLAR_MODE_FULL_CHARGE ){

				gfx_v_set_system_enable( TRUE );
			}


			// // set up any init conditions for entry to next state
			// if( next_state == SOLAR_MODE_SHUTDOWN ){

			// 	// don't change GFX state, just leave it
			// 	// on whatever it was set to when shutting down.
			// 	// if it was off, there is no reason to turn
			// 	// graphics back on for a few seconds.
			// 	// gfx_v_set_system_enable( TRUE );
			// }
			// else if( next_state == SOLAR_MODE_STOPPED ){

			// 	charge_timer = STOPPED_TIME;

			// 	gfx_v_set_system_enable( TRUE );
			// }
			// else if( next_state == SOLAR_MODE_DISCHARGE ){

			// 	gfx_v_set_system_enable( TRUE );	
			// }
			// else if( next_state == SOLAR_MODE_FULL_CHARGE ){

			// 	gfx_v_set_system_enable( TRUE );	
			// }
			// else if( next_state == SOLAR_MODE_CHARGE_DC ){

			// 	// !!!
			// 	// on DC charge, might want to leave gfx enabled
			// 	// so FX patterns can display charge status.
			// 	// gfx_v_set_system_enable( FALSE );

			// 	if( pixelpower_b_power_control_enabled() ){
					
			// 		gfx_v_set_system_enable( FALSE );		
			// 	}
			// 	else{		
					
			// 		gfx_v_set_system_enable( TRUE );		
			// 	}

			// 	enable_charge( next_state );
			// }
			// else if( next_state == SOLAR_MODE_CHARGE_SOLAR ){

			// 	// disable graphics when on solar charging.
			// 	// can't really see them anyway!
			// 	gfx_v_set_system_enable( FALSE );

			// 	// wait until pixel power shuts off
			// 	THREAD_WAIT_WHILE( pt, pixelpower_b_pixels_enabled() );

			// 	enable_charge( next_state );

			// 	// starting solar charge

			// 	// enable the solar panel connection
			// 	enable_solar_vbus();
			// }


			// // check if leaving solar charge mode
			// if( ( solar_state == SOLAR_MODE_CHARGE_SOLAR ) &&
			// 	( next_state != SOLAR_MODE_CHARGE_SOLAR ) ){

			// 	// check if something other than full charge:
			// 	if( next_state != SOLAR_MODE_FULL_CHARGE ){

			// 		// disable the solar panel connection.
			// 		disable_solar_vbus();
			// 	}

			// 	TMR_WAIT( pt, 100 );

			// 	disable_charge();
			// }
			// // check if leaving DC charge mode
			// else if( ( solar_state == SOLAR_MODE_CHARGE_DC ) &&
			// 		 ( next_state != SOLAR_MODE_CHARGE_DC ) ){

			// 	disable_charge();	
			// }



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

	strncpy_P( cycle_name.str, get_cycle_name( solar_cycle ), sizeof(cycle_name.str) );
}



static uint8_t cycle_threshold_counter;
static uint16_t cycle_countdown;

static uint32_t day_threshold = 1000 * 1000;
static uint32_t dusk_threshold = 100 * 1000;
static uint32_t twilight_threshold = 10 * 1000;
static uint32_t dawn_threshold = 10 * 1000;

static uint16_t twilight_countdown = 300;

KV_SECTION_OPT kv_meta_t solar_cycle_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_cycle,			0,  "solar_cycle" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&cycle_name,			0,  "solar_cycle_name" },

	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&day_threshold,			0,  "solar_day_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&dusk_threshold,		0,  "solar_dusk_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&twilight_threshold,	0,  "solar_twilight_threshold" },
	{ CATBUS_TYPE_UINT32,   0, KV_FLAGS_PERSIST, 	&dawn_threshold,		0,  "solar_dawn_threshold" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&twilight_countdown,	0,  "solar_twilight_countdown" },
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

		uint8_t next_cycle = SOLAR_CYCLE_UNKNOWN;

		if( solar_cycle == SOLAR_CYCLE_DAY ){

			if( light_level < dusk_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					next_cycle = SOLAR_CYCLE_DUSK;
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

					next_cycle = SOLAR_CYCLE_TWILIGHT;
					cycle_countdown = twilight_countdown;
					cycle_threshold_counter = 0;
				}
			}
			else if( light_level > day_threshold ){

				next_cycle = SOLAR_CYCLE_DAY;
				cycle_threshold_counter = 0;
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_TWILIGHT ){

			cycle_countdown--;

			if( cycle_countdown == 0 ){

				next_cycle = SOLAR_CYCLE_NIGHT;
			}
		}
		else if( solar_cycle == SOLAR_CYCLE_NIGHT ){

			if( light_level > dawn_threshold ){

				cycle_threshold_counter++;

				if( cycle_threshold_counter >= SOLAR_CYCLE_VALIDITY_THRESH ){

					next_cycle = SOLAR_CYCLE_DAWN;
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

					next_cycle = SOLAR_CYCLE_DAY;
					cycle_threshold_counter = 0;
				}
			}
			else{

				cycle_threshold_counter = 0;
			}
		}
		else{ // unknown state

			if( light_level > day_threshold ){

				next_cycle = SOLAR_CYCLE_DAY;
			}
			else if( light_level < twilight_threshold ){

				next_cycle = SOLAR_CYCLE_NIGHT;
			}
			else if( light_level < dusk_threshold ){

				next_cycle = SOLAR_CYCLE_DUSK;
			}
		}

		if( ( next_cycle != SOLAR_CYCLE_UNKNOWN ) && ( next_cycle != solar_cycle ) ){

			log_v_debug_P( PSTR("Changing solar cycle from: %d to: %d"), solar_cycle, next_cycle );

			solar_cycle = next_cycle;
		}

		apply_cycle_name();
	}

PT_END( pt );
}


#endif
