// Stubs for the ffs smoke suite (test_ffs_smoke.c).
//
// Policy: each stub here is the minimum needed to link ffs_page/ffs_block/
// flash25 on the host. They replace OS services that are NOT under test.
// Anything more than ~20 lines should be replaced by compiling the real
// sapphireos file instead (see README.md).

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "fs.h"
#include "logging.h"
#include "threading.h"
#include "system.h"
#include "timers.h"

// --- assertions: surface them, then fail the test process loudly --------

void sos_assert( FLASH_STRING_T str_expr, FLASH_STRING_T file, int line ){

    fprintf( stderr, "ASSERT failed: %s @ %s:%d\n", str_expr, file, line );
    abort();
}

// --- timers: a monotonically advancing fake clock -----------------------

static uint32_t sim_now_ms;

uint32_t tmr_u32_get_system_time_ms( void ){

    return sim_now_ms++;
}

uint32_t tmr_u32_elapsed_time_ms( uint32_t start ){

    return sim_now_ms - start;
}

// --- system: the harness is never in safe mode, the wdt does nothing ----

uint8_t sys_u8_get_mode( void ){

    return SYS_MODE_NORMAL;
}

void sys_v_set_warnings( sys_warnings_t flags ){

    (void)flags;
}

void sys_v_wdt_reset( void ){
}

// --- threading: ffs_block starts a background free-scan thread; the
// harness has no scheduler, so thread creation is a no-op. The smoke
// tests must not depend on the free-scan having run.
thread_t thread_t_create( PT_THREAD( ( *thread )( pt_t *pt, void *state ) ),
                          PGM_P name,
                          void *initial_data,
                          uint16_t size ){

    (void)thread;
    (void)name;
    (void)initial_data;
    (void)size;

    return 0;
}

void thread_v_active( void ){
}

void thread_v_set_alarm( uint32_t alarm ){

    (void)alarm;
}

// --- system error reporting goes nowhere ---------------------------------

void sys_v_set_error( sys_error_t error ){

    (void)error;
}

// --- logging: the real logging.c drags in netmsg/ip/msgflow; route ------
// to stderr instead so log output stays visible in the suite logs
void _log_v_print_P( uint8_t level, PGM_P file, uint16_t line,
                     PGM_P format, ... ){

    static const char *levels[] = { "DBG", "INF", "WRN", "ERR", "CRT" };

    va_list ap;

    (void)file;

    fprintf( stderr, "%s %d: ", levels[level < 5 ? level : 0], line );
    va_start( ap, format );
    vfprintf( stderr, format, ap );
    va_end( ap );
    fputc( '\n', stderr );
}

// --- virtual file system: mem2_v_init registers a "handleinfo" vfile;
// the smoke tests never read it, and pulling in the real fs.c would drag
// in eeprom/keyvalue. M3 uses the real fs.c.
void fs_v_create_virtual( PGM_P filename,
                          uint32_t (*handler)( vfile_op_t8 op, uint32_t pos,
                                               void *ptr, uint32_t len ) ){

    (void)filename;
    (void)handler;
}
