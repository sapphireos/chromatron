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

#ifndef __LINK_H
#define __LINK_H

// #define LINK_PORT                           44634
#define LINK2_PORT                           44637
#define LINK2_MGR_PORT                       44638



#define LINK_MIN_TICK_RATE                  20
#define LINK_MAX_TICK_RATE                  2000

#define LINK_RETRANSMIT_RATE                2000

#define LINK2_MGR_LINK_TIMEOUT				60

typedef list_node_t link2_handle_t;

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
#define LINK_FILTER_OFF                     0

typedef uint16_t link_rate_t16;
#define LINK_RATE_MIN                       20
#define LINK_RATE_1000ms                    1000
#define LINK_RATE_MAX                       30000

// DO NOT EXCEED 32767 ms on MAX LINK RATE
// internal timers use int16!
// NOTE link rate min is also the link timer tick rate!

typedef struct __attribute__((packed)){
    link_mode_t8 mode;
    link_aggregation_t8 aggregation;
    link_rate_t16 rate;
    catbus_hash_t32 source_key;
    catbus_hash_t32 dest_key;
    catbus_hash_t32 tag;
    catbus_query_t query;
} link2_t; // 48 bytes


typedef struct __attribute__((packed)){
    link2_t link;

    uint32_t data_hash;
    int16_t retransmit_timer;
    int16_t ticks;

    uint64_t hash; // must be last!
} link2_state_t;


#define LINK_BINDING_TIMEOUT  30

typedef struct __attribute__((packed)){
    catbus_hash_t32 key;
    uint16_t rate;
    uint8_t flags;
    uint8_t reserved;
} link2_binding_t;
#define LINK_MAX_BIND_ENTRIES         ( ( UDP_MAX_LEN - sizeof(link2_msg_header_t) ) / sizeof(link2_binding_t) )
#define LINK_BIND_FLAG_SOURCE   0x01
#define LINK_BIND_FLAG_SINK     0x02

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
    uint8_t flags;
    uint8_t reserved;
    uint64_t origin_id;
    catbus_hash_t32 universe;
} link2_msg_header_t;

typedef struct __attribute__((packed)){
    link2_msg_header_t header;
} link_msg_link_t;
#define LINK_MSG_TYPE_LINK        		1

typedef struct __attribute__((packed)){
    link2_msg_header_t header;
    // link2_binding_t follows
} link_msg_bind_t;
#define LINK_MSG_TYPE_BIND              2

#define LINK_MSG_TYPE_DATA              3


void link2_v_init( void );

uint64_t link2_u64_hash( link2_t *link );

link2_handle_t link2_l_create( 
    link_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link_rate_t16 rate,
    link_aggregation_t8 aggregation,
    link_filter_t16 filter );
link2_handle_t link2_l_create2( link2_state_t *state );

void link2_v_init_header( link2_msg_header_t *header, uint8_t msg_type );


void link_mgr_v_start( void );
void link_mgr_v_stop( void );


#endif
