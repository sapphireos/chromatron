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

#include "catbus_types.h"


uint16_t type_u16_size( catbus_type_t8 type ){

    uint16_t size = CATBUS_TYPE_SIZE_INVALID;

    switch( type ){
        case CATBUS_TYPE_NONE:
            size = 0;
            break;

        case CATBUS_TYPE_BOOL:
        case CATBUS_TYPE_UINT8:
        case CATBUS_TYPE_INT8:
            size = 1;
            break;

        case CATBUS_TYPE_UINT16:
        case CATBUS_TYPE_INT16:
            size = 2;
            break;

        case CATBUS_TYPE_UINT32:
        case CATBUS_TYPE_INT32:
        case CATBUS_TYPE_FLOAT:
        case CATBUS_TYPE_FIXED16:
        case CATBUS_TYPE_GFX16:
        case CATBUS_TYPE_IPv4:
            size = 4;
            break;

        case CATBUS_TYPE_UINT64:
        case CATBUS_TYPE_INT64:
            size = 8;
            break;

        case CATBUS_TYPE_STRING128:
            size = 128;
            break;

        case CATBUS_TYPE_STRING32:
            size = 32;
            break;

        case CATBUS_TYPE_STRING64:
            size = 64;
            break;

        case CATBUS_TYPE_STRING512:
            //size = 512;
            size = 488; 
            // NOTE!!! 
            // Currently catbus only supports 512 bytes of data *total* per message.
            // It is not a trivial matter to change this.
            // Since it is not terribly likely to actually need a full 512 byte string,
            // we will artificially constrain it to a number that fits the catbus limitations.
            break;

        case CATBUS_TYPE_MAC48:
            size = 6;
            break;

        case CATBUS_TYPE_MAC64:
            size = 8;
            break;

        case CATBUS_TYPE_KEY128:
            size = 16;
            break;

    }

    return size;
}

bool type_b_is_string( catbus_type_t8 type ){

    switch( type){
        case CATBUS_TYPE_STRING128:
        case CATBUS_TYPE_STRING32:
        case CATBUS_TYPE_STRING64:
        case CATBUS_TYPE_STRING512:
        case CATBUS_TYPE_MAC48:
        case CATBUS_TYPE_MAC64:
        case CATBUS_TYPE_KEY128:
            return TRUE;
            break;
    }

    return FALSE;
}
