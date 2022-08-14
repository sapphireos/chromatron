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
#ifndef _NTP_H
#define _NTP_H

// NTP timestamp
typedef struct{
    uint32_t seconds;
    uint32_t fraction;
} ntp_ts_t;

ntp_ts_t ntp_ts_from_ms( uint32_t ms );
uint16_t ntp_u16_get_fraction_as_ms( ntp_ts_t t );
uint64_t ntp_u64_conv_to_u64( ntp_ts_t t );
ntp_ts_t ntp_ts_from_u64( uint64_t u64 );
void ntp_v_to_iso8601( char *iso8601, uint8_t len, ntp_ts_t t );

#endif
