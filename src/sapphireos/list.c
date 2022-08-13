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

#include <inttypes.h>
#include "bool.h"
#include "memory.h"

#include "list.h"


#ifndef ENABLE_LIST_ATOMIC
    #undef ATOMIC
    #define ATOMIC
    #undef END_ATOMIC
    #define END_ATOMIC
#endif

void list_v_init( list_t *list ){

    list->head = -1;
    list->tail = -1;
    list->init = 1;
}


list_node_t list_ln_create_node( void *data, uint16_t len ){

    return list_ln_create_node2( data, len, MEM_TYPE_UNKNOWN );
}

list_node_t list_ln_create_node2( void *data, uint16_t len, mem_type_t8 type ){

    ATOMIC;

    // create memory object
    mem_handle_t h = mem2_h_alloc2( ( sizeof(list_node_state_t) - 1 ) + len, type );

    // create handle
    if( h < 0 ){

        goto end;
    }

    // get state pointer and initialize
    list_node_state_t *state = mem2_vp_get_ptr( h );

    state->prev = -1;
    state->next = -1;

    // check if data is provided
    if( data != 0 ){

        // copy data into netmsg object
        memcpy( &state->data, data, len );
    }

end:
    END_ATOMIC;

    return h;
}

// NOTE: this function does NOT remove the node from whatever list it was in.
// that needs to be done BEFORE calling this function!
void list_v_release_node( list_node_t node ){

    // mem2_v_free is already atomic
    mem2_v_free( node );
}

uint8_t list_u8_count( list_t *list ){

    ATOMIC;

    ASSERT( list->init != 0 );

    uint8_t count = 0;

    list_node_t node = list->head;

    while( node >= 0 ){

        count++;

        node = list_ln_next( node );
    }

    END_ATOMIC;

    return count;
}

uint16_t list_u16_size( list_t *list ){

    ATOMIC;

    ASSERT( list->init != 0 );

    uint16_t size = 0;

    list_node_t node = list->head;

    while( node >= 0 ){

        size += list_u16_node_size( node );

        node = list_ln_next( node );
    }

    END_ATOMIC;

    return size;
}

uint16_t list_u16_node_size( list_node_t node ){

    // atomic
    return ( mem2_u16_get_size( node ) - ( sizeof(list_node_state_t) - 1 ) );
}

#ifdef ENABLE_EXTENDED_VERIFY
void *_list_vp_get_data( list_node_t node, FLASH_STRING_T file, int line ){

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( node ) == FALSE ){

        sos_assert( 0, file, line );
    }
    
#else
void *list_vp_get_data( list_node_t node ){
#endif
    // atomic
	list_node_state_t *state = mem2_vp_get_ptr( node );

    return &state->data;
}

void list_v_insert_after( list_t *list, list_node_t node, list_node_t new_node ){

    ATOMIC;

    ASSERT( list->init != 0 );

    // get node states
    list_node_state_t *state = mem2_vp_get_ptr( node );
    list_node_state_t *new_state = mem2_vp_get_ptr( new_node );

    new_state->prev = node;
    new_state->next = state->next;

    if( state->next < 0 ){

        list->tail = new_node;
    }
    else{

        list_node_state_t *next_state = mem2_vp_get_ptr( state->next );

        next_state->prev = new_node;
    }

    state->next = new_node;

    END_ATOMIC;
}

void list_v_insert_tail( list_t *list, list_node_t node ){

    ASSERT( list->init != 0 );

    // get node state
    list_node_state_t *state = mem2_vp_get_ptr( node );

    // check if list is empty
    if( list->tail < 0 ){

        list->head = node;
        list->tail = node;

        state->prev = -1;
        state->next = -1;
    }
    else{

        list_v_insert_after( list, list->tail, node );
    }
}

void list_v_insert_head( list_t *list, list_node_t node ){

    ATOMIC;

    ASSERT( list->init != 0 );

    // get node state
    list_node_state_t *state = mem2_vp_get_ptr( node );

    // check if list is empty
    if( list->head < 0 ){

        list->head = node;
        list->tail = node;

        state->prev = -1;
        state->next = -1;
    }
    else{

        // insert at head
        list_node_state_t *head = mem2_vp_get_ptr( list->head );

        head->prev = node;

        state->prev = -1;
        state->next = list->head;

        list->head = node;
    }

    END_ATOMIC;
}

void list_v_remove( list_t *list, list_node_t node ){

    ATOMIC;

    ASSERT( list->init != 0 );

    // get node state
    list_node_state_t *state = mem2_vp_get_ptr( node );

    if( state->prev < 0 ){

        list->head = state->next;
    }
    else{

        // get previous state
        list_node_state_t *prev_state = mem2_vp_get_ptr( state->prev );

        prev_state->next = state->next;
    }

    if( state->next < 0 ){

        list->tail = state->prev;
    }
    else{

        // get next state
        list_node_state_t *next_state = mem2_vp_get_ptr( state->next );

        next_state->prev = state->prev;
    }

    END_ATOMIC;
}

list_node_t list_ln_remove_tail( list_t *list ){

    ATOMIC;

    ASSERT( list->init != 0 );

    list_node_t tail = list->tail;

    if( tail >= 0 ){

        list_v_remove( list, list->tail );
    }

    END_ATOMIC;

    return tail;
}

list_node_t list_ln_next( list_node_t node ){

    // atomic

    // get node state
    list_node_state_t *state = mem2_vp_get_ptr( node );

    return state->next;
}

list_node_t list_ln_prev( list_node_t node ){

    // atomic

    // get node state
    list_node_state_t *state = mem2_vp_get_ptr( node );

    return state->prev;
}

list_node_t list_ln_index( list_t *list, uint16_t index ){

    ATOMIC;

    ASSERT( list->init != 0 );

    list_node_t node = list->head;

    while( ( node >= 0 ) && ( index > 0 ) ){

        list_node_t next = list_ln_next( node );

        node = next;
        index--;
    }

    END_ATOMIC;

    return node;
}

bool list_b_is_empty( list_t *list ){

    ATOMIC;

    ASSERT( list->init != 0 );

    bool empty = ( list->head < 0 );

    END_ATOMIC;

    return empty;
}

// release every object in given list
void list_v_destroy( list_t *list ){

    ATOMIC;

    if( list->init == 0 ){
        // list is not intialized, nothing to destroy

        goto end;
    }

    list_node_t node = list->head;

    while( node >= 0 ){

        list_node_t next = list_ln_next( node );

        list_v_release_node( node );

        node = next;
    }

    list_v_init( list );

end:
    END_ATOMIC;
}

uint16_t list_u16_flatten( list_t *list, uint16_t pos, void *dest, uint16_t len ){

    ATOMIC;

    ASSERT( list->init != 0 );

    uint16_t current_pos = 0;
    uint16_t data_copied = 0;

    // seek to requested position
    list_node_t node = list->head;

    while( ( node >= 0 ) && ( len > 0 ) ){

        uint16_t node_size = list_u16_node_size( node );

        if( ( pos >= current_pos ) &&
            ( pos < ( current_pos + node_size ) ) ){

            uint16_t offset = pos - current_pos;
            uint16_t copy_len = node_size - offset;

            if( copy_len > len ){

                copy_len = len;
            }

            memcpy( dest, list_vp_get_data( node ) + offset, copy_len );

            data_copied += copy_len;
            dest += copy_len;
            pos += copy_len;
            len -= copy_len;
        }

        current_pos += node_size;
        node = list_ln_next( node );
    }

    END_ATOMIC;

    return data_copied;
}
