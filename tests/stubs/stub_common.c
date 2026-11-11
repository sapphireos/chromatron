// <license>
//
//     This file is part of the Chromatron project.
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

// Harness stubs for the kv and math suites (see stubs/README.md for the
// audit of each). Compiled with the same sim flags as the SUTs, so the
// signatures below come from the real headers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

// note: no <stdbool.h> - the project's bool.h owns bool/true/false
// (typedef uint8_t bool + TRUE/FALSE macros) and conflicts with it

#include "system.h"
#include "memory.h"
#include "list.h"
#include "fs.h"
#include "threading.h"
#include "pt.h"
#include "timers.h"
#include "logging.h"
#include "keyvalue.h"
#include "config.h"
#include "gfx_lib.h"

// ---------------------------------------------------------------- mem2
// Fixed block pool: the real allocator owns firmware memory; list.c (and
// only list.c in these closures) needs alloc/free/ptr/size/verify.
// handle 0 is the null node, -1 is "no neighbour" - keep the pool small
// enough that handles fit int8_t (mem_handle_t on the sim target)
#define MEM2_SLOTS    120
#define MEM2_SLOT_SZ  256

static uint8_t mem2_pool[MEM2_SLOTS][MEM2_SLOT_SZ];
static bool mem2_used[MEM2_SLOTS];
static uint16_t mem2_size[MEM2_SLOTS]; // allocated (useful) size per slot

static int mem2_slot_from_handle( mem_handle_t h ){

    // handles are 1-based slots (0 is the null handle)
    if( h < 1 || h > MEM2_SLOTS ){

        return -1;
    }

    return h - 1;
}

mem_handle_t mem2_h_alloc2( uint16_t size, mem_type_t8 type ){

    (void)type;

    if( size > MEM2_SLOT_SZ ){

        return -1;
    }

    for( int i = 0; i < MEM2_SLOTS; i++ ){

        if( !mem2_used[i] ){

            mem2_used[i] = true;
            mem2_size[i] = size;
            memset( mem2_pool[i], 0, MEM2_SLOT_SZ );
            return ( mem_handle_t )i + 1;
        }
    }

    return -1;
}

// extended-verify variants (list.h/memory.h macros expand to these when
// INCLUDE_ASSERTS is set, as on the sim target)
void *_mem2_vp_get_ptr( mem_handle_t h, FLASH_STRING_T file, int line ){

    (void)file;
    (void)line;

    int slot = mem2_slot_from_handle( h );

    if( slot < 0 ){

        return 0;
    }

    return mem2_pool[slot];
}

void _mem2_v_free( mem_handle_t h, FLASH_STRING_T file, int line ){

    (void)file;
    (void)line;

    int slot = mem2_slot_from_handle( h );

    if( slot >= 0 ){

        mem2_used[slot] = false;
    }
}

uint16_t _mem2_u16_get_size( mem_handle_t h, FLASH_STRING_T file, int line ){

    (void)file;
    (void)line;

    int slot = mem2_slot_from_handle( h );

    // list_u16_node_size subtracts the node-state header from this, so
    // it must be the allocated (useful) size, not the slot capacity
    return slot < 0 ? 0 : mem2_size[slot];
}

void *mem2_vp_get_ptr_fast( mem_handle_t h ){

    return _mem2_vp_get_ptr( h, 0, 0 );
}

bool mem2_b_verify_handle( mem_handle_t h ){

    return mem2_slot_from_handle( h ) >= 0 && mem2_used[mem2_slot_from_handle(h)];
}

// ---------------------------------------------------------------- fs
// FILE* over the suite's working directory. This is what lets keyvalue.c
// exercise its real persist file format (header + fixed-size blocks).
// The real fs.c comes in M3.
//
// One deliberate deviation from the flash fs: FS_MODE_WRITE_OVERWRITE is
// cached in memory and only flushed on close (stub_fs_v_flush_all). The
// firmware persist file is rewritten in place by the persist thread
// (seek + write, no truncate), and the keyvalue layer re-opens it with
// WRITE_OVERWRITE on every init - a real truncate would let a stale
// persist thread from an earlier test wipe the file out from under the
// current one.
#define FS_MAX_OPEN   8

