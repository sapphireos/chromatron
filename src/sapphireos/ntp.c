// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#include "system.h"

#include "ntp.h"


ntp_ts_t ntp_ts_from_ms( uint32_t ms ){

    ntp_ts_t n;

    n.seconds = ms / 1000;
    n.fraction = ( ( ( ms % 1000 ) * 1000 ) / 1024 ) << 22;

    return n;
}

uint16_t ntp_u16_get_fraction_as_ms( ntp_ts_t t ){

    uint64_t frac = t.fraction * 1000;

    frac >>= 32; // divide by 2^32, without having to cast to 64 bits

    return (uint16_t)frac;
}
