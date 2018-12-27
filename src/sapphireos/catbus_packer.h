/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#ifndef __CATBUS_PACKER_H
#define __CATBUS_PACKER_H

#include <stdint.h>

#include "catbus_common.h"

typedef struct{
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    uint16_t index;
    uint16_t count;
} catbus_pack_ctx_t;

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    catbus_type_t8 type;
    uint16_t index;
    uint8_t count;
} catbus_pack_hdr_t;


int8_t catbus_i8_init_pack_ctx( catbus_hash_t32 hash, catbus_pack_ctx_t *ctx );
bool catbus_b_pack_complete( catbus_pack_ctx_t *ctx );
int16_t catbus_i16_pack( catbus_pack_ctx_t *ctx, void *buf, int16_t max_len );
int16_t catbus_i16_unpack( const void *buf, int16_t max_len );



// int8_t test_packer( void );

#endif
