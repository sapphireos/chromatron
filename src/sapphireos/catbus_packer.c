/*
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
 */

#include <string.h>
#include "catbus_packer.h"
#include "catbus_common.h"
#include "catbus_types.h"

#include "kvdb.h"



int8_t catbus_i8_init_pack_ctx( catbus_hash_t32 hash, catbus_pack_ctx_t *ctx ){

    catbus_meta_t meta;

    if( kvdb_i8_get_meta( hash, &meta ) < 0 ){

        return -1;
    }

    ctx->hash   = hash;
    ctx->type   = meta.type;
    ctx->count  = meta.count + 1;
    ctx->index  = 0;

    return 0;
}

int16_t catbus_i16_pack( catbus_pack_ctx_t *ctx, void *buf, int16_t max_len ){

    int16_t type_len = type_u16_size( ctx->type );
    int16_t data_written = 0;

    // need enough space for at least one entry
    int16_t minimum_space = sizeof(catbus_pack_hdr_t) + type_len;

    if( max_len < minimum_space ){

        return -1;
    }

    catbus_pack_hdr_t *hdr = (catbus_pack_hdr_t *)buf;

    max_len -= sizeof(catbus_pack_hdr_t);

    // set up pack header
    hdr->hash       = ctx->hash;
    hdr->type       = ctx->type;
    hdr->index      = ctx->index;
    hdr->count      = 0;

    uint8_t *data = (uint8_t *)( hdr + 1 );

    while( max_len >= type_len ){

        if( kvdb_i8_array_get( ctx->hash, ctx->type, ctx->index, data, type_len ) < 0 ){

            break;
        }

        data            += type_len;
        max_len         -= type_len;
        data_written    += type_len;
        ctx->index++;
        hdr->count++;
    }

    return data_written;
}

int16_t catbus_i16_unpack( const void *buf, int16_t len ){

    catbus_pack_hdr_t *hdr = (catbus_pack_hdr_t *)buf;
    
    catbus_meta_t meta;

    if( kvdb_i8_get_meta( hdr->hash, &meta ) < 0 ){

        return -1;
    }

    // verify types match
    if( hdr->type != meta.type ){

        return -2;
    }

    int16_t type_len = type_u16_size( hdr->type );
    int16_t data_written = 0;

    // need enough space for at least one entry
    int16_t minimum_space = sizeof(catbus_pack_hdr_t) + type_len;

    if( len < minimum_space ){

        return -1;
    }

    uint8_t *data = (uint8_t *)( hdr + 1 );

    while( hdr->count > 0 ){

        if( len < type_len ){

            break;
        }

        if( kvdb_i8_array_set( hdr->hash, hdr->type, hdr->index, data, type_len ) < 0 ){

            break;
        }

        buf             += type_len;
        len             -= type_len;
        data_written    += type_len;
        hdr->count--;
        hdr->index++;
    }

    return data_written;
}







