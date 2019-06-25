/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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


/*

Dynamic Memory Pool Implementation

*/


/*
Notes:

Memory block format: header - data - canary

Heap format: used and dirty blocks - free space

Memory overhead is 6 bytes per handle used

*/


#include <string.h>

#include "threading.h"
#include "system.h"
#include "keyvalue.h"

#include "fs.h"
#include "memory.h"

//#define NO_LOGGING
#include "logging.h"

#include "event_log.h"

#ifdef ENABLE_ATOMIC_MEMORY
    #define MEM_ATOMIC ATOMIC
    #define MEM_END_ATOMIC END_ATOMIC
#else
    #define MEM_ATOMIC
    #define MEM_END_ATOMIC
#endif


static void *handles[MAX_MEM_HANDLES];

#if defined(__SIM__) || defined(ARM)
    static uint8_t _heap[MEM_HEAP_SIZE] MEMORY_HEAP;
    static uint8_t *heap = _heap;
#else
    static uint8_t *heap;
#endif

static void *free_space_ptr;

static mem_rt_data_t mem_rt_data;
static uint16_t stack_usage;

static uint32_t mem_allocs;
static uint32_t mem_alloc_fails;

#ifdef AVR
    extern uint8_t __stack, __heap_start, _end;
#endif

#ifdef ARM
    extern uint8_t _estack;
#endif


static uint16_t mem_info_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint16_t len ){

    uint16_t ret_val = 0;

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            // iterate over data length and fill info buffers as needed
            while( len > 0 ){

                uint16_t page = pos / sizeof(mem_info_t);

                // set up info page
                mem_info_t info;

                memset( &info, 0, sizeof(info) );

                if( handles[page] != 0 ){

                    mem_block_header_t header = mem2_h_get_header( page );

                    info.size = header.size;
                    info.type = header.type;
                }

                // get offset info page
                uint16_t offset = pos - ( page * sizeof(info) );

                // set copy length
                uint16_t copy_len = sizeof(info) - offset;

                if( copy_len > len ){

                    copy_len = len;
                }

                // copy data
                memcpy( ptr, (void *)&info + offset, copy_len );

                // adjust pointers
                ptr += copy_len;
                len -= copy_len;
                pos += copy_len;
                ret_val += copy_len;
            }

            break;

        case FS_VFILE_OP_SIZE:
            ret_val = sizeof(mem_info_t) * MAX_MEM_HANDLES;
            break;

        default:
            ret_val = 0;

            break;
    }

    return ret_val;
}

// KV:
static int8_t mem_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_GET ){

        uint16_t a = 0;

        if( hash == __KV__mem_max_handles ){

            a = MAX_MEM_HANDLES;
        }
        else if( hash == __KV__mem_max_stack ){

            a = MEM_MAX_STACK;
        }
        else if( hash == __KV__mem_stack ){

            a = mem2_u16_stack_count();
        }
        else{

            ASSERT( FALSE );
        }

        memcpy( data, &a, sizeof(a) );
    }

    return 0;
}

KV_SECTION_META kv_meta_t mem_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.handles_used,  0,  "mem_handles" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, mem_i8_kv_handler,           "mem_max_handles" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, mem_i8_kv_handler,           "mem_stack" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  0, mem_i8_kv_handler,           "mem_max_stack" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.heap_size,     0,  "mem_heap_size" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.free_space,    0,  "mem_free_space" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.peak_usage,    0,  "mem_peak_usage" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.used_space,    0,  "mem_used" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &mem_rt_data.dirty_space,   0,  "mem_dirty" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &mem_allocs,                0,  "mem_allocs" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &mem_alloc_fails,           0,  "mem_alloc_fails" },
};


#define CANARY_VALUE 0x47

#define MEM_BLOCK_SIZE( header ) ( sizeof(mem_block_header_t) + \
								( ( ~MEM_SIZE_DIRTY_MASK ) & header->size ) + \
								1 )

#define CANARY_PTR( header ) ( ( uint8_t * )header + \
							  MEM_BLOCK_SIZE( header ) - 1 )


#define SWIZZLE_VALUE 1

static void release_block( mem_handle_t handle );


static mem_handle_t swizzle( mem_handle_t handle ){

	return handle + SWIZZLE_VALUE;
}

static mem_handle_t unswizzle( mem_handle_t handle ){

	return handle - SWIZZLE_VALUE;
}


static uint8_t generate_canary( mem_block_header_t *header ){

	return 	CANARY_VALUE;
}


static bool is_dirty( mem_block_header_t *header ){

    if( ( header->size & MEM_SIZE_DIRTY_MASK ) != 0 ){

        return TRUE;
    }

    return FALSE;
}

