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

#ifndef _LIST_H
#define _LIST_H

#include "memory.h"

typedef mem_handle_t list_node_t;

typedef struct{
    list_node_t prev;
    list_node_t next;
    uint8_t     data; // first byte of data
} list_node_state_t;

typedef struct{
    uint8_t init;
    list_node_t head;
    list_node_t tail;
} list_t;


void list_v_init( list_t *list );

list_node_t list_ln_create_node( void *data, uint16_t len );
list_node_t list_ln_create_node2( void *data, uint16_t len, mem_type_t8 type );
void list_v_release_node( list_node_t node );
uint8_t list_u8_count( list_t *list );
uint16_t list_u16_size( list_t *list );
uint16_t list_u16_node_size( list_node_t node );


#ifdef ENABLE_EXTENDED_VERIFY
    void *_list_vp_get_data( list_node_t node, FLASH_STRING_T file, int line );

    #define list_vp_get_data(node)         _list_vp_get_data(node, __SOURCE_FILE__, __LINE__ )
#else
    void *list_vp_get_data( list_node_t node );
#endif


void list_v_insert_after( list_t *list, list_node_t node, list_node_t new_node );
void list_v_insert_tail( list_t *list, list_node_t node );
void list_v_insert_head( list_t *list, list_node_t node );
void list_v_remove( list_t *list, list_node_t node );
list_node_t list_ln_remove_tail( list_t *list );
list_node_t list_ln_next( list_node_t node );
list_node_t list_ln_prev( list_node_t node );
list_node_t list_ln_index( list_t *list, uint16_t index );

bool list_b_is_empty( list_t *list );

void list_v_destroy( list_t *list );

uint16_t list_u16_flatten( list_t *list, uint16_t pos, void *dest, uint16_t len );

#endif