static struct{

    FILE *f;
    char name[64];
    bool used;
    bool append;
    bool cached;   // WRITE_OVERWRITE: memory copy until close/flush
    int64_t cache_len;
    uint8_t *cache;

} fs_table[FS_MAX_OPEN];

// one open per filename: the SUTs keep handles across re-inits
// (kv_data_file_handle is never closed), so the table must not grow
// with the number of inits
//
// glibc quirk (observed on 2.39): after fseek, fread on a stream
// opened "ab" (O_APPEND) returns 0. So append is emulated: open
// read/write and seek to the end before each write - exactly the
// flash-fs append semantics (writes always land at the end).
static void fs_slot_reset( int slot, const char *filename, mode_t8 mode ){

    FILE *f = NULL;

    if( ( mode & FS_MODE_WRITE_APPEND ) || ( mode & FS_MODE_WRITE_OVERWRITE ) ){

        f = fopen( filename, "rb+" );

        if( f == NULL ){

            f = fopen( filename, "wb+" );
        }
    }
    else{

        f = fopen( filename, "rb" );
    }

    if( f == NULL ){

        fs_table[slot].used = false;

        return;
    }

    free( fs_table[slot].cache );

    fs_table[slot].f = f;
    fs_table[slot].used = true;
    fs_table[slot].append = ( mode & FS_MODE_WRITE_APPEND ) != 0;
    fs_table[slot].cache = NULL;
    fs_table[slot].cached = false;
    strncpy( fs_table[slot].name, filename, sizeof(fs_table[slot].name) );
    fs_table[slot].name[sizeof(fs_table[slot].name) - 1] = 0;

    // set up the no-truncate cache for in-place rewrites
    if( mode & FS_MODE_WRITE_OVERWRITE ){

        fseek( f, 0, SEEK_END );
        int64_t len = ftell( f );
        fseek( f, 0, SEEK_SET );

        fs_table[slot].cache = malloc( len ? len : 1 );
        fs_table[slot].cache_len = len;

        if( fs_table[slot].cache && len > 0 ){

            fread( fs_table[slot].cache, 1, len, f );
        }

        fs_table[slot].cached = fs_table[slot].cache != NULL;
    }
}

file_t fs_f_open_P( PGM_P filename, mode_t8 mode ){

    // same filename already open: reuse the slot (flash fs semantics:
    // the file persists, only the handle changes)
    for( int i = 0; i < FS_MAX_OPEN; i++ ){

        if( fs_table[i].used && strcmp( fs_table[i].name, filename ) == 0 ){

            fs_slot_reset( i, filename, mode );

            return fs_table[i].used ? i + 1 : -1;
        }
    }

    int slot = -1;

    for( int i = 0; i < FS_MAX_OPEN; i++ ){

        if( !fs_table[i].used ){

            slot = i;
            break;
        }
    }

    if( slot < 0 ){

        return -1;
    }

    fs_slot_reset( slot, filename, mode );

    return fs_table[slot].used ? slot + 1 : -1;
}

static FILE *fs_file_from_handle( file_t file ){

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[file - 1].used ){

        return NULL;
    }

    return fs_table[file - 1].f;
}

static void fs_slot_write_through( int slot, const void *src, int64_t pos, size_t len ){

    if( fs_table[slot].cached ){

        if( pos + ( int64_t )len > fs_table[slot].cache_len ){

            fs_table[slot].cache = realloc( fs_table[slot].cache, pos + len );
            memset( fs_table[slot].cache + fs_table[slot].cache_len, 0,
                    pos + len - fs_table[slot].cache_len );
            fs_table[slot].cache_len = pos + len;
        }

        memcpy( fs_table[slot].cache + pos, src, len );
    }
    else{

        fwrite( src, 1, len, fs_table[slot].f );
    }
}

