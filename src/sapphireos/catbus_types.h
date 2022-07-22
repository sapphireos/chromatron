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

#ifndef __CATBUS_TYPES_H
#define __CATBUS_TYPES_H

#include <inttypes.h>
#include "bool.h"

typedef int8_t catbus_type_t8;

#define CATBUS_TYPE_NONE              0
#define CATBUS_TYPE_BOOL              1
#define CATBUS_TYPE_UINT8             2
#define CATBUS_TYPE_INT8              3
#define CATBUS_TYPE_UINT16            4
#define CATBUS_TYPE_INT16             5
#define CATBUS_TYPE_UINT32            6
#define CATBUS_TYPE_INT32             7
#define CATBUS_TYPE_UINT64            8
#define CATBUS_TYPE_INT64             9
#define CATBUS_TYPE_FLOAT             10
#define CATBUS_TYPE_FIXED16           11

#define CATBUS_TYPE_GFX16             20

#define CATBUS_TYPE_STRING128         40
#define CATBUS_TYPE_MAC48             41
#define CATBUS_TYPE_MAC64             42
#define CATBUS_TYPE_KEY128            43
#define CATBUS_TYPE_IPv4              44
#define CATBUS_TYPE_STRING512         45
#define CATBUS_TYPE_STRING32          46
#define CATBUS_TYPE_STRING64          47

#define CATBUS_TYPE_INVALID           255


#define CATBUS_TYPE_SIZE_INVALID      65535
uint16_t type_u16_size( catbus_type_t8 type );
bool type_b_is_string( catbus_type_t8 type );

#endif
