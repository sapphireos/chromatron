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
#include "buttons.h"
#include "thermal.h"
#include "battery.h"
#include "patch_board.h"
#include "pixel_power.h"
#include "led_detect.h"

#include "hal_boards.h"

#include "bq25895.h"


PT_THREAD( solar_control_thread( pt_t *pt, void *state ) );


// config parameters:
static bool patch_board_installed;
static bool charger2_board_installed;
static bool enable_dc_charge = TRUE;
static bool enable_solar_charge;


static uint8_t solar_state;
#define SOLAR_MODE_STOPPED				0
#define SOLAR_MODE_DISCHARGE			1
#define SOLAR_MODE_CHARGE_DC			2
#define SOLAR_MODE_CHARGE_SOLAR			3
#define SOLAR_MODE_FULL_CHARGE			4

static catbus_string_t state_name;


static uint16_t charge_timer;
#define MAX_CHARGE_TIME		  			( 12 * 3600 )	// control loop runs at 1 hz
#define STOPPED_TIME					( 30 * 60 ) // time to remain in stopped state

static uint16_t fuel_gauge_timer;
#define FUEL_SAMPLE_TIME				( 60 ) // debug! 60 seconds is probably much more than we need

#define MIN_CHARGE_VOLTS				4000

static uint16_t filtered_batt_volts;
#define RECHARGE_THRESHOLD   ( batt_u16_get_charge_voltage() - BATT_RECHARGE_THRESHOLD )


KV_SECTION_OPT kv_meta_t solar_control_opt_kv[] = {
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&solar_state,				0,  "solar_control_state" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_READ_ONLY, 	&state_name,				0,  "solar_control_state_text" },

	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&patch_board_installed, 	0,  "solar_enable_patch_board" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&charger2_board_installed, 	0,  "solar_enable_charger2" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_dc_charge, 			0,  "solar_enable_dc_charge" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST, 	&enable_solar_charge, 		0,  "solar_enable_solar_charge" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY, 	&charge_timer,				0,  "solar_charge_timer" },
};


void solar_v_init( void ){

	thermal_v_init();

	// // debug!
	// onewire_v_init( IO_PIN_25_A1 );

	// _delay_ms( 1 ); // delay to charge bus!

	// bool device_present = onewire_b_reset();

	// log_v_debug_P( PSTR("onewire: %d"), device_present );

	// if( device_present ){

	// 	uint8_t family;
	// 	uint64_t id;
	// 	bool rom_valid = onewire_b_read_rom_id( &family, &id );

	// 	// onewire_v_write_byte( 0x33 );
		
	// 	// uint8_t id[8];

	// 	// for( uint8_t i = 0; i < 8; i++ ){

	// 	// 	id[i] = onewire_u8_read_byte();
	// 	// }

	// 	// uint8_t crc = onewire_u8_crc( id, sizeof(id) - 1 );
	// 	if( rom_valid ){

	// 		log_v_debug_P( PSTR("onewire family: %02x"), family );
	// 		log_v_debug_P( PSTR("onewire ID: %08x"), id );
	// 	}
		
	// 	// log_v_debug_P( PSTR("onewire crc: %02x"), id[7] );
	// 	// log_v_debug_P( PSTR("onewire calc crc: %02x"), crc );
	// }
	

	return;

	button_v_init();
	pixelpower_v_init();

	solar_tilt_v_init();

	kv_v_add_db_info( solar_control_opt_kv, sizeof(solar_control_opt_kv) );

	thread_t_create( solar_control_thread,
                     PSTR("solar_control"),
                     0,
                     0 );
}

bool solar_b_has_patch_board( void ){

	return patch_board_installed;
}

bool solar_b_has_charger2_board( void ){

	return charger2_board_installed;
}


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

static void enable_charge( void ){

	// check if state is set to charge
	if( !is_charging() ){

		// don't physically enable the charger if
		// we are not in a charge state

		return;
	}

	batt_v_enable_charge();
}

static void disable_charge( void ){

	batt_v_disable_charge();	
}


PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	apply_state_name();

	while(1){

		TMR_WAIT( pt, 1000 );



		fuel_gauge_timer++;

		if( fuel_gauge_timer >= FUEL_SAMPLE_TIME ){

			fuel_gauge_timer = 0;

			// disable charge mechanism (includes cutting off solar array)
			// delay 500 ms or so
			// read batt voltage
			// re-enable charge

			// note that the disable/enable charge here does not change the solar
			// control loop state.  technically we are still charging, but we turn
			// off the charger when we want to sample the current state of charge.
			// the charge current will increase the battery voltage according
			// to the impedance of the cell.

			disable_charge();

			TMR_WAIT( pt, 500 );


			// do fuel gauge here

			// filtered_batt_volts = etc

			uint16_t batt_volts = batt_u16_get_batt_volts();

			enable_charge();



			// // check if charging
			// if( ( solar_state == SOLAR_MODE_CHARGE_DC ) ||
			// 	( solar_state == SOLAR_MODE_CHARGE_SOLAR ) ){


			// }
			// else{


			// }
		}



		static uint8_t next_state;
		next_state = solar_state;


		if( solar_state == SOLAR_MODE_STOPPED ){

			charge_timer++;

			// charge timer exceeded
			if( charge_timer >= STOPPED_TIME ){

				// signal charger control to stop

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_DISCHARGE ){

			// check if DC is connected:

			if( patch_board_installed ){

				// patch board has a dedicated DC detect signal:
				if( patchboard_b_read_dc_detect() ){

					next_state = SOLAR_MODE_CHARGE_DC;
				}
				else if( patchboard_u16_read_solar_volts() >= MIN_CHARGE_VOLTS ){

					next_state = SOLAR_MODE_CHARGE_SOLAR;
				}
			}
			else if( charger2_board_installed ){

				// charger2 board is USB powered
				if( batt_u16_get_vbus_volts() >= MIN_CHARGE_VOLTS ){

					next_state = SOLAR_MODE_CHARGE_DC;
				}
			}
			else{

				// generic board
				// no dedicated DC detection.
				// we make an assumption based on configuration here.

				if( batt_u16_get_vbus_volts() >= MIN_CHARGE_VOLTS ){

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
		else if( solar_state == SOLAR_MODE_CHARGE_DC ){

			if( !enable_dc_charge ){						

				next_state = SOLAR_MODE_DISCHARGE;
			}

			// check if no longer charging:
			if( batt_b_is_charge_complete() ){

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


			// mppt control
			// tilt control



			// check if no longer charging:
			if( batt_b_is_charge_complete() ){

				next_state = SOLAR_MODE_FULL_CHARGE;
			}
			else if( !batt_b_is_charging() ){

				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else if( solar_state == SOLAR_MODE_FULL_CHARGE ){

			// ensure charge hardware is OFF
			disable_charge();

			// we do not leave full charge state until we start discharging,
			// IE battery voltage drops below a threshold
			if( filtered_batt_volts < RECHARGE_THRESHOLD ){

				// switch to discharge state
				next_state = SOLAR_MODE_DISCHARGE;
			}
		}
		else{

			ASSERT( FALSE );
		}




		if( is_charging() ){

			charge_timer++;

			// charge timer exceeded
			if( charge_timer >= MAX_CHARGE_TIME ){

				// signal charger control to stop

				next_state = SOLAR_MODE_STOPPED;
			}
		}


		if( next_state != solar_state ){

			// set up any init conditions for entry to next state
			if( next_state == SOLAR_MODE_STOPPED ){

				charge_timer = STOPPED_TIME;
			}
			else if( next_state == SOLAR_MODE_CHARGE_DC ){

				enable_charge();

				charge_timer = 0;
			}
			else if( next_state == SOLAR_MODE_CHARGE_SOLAR ){

				enable_charge();

				charge_timer = 0;

				// starting solar charge

				// if patch board is installed,
				// enable the solar panel connection
				if( patch_board_installed ){

					patchboard_v_set_solar_en( TRUE );					
				}
			}


			// check if leaving solar charge mode
			if( next_state != SOLAR_MODE_CHARGE_SOLAR ){

				// if patch board is installed:
				// disable the solar panel connection.
				if( patch_board_installed ){

					patchboard_v_set_solar_en( FALSE );					
				}

				TMR_WAIT( pt, 100 );

				disable_charge();
			}
			// check if leaving DC charge mode
			else if( next_state != SOLAR_MODE_CHARGE_DC ){

				disable_charge();	
			}



			fuel_gauge_timer = FUEL_SAMPLE_TIME - 2; // want fuel to sample shortly after any state change

			log_v_debug_P( PSTR("Changing states from %s to %s"), get_state_name( solar_state ), get_state_name( next_state ) );


			// switch states for next cycle
			solar_state = next_state;
			apply_state_name();
		}
	}

PT_END( pt );
}

