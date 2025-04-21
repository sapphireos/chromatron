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
#include "bq25895.h"
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

static bool enable_solar;
// static bool mppt_enabled;

static uint8_t solar_state;
static catbus_string_t state_name;


static uint16_t charge_timer;
// #define MAX_CHARGE_TIME		  			( 12 * 3600 )	// control loop runs at 1 hz
#define DISCHARGE_HOLD_TIME				( 4 ) // time to remain in discharge before allowing a switch back to charge
#define CHARGE_HOLD_TIME				( 4 )  // time to remain in charge before allowing a switch back to discharge or full
#define SOLAR_HOLD_TIME					( 4 )  // time to remain in charge before allowing a switch back to discharge or full
#define FAULT_HOLD_TIME					( 10 )  // minimum time to remain in fault state

#define RECHARGE_THRESHOLD   ( batt_u16_get_charge_voltage() - BATT_RECHARGE_THRESHOLD )


static uint16_t solar_vindpm = 5800;



KV_SECTION_META kv_meta_t solar_enable_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &enable_solar,               0,  "solar_enable" },
};


KV_SECTION_OPT kv_meta_t solar_control_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_state,				0,  "solar_control_state" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&state_name,				0,  "solar_control_state_text" },

	// { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &mppt_enabled,              0,  "solar_enable_mppt" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&solar_vindpm,  			0,  "solar_vindpm" },
};



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


#define NEXT_STATE_VALID 10

PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static uint8_t seconds_counter;
	seconds_counter = 1;

	static uint8_t log_timer;
	log_timer = 0;

	static uint8_t candidate_next_state;
	candidate_next_state = solar_state;

	static uint8_t next_state_validation_counter;
	next_state_validation_counter = 0;

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

		// process fault handling:

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

				log_v_warn_P( PSTR("Fault mode: main charger 0x%02x"), bq25895_u8_get_faults() );

				next_state = SOLAR_MODE_FAULT;
			}

			if( bq25895_aux_b_is_batt_fault() ){

				uint8_t faults = bq25895_aux_u8_get_faults();

				// check if input fault - this is normal behavior in low light on the aux/solar
				// charger.
				if( ( faults & ~BQ25895_MASK_CHRG_FAULT ) == 0 ){

					// input fault - this is ok
				}
				else{

					// some other fault, not ok

					log_v_warn_P( PSTR("Fault mode: aux charger: 0x%02x"), faults );
					next_state = SOLAR_MODE_FAULT;
				}
			}
		}

		// process state machine

		if( ( solar_state == SOLAR_MODE_FAULT ) || ( next_state == SOLAR_MODE_FAULT ) ){

			// skip state machine if we are going in to fault mode.

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
				log_v_debug_P( PSTR("SOLAR_MODE_CHARGE_DC: %d %d"), batt_b_is_vbus_connected(), batt_u16_get_charge_current() );
			}
			else if( bq25895_aux_u16_get_charge_current() > 450 ){

				// starting out with a strong solar charge current

				next_state = SOLAR_MODE_CHARGE_SOLAR;	
			}
			else if( bq25895_aux_b_is_vbus_connected() ){

				// if solar VBUS is connected, but 
				// not enough charge current:

				next_state = SOLAR_MODE_LOW_SOLAR;	

				log_v_debug_P( PSTR("SOLAR_MODE_LOW_SOLAR: %d %d"), bq25895_aux_b_is_vbus_connected(), bq25895_aux_u16_read_vbus() );
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

			bq25895_aux_v_set_vindpm( solar_vindpm );

			if( log_timer == 0 ){

				log_timer = 30;
			
				log_v_debug_P( PSTR("low solar: %d mA %d mV"), bq25895_aux_u16_get_charge_current(), batt_u16_get_batt_volts() );
			}

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
			else if( bq25895_aux_u16_get_charge_current() < 200 ){

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

			bq25895_aux_v_set_vindpm( solar_vindpm );

			if( log_timer == 0 ){
				
				log_timer = 30;

				log_v_debug_P( PSTR("solar: %d mA %d mV"), bq25895_aux_u16_get_charge_current(), batt_u16_get_batt_volts() );
			}

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

		if( seconds_counter == 0 ){

			if( log_timer > 0 ){

				log_timer--;
			}	
		}

		// if state is changing:

		if( next_state != solar_state ){

			if( next_state_validation_counter == 0 ){

				candidate_next_state = next_state;
			}

			if( next_state == candidate_next_state ){

				next_state_validation_counter++;	
			}
			else{

				next_state_validation_counter = 0;
			}

			if( next_state_validation_counter >= NEXT_STATE_VALID ){

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

					gfx_v_set_system_enable( TRUE );

					batt_v_disable_charge();				

					bq25895_aux_v_enable_charger();
					bq25895_aux_v_set_vindpm( solar_vindpm );
				}
				else if( next_state == SOLAR_MODE_CHARGE_SOLAR ){

					gfx_v_set_system_enable( FALSE );

					batt_v_disable_charge();

					bq25895_aux_v_enable_charger();
					bq25895_aux_v_set_vindpm( solar_vindpm );
				}
				else if( next_state == SOLAR_MODE_FULL_CHARGE ){

					gfx_v_set_system_enable( TRUE );
				}

				log_v_debug_P( PSTR("Changing states from %s to %s"), get_state_name( solar_state ), get_state_name( next_state ) );

				// switch states for next cycle
				solar_state = next_state;
				apply_state_name();

			}
		}
		else{

			next_state_validation_counter = 0;
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
