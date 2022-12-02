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
#include "battery.h"

#include "bq25895.h"


PT_THREAD( solar_control_thread( pt_t *pt, void *state ) );



void solar_v_init( void ){

	solar_tilt_v_init();

	thread_t_create( solar_control_thread,
                     PSTR("solar_control"),
                     0,
                     0 );
}



PT_THREAD( solar_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while(1){


		TMR_WAIT( pt, 1000 );


	}

PT_END( pt );
}