static void set_dirty( mem_block_header_t *header ){

    header->size |= MEM_SIZE_DIRTY_MASK;
}

#ifndef ENABLE_EXTENDED_VERIFY
static void verify_handle( mem_handle_t handle ){

    ASSERT_MSG( handles[handle] != 0, "Invalid handle" );
    ASSERT( handle >= 0 );

	mem_block_header_t *header = handles[handle];
	uint8_t *canary = CANARY_PTR( header );

	ASSERT_MSG( *canary == generate_canary( header ), "Invalid canary" );
	ASSERT_MSG( is_dirty( header ) == FALSE, "Memory block is marked as dirty" );
}
#endif

#if defined(__SIM__) || defined(BOOTLOADER)
    void stack_fill( void ){}

#elif defined(ARM)
    void stack_fill( void ){


    }

#elif defined(AVR)
    void stack_fill( void ) __attribute__ ((section (".init1"), naked, used));

    void stack_fill( void ){

        __asm volatile(
            "ldi r30, lo8(__stack)\n"
            "ldi r31, hi8(__stack)\n"
            "ldi r24, lo8(__stack)\n"
            "ldi r25, hi8(__stack)\n"
            "subi r24, lo8(_end)\n"
            "sbci r25, hi8(_end)\n"
            "ldi r16, 0x47\n" // CANARY
            ".loop:\n"
            "   st -Z, r16\n"
            "   sbiw r24, 1\n"
            "   brne .loop\n"
        );
    }
#else 
    void stack_fill( void ){}
#endif

void mem2_v_init( void ){

    #if defined(__SIM__) || defined(ARM)
    uint16_t heap_size = cnt_of_array(_heap);
    #elif defined(AVR)
    uint16_t heap_start = (uintptr_t)&__heap_start;
    uint16_t stack_end = (uintptr_t)&__stack - ( MEM_MAX_STACK + MEM_STACK_HEAP_GUARD );
    uint16_t heap_size = stack_end - heap_start;
    heap = (uint8_t *)heap_start;
    #endif

    mem_rt_data.heap_size = heap_size;
    mem_rt_data.free_space = heap_size;
	free_space_ptr = heap;

	mem_rt_data.used_space = 0;
	mem_rt_data.data_space = 0;
	mem_rt_data.dirty_space = 0;

	for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

		handles[i] = 0;
	}

	mem_rt_data.handles_used = 0;

    fs_f_create_virtual( PSTR("handleinfo"), mem_info_vfile_handler );
}

// for debug only, returns a copy of the header at given index
mem_block_header_t mem2_h_get_header( uint16_t index ){

    MEM_ATOMIC;

    mem_block_header_t header_copy;
    memset( &header_copy, 0, sizeof(header_copy) );

    // check if block exists
    if( handles[index] != 0 ){

        mem_block_header_t *block_header = handles[index];

        memcpy( &header_copy, block_header, sizeof(header_copy) );
    }

    MEM_END_ATOMIC;

    return header_copy;
}

// external API for verifying a handle.
// used for checking for bad handles and asserting at the
// source file instead of within the memory module, where it is difficult
// to find the source of the problem.
// returns TRUE if handle is valid
bool mem2_b_verify_handle( mem_handle_t handle ){

    bool status = TRUE;

    MEM_ATOMIC;

    // check if handle is negative
    if( handle < 0 ){

        sys_v_set_error( SYS_ERR_INVALID_HANDLE );

        status = FALSE;
        goto end;
    }

    // unswizzle the handle
    handle = unswizzle(handle);

    if( handles[handle] == 0 ){

        sys_v_set_error( SYS_ERR_HANDLE_UNALLOCATED );

        status = FALSE;
        goto end;
    }

    // get header and canary
    mem_block_header_t *header = handles[handle];
	uint8_t *canary = CANARY_PTR( header );

    if( *canary != generate_canary( header ) ){

        sys_v_set_error( SYS_ERR_INVALID_CANARY );

        status = FALSE;
        goto end;
    }

    if( is_dirty( header ) == TRUE ){

        sys_v_set_error( SYS_ERR_MEM_BLOCK_IS_DIRTY );

        status = FALSE;
        goto end;
    }

end:
    MEM_END_ATOMIC;

    return status;
}

