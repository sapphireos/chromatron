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

#include "bq25895.h"

#ifdef ENABLE_AUX_BATTERY

void set_register_bank_main( void );
void set_register_bank_aux( void );
// void init_charger( void );


void bq25895_aux_v_enable_charger( void ){

	set_register_bank_aux();

	bq25895_v_enable_charger();

	set_register_bank_main();
}

void bq25895_aux_v_disable_charger( void ){

	set_register_bank_aux();

	bq25895_v_disable_charger();

	set_register_bank_main();
}

void bq25895_aux_v_set_vindpm( int16_t mv ){

	set_register_bank_aux();

	bq25895_v_set_vindpm( mv );

	set_register_bank_main();
}

bool bq25895_aux_b_is_batt_fault( void ){

	bool temp = FALSE;

	set_register_bank_aux();

	temp = bq25895_u8_get_faults() != 0;

	set_register_bank_main();

	return temp;
}

uint16_t bq25895_aux_u16_read_vbus( void ){

	uint16_t temp = 0;

	set_register_bank_aux();

	temp = bq25895_u16_read_vbus();

	set_register_bank_main();	

	return temp;
}

bool bq25895_aux_b_is_charging( void ){

	bool temp = FALSE;

	set_register_bank_aux();

	temp = bq25895_b_is_charging();

	set_register_bank_main();

	return temp;
}

uint8_t bq25895_aux_u8_get_charge_status( void ){

	uint8_t temp = 0;

	set_register_bank_aux();

	temp = bq25895_u8_get_charge_status();

	set_register_bank_main();	

	return temp;
}

#endif

