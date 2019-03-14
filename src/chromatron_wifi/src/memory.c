

#include <string.h>

#include "memory.h"

#include "comm_printf.h"
#include "comm_intf.h"


static void *handles[MAX_MEM_HANDLES];
static uint8_t *heap;
static uint8_t *free_space_ptr;
static mem_rt_data_t mem_rt_data;

static uint32_t mem_allocs;
static uint32_t mem_alloc_fails;

void mem_assert( const char* file, int line, const char *format, ... ){

    intf_v_led_on();

    intf_v_serial_printf( "\n\n\nAssert File: %s Line: %d", file, line );

    if( format != 0 ){

        va_list ap;
        va_start( ap, format );
        intf_v_assert_printf( format, ap );
        va_end( ap );
    }

    // wait for watchdog to kick us
    while(1){};
}

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

    return  CANARY_VALUE;
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





void mem2_v_init( uint8_t *_heap, uint16_t size ){
    heap = _heap;
    mem_rt_data.free_space = size;
    free_space_ptr = heap;

    mem_rt_data.heap_size = size;
    mem_rt_data.used_space = 0;
    mem_rt_data.data_space = 0;
    mem_rt_data.dirty_space = 0;

    for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

        handles[i] = 0;
    }

    mem_rt_data.handles_used = 0;

    ASSERT_MSG( ( (uint32_t)free_space_ptr & 3 ) == 0, "free_space_ptr misalign addr: 0x%0x", (uint32_t)free_space_ptr );
}

// for debug only, returns a copy of the header at given index
mem_block_header_t mem2_h_get_header( uint16_t index ){


    mem_block_header_t header_copy;
    memset( &header_copy, 0, sizeof(header_copy) );

    // check if block exists
    if( handles[index] != 0 ){

        mem_block_header_t *block_header = handles[index];

        memcpy( &header_copy, block_header, sizeof(header_copy) );
    }

    return header_copy;
}

// external API for verifying a handle.
// used for checking for bad handles and asserting at the
// source file instead of within the memory module, where it is difficult
// to find the source of the problem.
// returns TRUE if handle is valid
bool mem2_b_verify_handle( mem_handle_t handle ){

    bool status = TRUE;


    // check if handle is negative
    if( handle < 0 ){

        status = FALSE;
        goto end;
    }

    // unswizzle the handle
    handle = unswizzle(handle);

    if( handles[handle] == 0 ){

        status = FALSE;
        goto end;
    }

    // get header and canary
    mem_block_header_t *header = handles[handle];
    uint8_t *canary = CANARY_PTR( header );

    if( *canary != generate_canary( header ) ){

        status = FALSE;
        goto end;
    }

    if( is_dirty( header ) == TRUE ){

        status = FALSE;
        goto end;
    }

end:

    return status;
}

static mem_handle_t alloc( uint16_t size, mem_type_t8 type ){

    mem_handle_t handle = -1;

    uint16_t total_size = sizeof(mem_block_header_t) + size + 1;

    uint8_t padding_len = 4 - ( total_size & 3 );

    size += padding_len;
    total_size += padding_len;

    // check if there is free space available
    if( mem_rt_data.free_space < total_size ){

        // allocation failed
        goto finish;
    }

    ASSERT_MSG( ( total_size & 3 ) == 0, "Size misalign: %u requested: %u pad: %u", (uint32_t)total_size, (uint32_t)size, (uint32_t)padding_len );

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
    header->padding_len = padding_len;

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

        mem_alloc_fails++;

        ASSERT( TRUE );
    }

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
int8_t _mem2_i8_realloc( mem_handle_t handle, uint16_t size, const char* file, int line ){


    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( file, line, 0 );
    }

#else
int8_t mem2_i8_realloc( mem_handle_t handle, uint16_t size ){


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
    uint8_t *old_ptr = (uint8_t*)handles[handle] + sizeof( mem_block_header_t );
    uint8_t *new_ptr = (uint8_t*)handles[new_handle] + sizeof( mem_block_header_t );
    memcpy( new_ptr, old_ptr, header->size );

    // release old handle
    release_block( handle );

    // replace handle with the original one    
    new_header->handle  = handle;
    handles[handle]     = new_header;
    handles[new_handle] = 0;


end:

    return status;
}

