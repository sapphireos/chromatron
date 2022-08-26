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

#ifndef _TIME_NTP_H_
#define _TIME_NTP_H_

#include "target.h"

#ifdef ENABLE_TIME_SYNC

void time_v_set_ntp_master_clock( 
    ntp_ts_t source_ts, 
    uint32_t local_system_time,
    uint8_t source );
ntp_ts_t time_t_from_system_time( uint32_t end_time );
ntp_ts_t time_t_now( void );
ntp_ts_t time_t_local_now( void );


#endif

