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



#include "random.h"
#include "config.h"

static uint32_t lfsr32, lfsr31;

// 31 and 32 bit XOR's LFSR from https://www.maximintegrated.com/en/app-notes/index.mvp/id/4400
#define POLY32 0xB4BCD35C
#define POLY31 0x7A5BC2E3

#define LFSR31_SEED 0x10856523


uint32_t shift_lfsr32( uint32_t _lfsr, uint32_t mask ){

    uint8_t fb = _lfsr & 1;
    _lfsr >>= 1;
    if( fb == 1 ){

        _lfsr ^= mask;
    }

    return _lfsr;
}

uint16_t rand31_32( uint32_t *_lfsr31, uint32_t *_lfsr32 ){

    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr31 = shift_lfsr32( *_lfsr31, POLY31 );

    return ( *_lfsr32 ^ *_lfsr31 ) & 0xffff;
}

void init_random( void ){

    uint64_t device_id;
    cfg_i8_get( CFG_PARAM_DEVICE_ID, &device_id );

    // initialize random value
    rnd_v_seed( device_id );
}

void rnd_v_init( void ){

    init_random();
}

void rnd_v_seed( uint64_t seed ){

    lfsr31 = seed >> 32;
    lfsr32 = seed & 0xffffffff;

    // check if we got a 0, the LFSR will not work if initialized with all 0s
    if( lfsr31 == 0 ){

        lfsr31 = 1;
    }

    if( lfsr32 == 0 ){

        lfsr32 = 1;
    }
}

uint64_t rnd_u64_get_seed( void ){

    return ( (uint64_t)lfsr31 << 32 ) | lfsr32;
}

uint8_t rnd_u8_get_int_with_seed( uint64_t *seed ){

    return rnd_u16_get_int_with_seed( seed ) & 0xff;
}

uint16_t rnd_u16_get_int_with_seed( uint64_t *seed ){

    uint32_t _lfsr31 = *seed >> 32;
    uint32_t _lfsr32 = *seed & 0xffffffff;

    uint16_t val = rand31_32( &_lfsr31, &_lfsr32 );

    *seed = ( ( (uint64_t)_lfsr31 ) << 32 ) | _lfsr32;

    return val;
}

uint8_t rnd_u8_get_int( void ){

    uint16_t val = rand31_32( &lfsr31, &lfsr32 );

    return val & 0xff;
}

uint16_t rnd_u16_get_int( void ){

    uint16_t val = rand31_32( &lfsr31, &lfsr32 );

    return val;
}

uint32_t rnd_u32_get_int( void ){

    uint32_t val = ( (uint32_t)rnd_u16_get_int() << 16 ) | rnd_u16_get_int();

    return val;
}

uint16_t rnd_u16_get_int_hw( void ){

    #ifdef __SIM__
        return 1;
    #else

    return rnd_u16_get_int();

    #endif
}

// fill random data
void rnd_v_fill( uint8_t *data, uint16_t len ){

	while( len > 0 ){

		*data = rnd_u8_get_int();

		data++;
		len--;
	}
}

