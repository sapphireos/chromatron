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

#include "list.h"
#include "ip.h"
#include "catbus_common.h"

#define LINK_PORT                           44633
#define LINK_VERSION                        1
#define LINK_MAGIC                          0x4b4e494c // 'LINK'

#define LINK_MAX_LINKS						16

#define LINK_SERVICE                        __KV__link

#define LINK_BASE_PRIORITY                  256

typedef list_node_t link_handle_t;

typedef uint8_t link_aggregation_t8;
#define LINK_AGG_ANY						0
#define LINK_AGG_MIN						1
#define LINK_AGG_MAX						2
#define LINK_AGG_SUM						3
#define LINK_AGG_AVG						4

typedef uint8_t link_mode_t8;
#define LINK_MODE_SEND						0
#define LINK_MODE_RECV						1
#define LINK_MODE_SYNC						2

typedef uint16_t link_filter_t16;
typedef uint16_t link_rate_t16;


typedef struct __attribute__((packed)){
    catbus_hash_t32 tag;
    catbus_hash_t32 source_hash;
    catbus_hash_t32 dest_hash;
    catbus_query_t query;
    link_mode_t8 mode;
    link_aggregation_t8 aggregation;
    link_filter_t16 filter;
    link_rate_t16 rate;    
} link_state_t;


typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    uint64_t origin_id;
    catbus_hash_t32 universe;
} link_msg_header_t;

// typedef struct __attribute__((packed)){
//     link_msg_header_t header;
    
// } link_msg_link_t;
// #define LINK_MSG_TYPE_LINK                1


void link_v_init( void );

link_state_t link_ls_assemble(
    link_mode_t8 mode, 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter );

link_state_t* link_ls_get_data( link_handle_t link );

bool link_b_compare( link_state_t *link1, link_state_t *link2 );
uint64_t link_u64_hash( link_state_t *link );
link_handle_t link_l_lookup( link_state_t *link );
link_handle_t link_l_lookup_by_hash( uint64_t hash );

link_handle_t link_l_create( 
    link_mode_t8 mode, 
    catbus_hash_t32 source_hash, 
    catbus_hash_t32 dest_hash, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter );
link_handle_t link_l_create2( link_state_t *state );

uint8_t link_u8_count( void );

void link_v_delete_by_tag( catbus_hash_t32 tag );
void link_v_delete_by_hash( uint64_t hash );

void link_v_handle_shutdown( ip_addr4_t ip );
void link_v_shutdown( void );


#endif
