/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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
#include "catbus_common.h"

int64_t specific_to_i64( catbus_type_t8 type, const void *data ){

    int64_t i64_data = 0;
    uint64_t u64_data = 0;

    uint8_t buf[8] __attribute__((aligned(4)));

    // align to 32 bits
    uint16_t len = type_u16_size( type );

    if( len > 8 ){

        return -1;
    }

    memcpy( buf, data, len );

    data = buf;

    switch( type ){
        case CATBUS_TYPE_BOOL:
            i64_data = *(bool *)data;
            break;

        case CATBUS_TYPE_UINT8:
            i64_data = *(uint8_t *)data;
            break;

        case CATBUS_TYPE_UINT16:
            i64_data = *(uint16_t *)data;
            break;

        case CATBUS_TYPE_INT8:
            i64_data = *(int8_t *)data;
            break;

        case CATBUS_TYPE_INT16:
            i64_data = *(int16_t *)data;
            break;

        case CATBUS_TYPE_INT32:
            i64_data = *(int32_t *)data;
            break;

        case CATBUS_TYPE_FIXED16:
            i64_data = *(int32_t *)data;
            break;
            
        case CATBUS_TYPE_UINT32:
            i64_data = *(uint32_t *)data;
            break;

        case CATBUS_TYPE_UINT64:
            u64_data = *(uint64_t *)data;

            if( u64_data > INT64_MAX ){

                u64_data = INT64_MAX;
            }

            i64_data = u64_data;
            break;

        case CATBUS_TYPE_INT64:
            i64_data = *(int64_t *)data;
            break;

        case CATBUS_TYPE_FLOAT:
            i64_data = *(float *)data;
            break;

        default:
            return -1;
            break;
    }

    return i64_data;
}

void i64_to_specific( int64_t source_data, catbus_type_t8 type, void *data ){

    switch( type ){
        case CATBUS_TYPE_BOOL:
            *(bool *)data = ( source_data != 0 );
            break;

        case CATBUS_TYPE_UINT8:
            if( source_data > UINT8_MAX ){

                source_data = UINT8_MAX;
            }

            *(uint8_t *)data = source_data;
            break;

        case CATBUS_TYPE_UINT16:
            if( source_data > UINT16_MAX ){

                source_data = UINT16_MAX;
            }

            *(uint16_t *)data = source_data;
            break;

        case CATBUS_TYPE_INT8:
            if( source_data > INT8_MAX ){

                source_data = INT8_MAX;
            }
            else if( source_data < INT8_MIN ){

                source_data = INT8_MIN;
            }

            *(int8_t *)data = source_data;
            break;

        case CATBUS_TYPE_INT16:
            if( source_data > INT16_MAX ){

                source_data = INT16_MAX;
            }
            else if( source_data < INT16_MIN ){

                source_data = INT16_MIN;
            }

            *(int16_t *)data = source_data;
            break;

        case CATBUS_TYPE_INT32:
            if( source_data > INT32_MAX ){

                source_data = INT32_MAX;
            }
            else if( source_data < INT32_MIN ){

                source_data = INT32_MIN;
            }

            *(int32_t *)data = source_data;
            break;

        case CATBUS_TYPE_FIXED16:
            if( source_data > INT32_MAX ){

                source_data = INT32_MAX;
            }
            else if( source_data < INT32_MIN ){

                source_data = INT32_MIN;
            }

            *(int32_t *)data = source_data;
            break;
            
        case CATBUS_TYPE_UINT32:
            if( source_data > UINT32_MAX ){

                source_data = UINT32_MAX;
            }

            *(uint32_t *)data = source_data;
            break;

        case CATBUS_TYPE_UINT64:
            if( source_data > INT64_MAX ){

                source_data = INT64_MAX;
            }

            *(int64_t *)data = source_data;
            break;

        case CATBUS_TYPE_INT64:
            *(int64_t *)data = source_data;
            break;

        case CATBUS_TYPE_FLOAT:
            *(float *)data = source_data;
            break;

        default:
            break;
    }
}

int8_t type_i8_convert( 
    catbus_type_t8 dest_type,
    void *dest_data,
    catbus_type_t8 src_type,
    const void *src_data,
    uint16_t src_data_len ){

    // check for strings
    bool dst_string = type_b_is_string( dest_type );
    bool src_string = type_b_is_string( src_type );

    // get type sizes
    uint16_t src_size = type_u16_size( src_type );
    uint16_t dst_size = type_u16_size( dest_type );

    // bounds check against maximum conversion size
    if( src_size > CATBUS_CONVERT_BUF_LEN ){

        src_size = CATBUS_CONVERT_BUF_LEN;
    }

    if( dst_size > CATBUS_CONVERT_BUF_LEN ){

        dst_size = CATBUS_CONVERT_BUF_LEN;
    }

    if( ( src_data_len > 0 ) && ( src_size > src_data_len ) ){

        src_size = src_data_len;
    }

    // numeric to numeric
    if( !dst_string && !src_string ){
    
        int64_t src_i64 = specific_to_i64( src_type, src_data );
        int64_t dst_i64 = specific_to_i64( dest_type, dest_data );

        i64_to_specific( src_i64, dest_type, dest_data );

        // check if changing
        if( src_i64 > dst_i64 ){

            return 1;
        }
        else if( src_i64 < dst_i64 ){

            return -1;
        }
    }
    // string to string
    else if( dst_string && src_string ){

        // compute whichever size is minimum
        uint8_t size = src_size;
        if( dst_size < src_size ){

            size = dst_size;
        }
    
        // set dest to 0s
        memset( dest_data, 0, dst_size );

        // copy src to dest
        memcpy( dest_data, src_data, size );
    }
    // string to numeric
    else if( src_string ){

        // set dest to 0s
        memset( dest_data, 0, dst_size );
    }
    // numeric to string
    else if( dst_string ){

        // set dest to 0s
        memset( dest_data, 0, dst_size );
    }
    else{

        // shouldn't get here!
    }

    return 0;
}


uint16_t type_u16_size_meta( catbus_meta_t *meta ){

    return ( meta->count + 1 ) * type_u16_size( meta->type );
}


