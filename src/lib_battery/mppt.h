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

#ifndef _MPPT_H
#define _MPPT_H

#define MPPT_CHECK_INTERVAL			60000
#define MPPT_CURRENT_THRESHOLD		250

#define BQ25895_MIN_MPPT_VINDPM     4900
#define BQ25895_MAX_MPPT_VINDPM     6200
#define BQ25895_MPPT_VINDPM_STEP    100

void mppt_v_init( void );

void mppt_v_run( uint16_t charge_current );
void mppt_v_reset( void );

void mppt_v_enable( void );
void mppt_v_disable( void );

#endif