static mem_handle_t alloc( uint16_t size, mem_type_t8 type ){

    MEM_ATOMIC;

    mem_handle_t handle = -1;

    #ifdef ARM
    uint8_t padding_len = 4 - ( ( sizeof(mem_block_header_t) + size + 1 ) % 4 );

    size += padding_len;
    #endif

    // check if there is free space available
    if( mem_rt_data.free_space < ( size + sizeof(mem_block_header_t) + 1 ) ){

        // allocation failed
        goto finish;
    }

    // get a handle
    for( handle = 0; handle < MAX_MEM_HANDLES; handle++ ){

        if( handles[handle] == 0 ){

            mem_rt_data.handles_used++;

            break;
        }
    }

    // if a handle was not found
    if( handle >= MAX_MEM_HANDLES ){

        // reset handle to unallocated
        handle = -1;

        // handle allocation failed
        goto finish;
    }

    // create the memory block
    handles[handle] = free_space_ptr;

    mem_block_header_t *header = handles[handle];

    header->size = size;
    header->handle = handle;
    header->type = type;
    #ifdef ARM
    header->padding_len = padding_len;
    #endif

    #ifdef ENABLE_RECORD_CREATOR
    // notice we multiply the address by 2 (left shift 1) to get the byte address
    header->creator_address = (uint16_t)thread_p_get_function( thread_t_get_current_thread() ) << 1;
    #endif

    // set data space
    mem_rt_data.data_space += size;

    // set canary
    uint8_t *canary = CANARY_PTR( header );

    *canary = generate_canary( header );

    // adjust free space
    free_space_ptr += MEM_BLOCK_SIZE( header );

    mem_rt_data.free_space -= MEM_BLOCK_SIZE( header );

    ASSERT_MSG( mem_rt_data.free_space <= mem_rt_data.heap_size, "Free space invalid!" );

    // adjust used space counter
    mem_rt_data.used_space += MEM_BLOCK_SIZE( header );

    // adjust peak usage state
    if( mem_rt_data.peak_usage < mem_rt_data.used_space ){

        mem_rt_data.peak_usage = mem_rt_data.used_space;
    }

    handle = swizzle(handle);

    mem_allocs++;

finish:

    if( handle < 0 ){

        sys_v_set_warnings( SYS_WARN_MEM_FULL );

        mem_alloc_fails++;

        ASSERT( TRUE );
    }

    MEM_END_ATOMIC;

    return handle;
}



// attempt to allocate a memory block of a specified size
// returns -1 if the allocation failed.
mem_handle_t mem2_h_alloc( uint16_t size ){

    return alloc( size, MEM_TYPE_UNKNOWN );
}


// attempt to allocate a memory block of a specified size
// returns -1 if the allocation failed.
mem_handle_t mem2_h_alloc2( uint16_t size, mem_type_t8 type ){

    return alloc( size, type );
}

// attempt to reallocate handle with a new size.
// returns -1 if unsuccessful.
// new memory block will be assigned to the original handle,
// however, all direct pointers will be invalid!
#ifdef ENABLE_EXTENDED_VERIFY
int8_t _mem2_i8_realloc( mem_handle_t handle, uint16_t size, FLASH_STRING_T file, int line ){

    MEM_ATOMIC;

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( 0, file, line );
    }

#else
int8_t mem2_i8_realloc( mem_handle_t handle, uint16_t size ){

    MEM_ATOMIC;

#endif

    #ifndef ENABLE_EXTENDED_VERIFY
    verify_handle( handle );
    #endif

    int8_t status = 0;

    // get header
    handle = unswizzle( handle );
    mem_block_header_t *header = handles[handle];

    // allocate new block
    mem_handle_t new_handle = alloc( size, header->type );

    // check allocation
    if( new_handle < 0 ){

        status = -1;

        goto end;
    }

    new_handle = unswizzle( new_handle );
    mem_block_header_t *new_header = handles[new_handle];

    // move data to new handle
    void *old_ptr = handles[handle] + sizeof( mem_block_header_t );;
    void *new_ptr = handles[new_handle] + sizeof( mem_block_header_t );;
    memcpy( new_ptr, old_ptr, header->size );

    // release old handle
    release_block( handle );

    // replace handle with the original one    
    new_header->handle  = handle;
    handles[handle]     = new_header;
    handles[new_handle] = 0;


end:
    MEM_END_ATOMIC;

    return status;
}

mem_handle_t mem2_h_get_handle( uint16_t index, mem_type_t8 type ){

    if( index >= cnt_of_array(handles) ){

        return MEM_ERR_END_OF_LIST;
    }

    mem_block_header_t *header = handles[index];
    
    if( header == 0 ){

        return MEM_ERR_INVALID_HANDLE;        
    }

    if( header->type != type ){

        return MEM_ERR_INCORRECT_TYPE;
    }

    return swizzle(index);    
}