mem_handle_t mem2_h_get_handle( uint8_t index, mem_type_t8 type ){

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
void _mem2_v_free( mem_handle_t handle, const char* file, int line ){


    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( file, line, 0 );
    }

#else
void mem2_v_free( mem_handle_t handle ){


#endif

    handle = unswizzle(handle);

    if( handle >= 0 ){

        #ifndef ENABLE_EXTENDED_VERIFY
        verify_handle( handle );
        #endif

        release_block( handle );
    }
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
uint16_t _mem2_u16_get_size( mem_handle_t handle, const char* file, int line ){


    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( file, line, 0 );
    }

#else
uint16_t mem2_u16_get_size( mem_handle_t handle ){

#endif

    handle = unswizzle(handle);

    #ifndef ENABLE_EXTENDED_VERIFY
    verify_handle( handle );
    #endif

    // get pointer to the header
    mem_block_header_t *header = handles[handle];

    uint16_t size = header->size - header->padding_len;

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
void *_mem2_vp_get_ptr( mem_handle_t handle, const char* file, int line ){

    // check validity of handle and assert if there is a failure.
    // this overrides the system based assert so we can insert the file and line
    // from the caller.
    if( mem2_b_verify_handle( handle ) == FALSE ){

        assert( file, line, 0 );
    }

#else
void *mem2_vp_get_ptr( mem_handle_t handle ){

#endif

    handle = unswizzle(handle);

    #ifndef ENABLE_EXTENDED_VERIFY
    verify_handle( handle );
    #endif

    uint8_t *ptr = (uint8_t*)handles[handle] + sizeof( mem_block_header_t );

    return ptr;
}

void *mem2_vp_get_ptr_fast( mem_handle_t handle ){

    handle = unswizzle(handle);

    uint8_t *ptr = (uint8_t*)handles[handle] + sizeof( mem_block_header_t );

    return ptr;
}

// check the canaries for all allocated handles.
// this function will assert on any failures.
void mem2_v_check_canaries( void ){

    for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

        // if the handle is allocated
        if( handles[i] != 0 ){

            // get pointer to the header
            mem_block_header_t *header = handles[i];

            uint8_t *canary = CANARY_PTR( header );

            // check the canary
            ASSERT_MSG( *canary == generate_canary( header ), "Invalid canary!" );
            ASSERT_MSG( ( (uint32_t)header & 3 ) == 0, "Header misalign, type: %d addr: 0x%0x", header->type, (uint32_t)header );
            ASSERT_MSG( ( MEM_BLOCK_SIZE( header ) & 3 ) == 0, "Block size invalid" );
        }
    }
}

uint8_t mem2_u8_get_handles_used( void ){

    uint8_t temp = mem_rt_data.handles_used;

    return temp;
}

void mem2_v_get_rt_data( mem_rt_data_t *rt_data ){

    *rt_data = mem_rt_data;
}

// return amount of free memory available
uint16_t mem2_u16_get_free( void ){

    uint16_t temp = mem_rt_data.free_space;

    return temp;
}

// return amount of dirty memory
uint16_t mem2_u16_get_dirty( void ){

    uint16_t temp = mem_rt_data.dirty_space;

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
    
    mem_block_header_t *clean;
    mem_block_header_t *next_block;
    mem_block_header_t *dirty;

    clean = ( mem_block_header_t * )heap;
    next_block = ( mem_block_header_t * )heap;
    dirty = ( mem_block_header_t * )heap;

    // search for a dirty block (loop while clean blocks)
    while( ( is_dirty( dirty ) == FALSE ) &&
            ( dirty < ( mem_block_header_t * )free_space_ptr ) ){

        // ASSERT_MSG( ( (uint32_t)dirty & 3 ) == 0, "Header misalign dirty: %lx", (uint32_t)dirty );

        dirty = (mem_block_header_t *)( (uint8_t *)dirty + MEM_BLOCK_SIZE( dirty ) );
    }

    // clean pointer needs to lead dirty pointer
    if( clean < dirty ){

        clean = dirty;
    }

    while( clean < ( mem_block_header_t * )free_space_ptr ){

        ASSERT_MSG( ( (uint32_t)clean & 3 ) == 0, "Header misalign clean: %lx off by: %lx next_block: %lx type: %lu", 
            (uint32_t)clean,  (uint32_t)clean & 3, (uint32_t)next_block, (uint32_t)clean->type );

        // search for a clean block (loop while dirty)
        // (couldn't find anymore clean blocks to move, so there should be
        // no clean blocks between the dirty pointer and the free pointer)
        while( is_dirty( clean ) == TRUE ){

            clean = (mem_block_header_t *)( (uint8_t *)clean + MEM_BLOCK_SIZE( clean ) );

            // if the free space is reached, the routine is finished
            if( clean >= ( mem_block_header_t * )free_space_ptr ){

                goto done_defrag;
            }

            ASSERT_MSG( ( (uint32_t)clean & 3 ) == 0, "clean misalign: %lx %lu", (uint32_t)clean, MEM_BLOCK_SIZE( clean ) );
            ASSERT_MSG( ( MEM_BLOCK_SIZE( clean ) & 3 ) == 0, "clean bad size: %lx %lu pad: %lu type: %lu handle: %lu hdrsize: %lu canary: %lu", 
                (uint32_t)clean, MEM_BLOCK_SIZE( clean ), (uint32_t)clean->padding_len, (uint32_t)clean->type, (uint32_t)clean->handle, (uint32_t)clean->size, *CANARY_PTR( clean ) );
        }

        // get next block
        next_block = (mem_block_header_t *)( (uint8_t *)clean + MEM_BLOCK_SIZE( clean ) );

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
        // if( next_block < ( mem_block_header_t * )free_space_ptr ){
        //     ASSERT_MSG( ( (uint32_t)next_block & 3 ) == 0, "next_block misalign: %lx clean: %lx size: %lu", (uint32_t)next_block, (uint32_t)clean, MEM_BLOCK_SIZE( clean ) );
        // }

        // copy the clean block to the dirty block pointer
        memcpy( dirty, clean, MEM_BLOCK_SIZE( clean ) );

        // increment dirty pointer
        dirty = (mem_block_header_t *)( (uint8_t *)dirty + MEM_BLOCK_SIZE( dirty ) );

        // if( dirty < ( mem_block_header_t * )free_space_ptr ){
        //     ASSERT_MSG( ( (uint32_t)dirty & 3 ) == 0, "dirty misalign: %lx", (uint32_t)dirty );
        // }

        // assign clean pointer to next block
        clean = next_block;
    }

    // ASSERT_MSG( ( (uint32_t)clean & 3 ) == 0, "Header misalign clean: %lx next_block: %lx", (uint32_t)clean, (uint32_t)next_block );

done_defrag:
    // EVENT( EVENT_ID_MEM_DEFRAG, 1 );

    // there should be no clean blocks between the dirty and free pointers,
    // set the free pointer to the dirty pointer
    free_space_ptr = (uint8_t *)dirty;

    // the dirty space is now free
    mem_rt_data.free_space += mem_rt_data.dirty_space;

    // no more dirty space
    mem_rt_data.dirty_space = 0;

    // run canary check
    mem2_v_check_canaries();
}

