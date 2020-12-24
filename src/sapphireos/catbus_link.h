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

#ifndef __CATBUS_LINK_H
#define __CATBUS_LINK_H

#include "catbus_common.h"

#define LINK_PORT                         44633
#define LINK_VERSION                      1
#define LINK_MAGIC                        0x4b4e494c // 'LINK'


typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    uint64_t origin_id;
    catbus_hash_t32 universe;
} link_header_t;


void link_v_init( void );
void link_v_handle_shutdown( ip_addr4_t ip );
void link_v_shutdown( void );


#endif