// free the memory associated with the given handle
#ifdef ENABLE_EXTENDED_VERIFY
void _mem2_v_free( mem_handle_t handle, FLASH_STRING_T file, int line ){

    MEM_ATOMIC;

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( 0, file, line );
    }

#else
void mem2_v_free( mem_handle_t handle ){

    MEM_ATOMIC;

#endif

	handle = unswizzle(handle);

	if( handle >= 0 ){

        #ifndef ENABLE_EXTENDED_VERIFY
		verify_handle( handle );
        #endif

		release_block( handle );
	}

    MEM_END_ATOMIC;
}

static void release_block( mem_handle_t handle ){

	// get pointer to the header
	mem_block_header_t *header = handles[handle];

	// clear the handle
	handles[handle] = 0;

	mem_rt_data.handles_used--;

	// decrement data space used
	mem_rt_data.data_space -= header->size;

	// set the flags to dirty so the defragmenter can pick it up
	set_dirty( header );

	// increment dirty space counter and decrement used space counter
	mem_rt_data.dirty_space += MEM_BLOCK_SIZE( header );
	mem_rt_data.used_space -= MEM_BLOCK_SIZE( header );
}

#ifdef ENABLE_EXTENDED_VERIFY
uint16_t _mem2_u16_get_size( mem_handle_t handle, FLASH_STRING_T file, int line ){

    MEM_ATOMIC;

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( 0, file, line );
    }

#else
uint16_t mem2_u16_get_size( mem_handle_t handle ){

    MEM_ATOMIC;
#endif

	handle = unswizzle(handle);

    #ifndef ENABLE_EXTENDED_VERIFY
	verify_handle( handle );
    #endif

	// get pointer to the header
	mem_block_header_t *header = handles[handle];

    #ifdef ARM
    uint16_t size = header->size - header->padding_len;
    #else
    uint16_t size = header->size;
    #endif

    MEM_END_ATOMIC;

	return size;
}

// Get a pointer to the memory at given handle.
// NOTES:
// This function is dangerous, but the fact is it is necessary to allow
// applications to map structs to memory without copying to a temp variable.
// IMPORTANT!!! The pointer returned by this function is only valid until the
// next time the garbage collector runs, IE, if the application thread yields
// to the scheduler, it needs to call this function again to get a fresh
// pointer.  Otherwise the garbage collector may move the application's memory
// and the app's old pointer will point to invalid memory.
#ifdef ENABLE_EXTENDED_VERIFY
void *_mem2_vp_get_ptr( mem_handle_t handle, FLASH_STRING_T file, int line ){

    MEM_ATOMIC;

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( 0, file, line );
    }

#else
void *mem2_vp_get_ptr( mem_handle_t handle ){

    MEM_ATOMIC;

#endif

	handle = unswizzle(handle);

	#ifndef ENABLE_EXTENDED_VERIFY
	verify_handle( handle );
	#endif

    void *ptr = handles[handle] + sizeof( mem_block_header_t );

    MEM_END_ATOMIC;

	return ptr;
}

void *mem2_vp_get_ptr_fast( mem_handle_t handle ){

    MEM_ATOMIC;

	handle = unswizzle(handle);

    void *ptr = handles[handle] + sizeof( mem_block_header_t );

    MEM_END_ATOMIC;

	return ptr;
}

// check the canaries for all allocated handles.
// this function will assert on any failures.
void mem2_v_check_canaries( void ){

	MEM_ATOMIC;

	for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

		// if the handle is allocated
		if( handles[i] != 0 ){

			// get pointer to the header
			mem_block_header_t *header = handles[i];

			uint8_t *canary = CANARY_PTR( header );

			// check the canary
			ASSERT_MSG( *canary == generate_canary( header ), "Invalid canary!" );
		}
	}

    MEM_END_ATOMIC;
}

uint16_t mem_u16_get_stack_usage( void ){

    MEM_ATOMIC;

    uint16_t temp = stack_usage;

    MEM_END_ATOMIC;

    return temp;
}

uint8_t mem2_u8_get_handles_used( void ){

    MEM_ATOMIC;

    uint8_t temp = mem_rt_data.handles_used;

    MEM_END_ATOMIC;

	return temp;
}

void mem2_v_get_rt_data( mem_rt_data_t *rt_data ){

    MEM_ATOMIC;

	*rt_data = mem_rt_data;

    MEM_END_ATOMIC;
}

// return amount of free memory available
uint16_t mem2_u16_get_free( void ){

    MEM_ATOMIC;

    uint16_t temp = mem_rt_data.free_space;

    MEM_END_ATOMIC;

    return temp;
}

