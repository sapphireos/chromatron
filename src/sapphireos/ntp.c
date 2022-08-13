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

#include "system.h"

#include "ntp.h"
#include "datetime.h"

#include <stdlib.h>


ntp_ts_t ntp_ts_from_ms( uint32_t ms ){

    ntp_ts_t n;

    n.seconds = ms / 1000;
    n.fraction = ( ( ( ms % 1000 ) * 1000 ) / 1024 ) << 22;

    return n;
}

uint16_t ntp_u16_get_fraction_as_ms( ntp_ts_t t ){

    uint64_t frac = (uint64_t)t.fraction * 1000;

    frac >>= 32; // divide by 2^32, without having to cast to 64 bits

    return (uint16_t)frac;
}

uint64_t ntp_u64_conv_to_u64( ntp_ts_t t ){

	return ( (uint64_t)t.seconds << 32 ) + ( t.fraction );
}

ntp_ts_t ntp_ts_from_u64( uint64_t u64 ){

	ntp_ts_t ts;

	ts.seconds = u64 >> 32;
    ts.fraction = u64 & 0xffffffff;

    return ts;
}

void ntp_v_to_iso8601( char *iso8601, uint8_t len, ntp_ts_t t ){

	datetime_t datetime;
    	
    datetime_v_seconds_to_datetime( t.seconds, &datetime );

    uint16_t ms = ntp_u16_get_fraction_as_ms( t );

    datetime_v_to_iso8601( iso8601, len, &datetime );

    if( len >= ISO8601_STRING_MIN_LEN_MS ){

    	snprintf_P( &iso8601[ISO8601_STRING_MIN_LEN - 1], 5, PSTR(".%03d"), ms );
    }

    iso8601[len - 1] = 0; // null terminate
}

