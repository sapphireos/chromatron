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


#include "hash.h"
#include <string.h>

#define FNV_32BIT_OFFSET_BASIS    (uint32_t)0x811c9dc5
#define FNV_32BIT_PRIME           (uint32_t)16777619


static uint32_t fnv1a( uint32_t base, uint8_t *data, uint16_t len ){

    uint32_t hash = base;

    while( len > 0 ){

        hash ^= *data;
        hash *= FNV_32BIT_PRIME;

        len--;
        data++;
    }

    return hash;
}

uint32_t hash_u32_data( uint8_t *data, uint16_t len ){
    
    if( len == 0 ){

        return 0;
    }

    return fnv1a( FNV_32BIT_OFFSET_BASIS, data, len );
}

uint32_t hash_u32_string( char *src ){

    uint8_t len = strnlen( src, 255 );

    return hash_u32_data( (uint8_t *)src, len );
}

#ifdef PGM_P
uint32_t hash_u32_string_P( PGM_P src ){

    char buf[64];
    strlcpy_P( buf, src, sizeof(buf) );

    return hash_u32_string( buf );
}
#endif

uint32_t hash_u32_start( void ){

    return FNV_32BIT_OFFSET_BASIS;
}

uint32_t hash_u32_partial( uint32_t hash, uint8_t *data, uint16_t len ){

    return fnv1a( hash, data, len );
}

uint32_t hash_u32_single( uint32_t hash, uint8_t data ){

    hash ^= data;
    hash *= FNV_32BIT_PRIME;

    return hash;
}   



#define FNV_64BIT_OFFSET_BASIS    (uint64_t)14695981039346656037UL
#define FNV_64BIT_PRIME           (uint64_t)1099511628211UL


static uint64_t fnv1a64( uint64_t base, uint8_t *data, uint16_t len ){

    uint64_t hash = base;

    while( len > 0 ){

        hash ^= *data;
        hash *= FNV_64BIT_PRIME;

        len--;
        data++;
    }

    return hash;
}

uint64_t hash_u64_data( uint8_t *data, uint16_t len ){
    
    if( len == 0 ){

        return 0;
    }

    return fnv1a64( FNV_64BIT_OFFSET_BASIS, data, len );
}

uint64_t hash_u64_string( char *src ){

    uint8_t len = strnlen( src, 255 );

    return hash_u64_data( (uint8_t *)src, len );
}

#ifdef PGM_P
uint64_t hash_u64_string_P( PGM_P src ){

    char buf[64];
    strlcpy_P( buf, src, sizeof(buf) );

    return hash_u64_string( buf );
}
#endif

uint64_t hash_u64_start( void ){

    return FNV_64BIT_OFFSET_BASIS;
}

uint64_t hash_u64_partial( uint64_t hash, uint8_t *data, uint16_t len ){

    return fnv1a64( hash, data, len );
}

uint64_t hash_u64_single( uint64_t hash, uint8_t data ){

    hash ^= data;
    hash *= FNV_64BIT_PRIME;

    return hash;
}   