// return amount of dirty memory
uint16_t mem2_u16_get_dirty( void ){

    MEM_ATOMIC;

    uint16_t temp = mem_rt_data.dirty_space;

    MEM_END_ATOMIC;

    return temp;
}

/*

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            DANGER DANGER DANGER
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

Do *NOT* call this function from anywhere other than
the thread scheduler!  It moves any and all memory around,
so any open handles will be corrupted!  If this happens,
the system will crash unpredictably and the built in
error indications will likely be misleading.

YOU HAVE BEEN WARNED!

*/
void mem2_v_collect_garbage( void ){

    if( mem2_u16_get_dirty() < MEM_DEFRAG_THRESHOLD ){

        return;
    }

    MEM_ATOMIC;

    EVENT( EVENT_ID_MEM_DEFRAG, 0 );

    mem_block_header_t *clean;
    mem_block_header_t *next_block;
    mem_block_header_t *dirty;

    clean = ( mem_block_header_t * )heap;
    next_block = ( mem_block_header_t * )heap;
    dirty = ( mem_block_header_t * )heap;

    // search for a dirty block (loop while clean blocks)
    while( ( is_dirty( dirty ) == FALSE ) &&
            ( dirty < ( mem_block_header_t * )free_space_ptr ) ){

        dirty = ( void * )dirty + MEM_BLOCK_SIZE( dirty );
    }

    // clean pointer needs to lead dirty pointer
    if( clean < dirty ){

        clean = dirty;
    }

    while( clean < ( mem_block_header_t * )free_space_ptr ){

        // search for a clean block (loop while dirty)
        // (couldn't find anymore clean blocks to move, so there should be
        // no clean blocks between the dirty pointer and the free pointer)
        while( is_dirty( clean ) == TRUE ){

            clean = ( void * )clean + MEM_BLOCK_SIZE( clean );

            // if the free space is reached, the routine is finished
            if( clean >= ( mem_block_header_t * )free_space_ptr ){

                goto done_defrag;
            }
        }

        // get next block
        next_block = ( void * )clean + MEM_BLOCK_SIZE( clean );

        // switch the handle from the old block to the new block
        handles[clean->handle] = dirty;
        // NOTE:
        // Could do this instead, and save the need to store the handle index
        // in the block, saving 2 bytes per object.
        /*
        for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

            if( handles[i] == clean ){

                handles[i] = dirty;
                break;
            }
        }
        */

        // copy the clean block to the dirty block pointer
        memcpy( dirty, clean, MEM_BLOCK_SIZE( clean ) );

        // increment dirty pointer
        dirty = ( void * )dirty + MEM_BLOCK_SIZE( dirty );

        // assign clean pointer to next block
        clean = next_block;
    }

done_defrag:
    // EVENT( EVENT_ID_MEM_DEFRAG, 1 );

    // there should be no clean blocks between the dirty and free pointers,
    // set the free pointer to the dirty pointer
    free_space_ptr = dirty;

    // the dirty space is now free
    mem_rt_data.free_space += mem_rt_data.dirty_space;

    // no more dirty space
    mem_rt_data.dirty_space = 0;

    // run canary check
    mem2_v_check_canaries();

    // EVENT( EVENT_ID_MEM_DEFRAG, 2 );

    // check stack guard
    #ifdef AVR
    uint8_t *stack = &__stack - ( MEM_MAX_STACK - MEM_STACK_GUARD_SIZE );
    ASSERT( *stack++ == CANARY_VALUE );
    ASSERT( *stack++ == CANARY_VALUE );
    ASSERT( *stack++ == CANARY_VALUE );
    ASSERT( *stack++ == CANARY_VALUE );

    // assert if the stack is blown up
    // ASSERT( stack_usage < MEM_MAX_STACK );
    #endif

    EVENT( EVENT_ID_MEM_DEFRAG, 3 );

    MEM_END_ATOMIC;
}


uint16_t mem2_u16_stack_count( void ){

#ifdef __SIM__
    return 0;
#endif
#ifdef AVR
    uint16_t count = 0;
    uint8_t *stack = &__stack - MEM_MAX_STACK;

    while( count < MEM_MAX_STACK ){

        if( *stack != CANARY_VALUE ){

            break;
        }

        stack++;
        count++;
    }

    stack_usage = MEM_MAX_STACK - count;

    return stack_usage;    
#endif
#ifdef ARM
    uint16_t count = stack_usage;
    uint8_t *stack = &_estack - MEM_MAX_STACK;
    stack += count;

    while( count < MEM_MAX_STACK ){

        if( *stack != CANARY_VALUE ){

            break;
        }

        stack++;
        count++;
    }

    stack_usage = MEM_MAX_STACK - count;

    return stack_usage; 
#endif
}