static void fs_slot_flush( int slot ){

    if( fs_table[slot].cached ){

        FILE *f = fs_table[slot].f;

        fseek( f, 0, SEEK_SET );
        fwrite( fs_table[slot].cache, 1, ( size_t )fs_table[slot].cache_len, f );
        fflush( f );
        free( fs_table[slot].cache );
        fs_table[slot].cache = NULL;
        fs_table[slot].cached = false;
    }
    else if( fs_table[slot].f ){

        fflush( fs_table[slot].f );
    }
}

// flush all cached (WRITE_OVERWRITE) files to disk
void stub_fs_v_flush_all( void ){

    for( int i = 0; i < FS_MAX_OPEN; i++ ){

        if( fs_table[i].used ){

            fs_slot_flush( i );
        }
    }
}

// cached writes in flight (the persist thread writes from its own
// pthread); the harness bumps this around each cached write
static volatile int fs_dirty = 0;

void stub_fs_v_wait_idle( void ){

    for( int i = 0; i < 5000 && fs_dirty; i++ ){

        usleep( 1000 );
    }
}

file_t fs_f_close( file_t file ){

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[file - 1].used ){

        return -1;
    }

    fs_slot_flush( file - 1 );

    if( fs_table[file - 1].f ){

        fclose( fs_table[file - 1].f );
    }

    fs_table[file - 1].used = false;

    return -1;
}

int16_t fs_i16_read( file_t file, void *dst, uint16_t len ){

    int slot = file - 1;

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[slot].used ){

        return -1;
    }

    if( fs_table[slot].cached ){

        int64_t pos = ftell( fs_table[slot].f );

        if( pos < 0 || pos >= fs_table[slot].cache_len ){

            return 0;
        }

        int64_t n = len;

        if( pos + n > fs_table[slot].cache_len ){

            n = fs_table[slot].cache_len - pos;
        }

        if( n > 0 ){

            memcpy( dst, fs_table[slot].cache + pos, n );
            fseek( fs_table[slot].f, pos + n, SEEK_SET );
        }

        return ( int16_t )n;
    }

    return ( int16_t )fread( dst, 1, len, fs_table[slot].f );
}

int16_t fs_i16_write( file_t file, const void *src, uint16_t len ){

    int slot = file - 1;

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[slot].used ){

        return -1;
    }

    if( fs_table[slot].cached ){

        int64_t pos = ftell( fs_table[slot].f );

        __sync_add_and_fetch( &fs_dirty, 1 );
        fs_slot_write_through( slot, src, pos, len );
        fseek( fs_table[slot].f, pos + len, SEEK_SET );
        __sync_sub_and_fetch( &fs_dirty, 1 );
    }
    else{

        // emulated append: writes always land at the end
        if( fs_table[slot].append ){

            fseek( fs_table[slot].f, 0, SEEK_END );
        }

        fwrite( src, 1, len, fs_table[slot].f );
    }

    return len;
}

int32_t fs_i32_get_size( file_t file ){

    int slot = file - 1;

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[slot].used ){

        return -1;
    }

    // logical size: on-disk position, extended by the rewrite cache
    int32_t size = ( int32_t )ftell( fs_table[slot].f );

    if( fs_table[slot].cached && fs_table[slot].cache_len > size ){

        size = ( int32_t )fs_table[slot].cache_len;
    }

    return size;
}

int32_t fs_i32_get_size_fname( char filename[] ){

    // size on disk (flush first so cached files are visible)
    stub_fs_v_flush_all();

    FILE *f = fopen( filename, "rb" );

    if( f == NULL ){

        return -1;
    }

    fseek( f, 0, SEEK_END );
    int32_t sz = ftell( f );
    fclose( f );

    return sz;
}

int32_t fs_i32_tell( file_t file ){

    FILE *f = fs_file_from_handle( file );

    return f ? ftell( f ) : -1;
}

