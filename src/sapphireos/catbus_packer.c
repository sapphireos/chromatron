/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#include "keyvalue.h"



int8_t catbus_i8_init_pack_ctx( catbus_hash_t32 hash, catbus_pack_ctx_t *ctx ){

    catbus_meta_t meta;

    if( kv_i8_get_meta( hash, &meta ) < 0 ){

        return -1;
    }

    ctx->hash   = hash;
    ctx->type   = meta.type;
    ctx->count  = meta.count + 1;
    ctx->index  = 0;

    return 0;
}

bool catbus_b_pack_complete( catbus_pack_ctx_t *ctx ){

    return ctx->index >= ctx->count;
}

int16_t catbus_i16_pack( catbus_pack_ctx_t *ctx, void *buf, int16_t max_len ){

    int16_t type_len = type_u16_size( ctx->type );
    int16_t data_written = sizeof(catbus_pack_hdr_t);

    // need enough space for at least one entry
    int16_t minimum_space = sizeof(catbus_pack_hdr_t) + type_len;

    if( max_len < minimum_space ){

        return -1;
    }

    if( ctx->index >= ctx->count ){

        return -2;
    }

    catbus_pack_hdr_t *hdr = (catbus_pack_hdr_t *)buf;

    max_len -= sizeof(catbus_pack_hdr_t);

    // set up pack header
    hdr->hash       = ctx->hash;
    hdr->type       = ctx->type;
    hdr->index      = ctx->index;
    hdr->count      = 0;

    uint8_t *data = (uint8_t *)( hdr + 1 );

    while( ( max_len >= type_len ) && ( ctx->index < ctx->count ) ){

        if( kv_i8_array_get( ctx->hash, ctx->index, 1, data, type_len ) < 0 ){

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

    if( kv_i8_get_meta( hdr->hash, &meta ) < 0 ){

        return -1;
    }

    // verify types match
    if( hdr->type != meta.type ){

        return -2;
    }

    int16_t type_len = type_u16_size( hdr->type );
    int16_t data_read = sizeof(catbus_pack_hdr_t);

    // need enough space for at least one entry
    int16_t minimum_space = sizeof(catbus_pack_hdr_t) + type_len;

    if( len < minimum_space ){

        return -3;
    }

    uint8_t *data = (uint8_t *)( hdr + 1 );

    while( hdr->count > 0 ){

        if( len < type_len ){

            break;
        }

        if( kv_i8_array_set( hdr->hash, hdr->index, 1, data, type_len ) < 0 ){

            break;
        }

        len             -= type_len;
        data_read       += type_len;
        data            += type_len;
        hdr->count--;
        hdr->index++;
    }

    return data_read;
}


// #include "kvdb.h"
// #include "system.h"
// #include "logging.h"

// // #include "comm_printf.h"
// // #define PSTR(a) a
// // #define log_v_debug_P intf_v_printf

// int8_t test_packer( void ){
//     return 0;

//     uint32_t array[16];

//     for( uint8_t i = 0; i < cnt_of_array(array); i++ ){

//         array[i] = i + 1;
//     }

//     kvdb_i8_add( __KV__test_array, CATBUS_TYPE_UINT32, cnt_of_array(array), array, sizeof(array), "test_array" );

//     for( uint8_t i = 0; i < cnt_of_array(array); i++ ){

//         array[i] = 0;
//     }
//     kvdb_i8_add( __KV__test_array2, CATBUS_TYPE_UINT32, cnt_of_array(array), array, sizeof(array), "test_array2" );


//     log_v_debug_P( PSTR("testing packer...") );    


//     catbus_pack_ctx_t ctx;
//     uint8_t buf[31];
//     memset( buf, 0x99, sizeof(buf) );
//     catbus_pack_hdr_t *hdr = (catbus_pack_hdr_t *)buf;
//     uint8_t *data = (uint8_t *)( hdr + 1 );
//     int8_t status;

//     status = catbus_i8_init_pack_ctx( 1, &ctx );
//     if( status >= 0 ){

//         log_v_debug_P( PSTR("pack error 1") );        
//         return -1;
//     }

//     status = catbus_i8_init_pack_ctx( __KV__test_array, &ctx );
//     if( status < 0 ){

//         log_v_debug_P( PSTR("pack error 2") );        
//         return -1;
//     }

//     log_v_debug_P( PSTR("ctx: type %d idx %d cnt %d"), ctx.type, ctx.index, ctx.count ); 

//     int16_t written = catbus_i16_pack( &ctx, buf, sizeof(buf) );

//     uint32_t *i32_data = (uint32_t *)data;

//     log_v_debug_P( PSTR("packer wrote: %d hash: %lx idx: %u count: %u data0: %lu data1: %lu data2: %lu data3: %lu"), 
//         written, hdr->hash, hdr->index, hdr->count, i32_data[0], i32_data[1], i32_data[2], i32_data[3] );

//     // change hash
//     hdr->hash = __KV__test_array2;

//     written = catbus_i16_unpack( buf, sizeof(buf) );

//     log_v_debug_P( PSTR("unpacker read: %d data0: %lu"), written, *(uint32_t *)data );


//     written = catbus_i16_pack( &ctx, buf, sizeof(buf) );
//     log_v_debug_P( PSTR("packer wrote: %d hash: %lx idx: %u count: %u data0: %lu data1: %lu data2: %lu data3: %lu"), 
//         written, hdr->hash, hdr->index, hdr->count, i32_data[0], i32_data[1], i32_data[2], i32_data[3] );
//     hdr->hash = __KV__test_array2;
//     written = catbus_i16_unpack( buf, sizeof(buf) );
//     log_v_debug_P( PSTR("unpacker read: %d data0: %lu"), written, *(uint32_t *)data );

//     written = catbus_i16_pack( &ctx, buf, sizeof(buf) );
//     log_v_debug_P( PSTR("packer wrote: %d hash: %lx idx: %u count: %u data0: %lu data1: %lu data2: %lu data3: %lu"), 
//         written, hdr->hash, hdr->index, hdr->count, i32_data[0], i32_data[1], i32_data[2], i32_data[3] );
//     hdr->hash = __KV__test_array2;
//     written = catbus_i16_unpack( buf, sizeof(buf) );
//     log_v_debug_P( PSTR("unpacker read: %d data0: %lu"), written, *(uint32_t *)data );


//     written = catbus_i16_pack( &ctx, buf, sizeof(buf) );
//     log_v_debug_P( PSTR("packer wrote: %d hash: %lx idx: %u count: %u data0: %lu data1: %lu data2: %lu data3: %lu"), 
//         written, hdr->hash, hdr->index, hdr->count, i32_data[0], i32_data[1], i32_data[2], i32_data[3] );
//     hdr->hash = __KV__test_array2;
//     written = catbus_i16_unpack( buf, sizeof(buf) );
//     log_v_debug_P( PSTR("unpacker read: %d data0: %lu"), written, *(uint32_t *)data );



//     return 0;
// }









