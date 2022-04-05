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

#include "cpu.h"
#include "threading.h"
#include "system.h"
#include "fs.h"
#include "keyvalue.h"
#include "timers.h"

#include "flash25.h"
#include "ffs_block.h"
#include "ffs_page.h"
#include "ffs_gc.h"

#define NO_EVENT_LOGGING
#include "event_log.h"

static const PROGMEM char gc_data_fname[] = "gc_data";

static uint16_t gc_passes;
static bool suspend_gc;

KV_SECTION_META kv_meta_t ffs_gc_info_kv[] = {
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &gc_passes,        0,  "flash_fs_gc_passes" },
};

/*

GC mods:

Make clean block a function that we can call from the file system, if we immediately need a clean block.

GC meta data goes into a static FIFO data structure and is periodically flushed to the gc_info file
after the periodic block clean.  This way we minimize additional writes, and we can safely call the block clean
without it hitting the file system.

*/

PT_THREAD( garbage_collector_thread( pt_t *pt, void *state ) );

void ffs_gc_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( garbage_collector_thread,
                     PSTR("ffs_garbage_collector"),
                     0,
                     0 );
}

void ffs_gc_v_suspend_gc( bool suspend ){

    suspend_gc = suspend;
}

PT_THREAD( garbage_collector_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // set up gc file
    file_t gc_file = fs_f_open_P( gc_data_fname, FS_MODE_WRITE_OVERWRITE | FS_MODE_CREATE_IF_NOT_FOUND );

    if( gc_file >= 0 ){

        // check file size
        if( fs_i32_get_size( gc_file ) == 0 ){

            uint32_t count = 0;

            for( uint16_t i = 0; i < ffs_block_u16_total_blocks(); i++ ){

                fs_i16_write( gc_file, &count, sizeof(count) );
            }
        }

        gc_file = fs_f_close( gc_file );
    }
    else{

        ffs_block_v_hard_error();

        THREAD_EXIT( pt );
    }


    while(1){

        THREAD_WAIT_WHILE( pt, ffs_block_u16_dirty_blocks() < FFS_DIRTY_THRESHOLD );

        THREAD_WAIT_WHILE( pt, suspend_gc );

        gc_passes++;

        while( ffs_block_u16_dirty_blocks() > 0 ){

            EVENT( EVENT_ID_FFS_GARBAGE_COLLECT, ffs_block_u16_dirty_blocks() );

            // get a dirty block
            block_t block = ffs_block_fb_get_dirty();
            ASSERT( block != FFS_BLOCK_INVALID );

            // erase block
            // trace_printf("erase block: %d\r\n", block);
            ffs_block_i8_erase( block );

            // open data file
            file_t f = fs_f_open_P( gc_data_fname, FS_MODE_WRITE_OVERWRITE );

            if( f < 0 ){

                THREAD_YIELD( pt );

                continue;
            }

            // read erase count
            uint32_t erase_count;
            fs_v_seek( f, sizeof(erase_count) * block );
            fs_i16_read( f, &erase_count, sizeof(erase_count) );

            fs_v_seek( f, sizeof(erase_count) * block );
            erase_count++;
            fs_i16_write( f, &erase_count, sizeof(erase_count) );

            f = fs_f_close( f );

            TMR_WAIT( pt, 50 );
        }

        EVENT( EVENT_ID_FFS_GARBAGE_COLLECT, 1 );



        // check wear leveler threshold
        if( ( gc_passes % FFS_WEAR_CHECK_THRESHOLD ) == 0 ){

            // run wear leveler algorithm

            // allocate memory to hold the gc data
            mem_handle_t gc_data_h = mem2_h_alloc( sizeof(uint32_t) * ffs_block_u16_total_blocks() );

            if( gc_data_h < 0 ){

                continue;
            }

            // open the gc data file
            file_t f = fs_f_open_P( gc_data_fname, FS_MODE_READ_ONLY );

            if( f < 0 ){

                goto wear_done;
            }

            uint32_t *counts = mem2_vp_get_ptr( gc_data_h );

            // load list
            fs_i16_read( f, counts, mem2_u16_get_size( gc_data_h ) );

            // close file
            f = fs_f_close( f );

            uint32_t highest_erase_count = 0;
            uint32_t lowest_erase_count = 0xffffffff;
            uint16_t lowest_block = 0;

            // find blocks with the highest erase count and lowest erase count
            for( uint16_t i = 0; i < ffs_block_u16_total_blocks(); i++ ){

                if( counts[i] > highest_erase_count ){

                    highest_erase_count = counts[i];
                }

                if( counts[i] < lowest_erase_count ){

                    lowest_erase_count = counts[i];
                    lowest_block = i;
                }
            }

            // check difference between lowest and highest
            if( ( highest_erase_count - lowest_erase_count ) > FFS_WEAR_THRESHOLD ){

                // check if the block is in the dirty or free lists
                if( ffs_block_b_is_block_free( lowest_block ) || ffs_block_b_is_block_dirty( lowest_block ) ){

                    // this block is eligible for wear leveling, but since it isn't a file block,
                    // there is no need to do anything with it.

                    goto wear_done;
                }

                // get meta data for lowest block
                ffs_block_meta_t meta;

                if( ffs_block_i8_read_meta( lowest_block, &meta ) < 0 ){

                    goto wear_done;
                }

                trace_printf("WEAR LEVEL\r\n");

                // replace block
                ffs_page_i16_replace_block( meta.file_id, meta.block );

                EVENT( EVENT_ID_FFS_WEAR_LEVEL, 0 );
            }
wear_done:

            // release memory
            mem2_v_free( gc_data_h );
        }
    }

PT_END( pt );
}