void fs_v_seek( file_t file, uint32_t pos ){

    FILE *f = fs_file_from_handle( file );

    if( f ){

        fseek( f, pos, SEEK_SET );
    }
}

void fs_v_delete( file_t file ){

    if( file < 1 || file > FS_MAX_OPEN || !fs_table[file - 1].used ){

        return;
    }

    fs_slot_flush( file - 1 );

    if( fs_table[file - 1].f ){

        fclose( fs_table[file - 1].f );
    }

    remove( fs_table[file - 1].name );
    fs_table[file - 1].used = false;
}

void fs_v_create_virtual( PGM_P filename,
                          uint32_t (*ops)( vfile_op_t8, uint32_t, void *, uint32_t ) ){

    (void)filename;
    (void)ops;

    // no virtual files in the harness (real fs.c comes in M3)
}

// ---------------------------------------------------------------- system
uint8_t sys_u8_get_mode( void ){

    return SYS_MODE_NORMAL;
}

uint32_t sys_u32_get_kv_index_addr( void ){

    // unused on the sim path (ram_index is built from the .kv_meta section)
    return 0;
}

uint32_t sys_u32_get_fw_length( void ){

    return 0;
}

void sys_v_set_warnings( uint32_t warnings ){

    (void)warnings;
}

uint8_t sys_u8_get_loader_status( void ){

    return 0;
}

// ---------------------------------------------------------------- threading
// The real scheduler is a cooperative protothread loop driven by os_irq;
// the one thing these suites need is for a PT_THREAD to actually run,
// because keyvalue.c's persist path runs in its persist_thread PT. This
// stub runs each PT in a pthread: PT_INIT, call, sleep on PT_WAITING,
// and set PT_YIELD_FLAG before re-entry (mirroring the scheduler's
// PT_INIT-before-dispatch - see threading.c thread_v_restart).
//
// Creation is idempotent per PT function: the SUTs re-init keyvalue in
// every test, which would otherwise spawn one live persist thread per
// test, all racing on the persist file. The firmware runs exactly one.
static volatile int thread_count = 0;
static volatile uint8_t signal_flags = 0;
static void *active_pt_fn = 0;
static bool active_pt_running = false;

typedef char ( *pt_fn_t )( pt_t *pt, void *state );

typedef struct{

    pt_fn_t fn;
    pt_t pt;

} pt_thread_t;

static void *pt_thread_run( void *arg ){

    pt_thread_t *t = arg;

    // one PT_INIT: lc-switch resume jumps back into the PT at its last
    // LC_SET; re-initializing here would restart it from the top
    PT_INIT( &t->pt );

    for(;;){

        int status = t->fn( &t->pt, &t->pt );

        if( status == PT_EXITED || status == PT_ENDED ){

            break;
        }

        // WAITING blocks on a condition; SLEEPING/YIELDED fake-timer
        // waits - give the wall clock a little room either way
        usleep( 1000 );
    }

    if( active_pt_fn == ( void * )t->fn ){

        active_pt_running = false;
        active_pt_fn = 0;
    }

    free( t );
    __sync_sub_and_fetch( &thread_count, 1 );

    return NULL;
}

thread_t thread_t_create( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                          PGM_P name,
                          void *initial_data,
                          uint16_t size ){

    (void)name;
    (void)initial_data;

    // already scheduled (PT threads use statics - one instance at a time)
    if( active_pt_running && active_pt_fn == ( void * )thread ){

        return 1;
    }

    pt_thread_t *t = malloc( sizeof(pt_thread_t) + size );

    if( t == NULL ){

        return 0;
    }

    t->fn = thread;

    __sync_add_and_fetch( &thread_count, 1 );
    active_pt_fn = ( void * )thread;
    active_pt_running = true;

    pthread_t th;

    if( pthread_create( &th, NULL, pt_thread_run, t ) != 0 ){

        free( t );
        __sync_sub_and_fetch( &thread_count, 1 );
        active_pt_running = false;
        active_pt_fn = 0;

        return 0;
    }

    pthread_detach( th );

    return 1;
}

void thread_v_active( void ){

    // no-op: no scheduler to wake
}

void thread_v_set_alarm( uint32_t alarm ){

    (void)alarm;

    // no scheduler: alarms are a no-op (TMR_WAIT falls back on THREAD_SLEEP)
}

bool thread_b_signalled( uint8_t signum ){

    return ( signal_flags & ( 1 << signum ) ) != 0;
}

void thread_v_signal( uint8_t signum ){

    signal_flags |= ( 1 << signum );
}

void thread_v_clear_signal( uint8_t signum ){

    signal_flags &= ~( 1 << signum );
}

void thread_v_set_signal_flag( void ){
}

void thread_v_clear_signal_flag( void ){
}

uint16_t thread_u16_get_signals( void ){

    return signal_flags;
}

// ---------------------------------------------------------------- timers
// Fake monotonic clock. Starts at 1970-01-01 in NTP seconds so that
// datetime tests see a coherent (if rapidly moving) wall clock.
#define NTP_1970_SECONDS  2208988800U

static uint32_t fake_time = NTP_1970_SECONDS;

uint32_t tmr_u32_get_system_time_ms( void ){

    return ++fake_time * 1000;
}

uint32_t tmr_u32_get_system_time_us( void ){

    return ++fake_time * 1000000;
}

uint32_t tmr_u32_elapsed_time_ms( uint32_t start_time ){

    return tmr_u32_get_system_time_ms() - start_time;
}

uint32_t tmr_u32_elapsed_time_us( uint32_t start_time ){

    return tmr_u32_get_system_time_us() - start_time;
}

uint32_t tmr_u32_elapsed_times( uint32_t start_time, uint32_t end_time ){

    return end_time - start_time;
}

int8_t tmr_i8_compare_time( uint32_t time ){

    uint32_t now = tmr_u32_get_system_time_us();

    return ( int8_t )( now < time ? -1 : ( now > time ? 1 : 0 ) );
}

int8_t tmr_i8_compare_times( uint32_t time1, uint32_t time2 ){

    return ( int8_t )( time1 < time2 ? -1 : ( time1 > time2 ? 1 : 0 ) );
}

// ---------------------------------------------------------------- misc
void sos_assert( FLASH_STRING_T str_expr, FLASH_STRING_T file, int line ){

    fprintf( stderr, "assert failed: %s (%s:%d)\n", str_expr ? str_expr : "?",
             file ? file : "?", line );
    abort();
}

void _log_v_print_P( uint8_t level, PGM_P file, uint16_t line,
                     PGM_P format, ... ){

    (void)level;
    (void)file;
    (void)line;
    (void)format;
}

char *itoa( long i, char *s, int dummy_radix ){

    (void)dummy_radix; // only base 10 is used by the SUTs

    // the firmware itoa (datetime.c's ISO formatting relies on it, no
    // zero padding - callers pad themselves)
    char *p = s;
    char *end = p;

    if( i == 0 ){

        *end++ = '0';
    }
    else{

        unsigned long v = i < 0 ? ( unsigned long )-i : ( unsigned long )i;

        if( i < 0 ){

            *end++ = '-';
        }

        char tmp[16];
        int len = 0;

        while( v > 0 ){

            tmp[len++] = ( char )('0' + ( v % 10 ));
            v /= 10;
        }

        while( len > 0 ){

            *end++ = tmp[--len];
        }
    }

    *end = 0;

    return s;
}

// ---------------------------------------------------------------- math suite extras
// gfx_lib.c's frame thread never renders in the harness
void sim_sdl_v_frame( void ){
}

// random.c
int8_t cfg_i8_get( catbus_hash_t32 parameter, void *value ){

    (void)parameter;
    (void)value;

    return -1;
}

// gfx_lib.c's frame-rate hint goes to the VM (lib_chromatron/vm.c),
// which is not in these closures
void gfx_vm_v_update_frame_rate( uint16_t new_frame_rate ){

    (void)new_frame_rate;
}
