// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published
//     by the Free Software Foundation, either version 3 of the License,
//     or (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program. If not, see <http://www.gnu.org/licenses/>.
// 
// </license>

#include "sapphire.h"
#include "init.h"
#include "threading.h"
#include "timers.h"
#include "catbus.h"
#include "keyvalue.h"
#include "pix_modes.h"
#include "vm4.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>

#include "fs.h"

#include "hal_sim.h"
#include "hal_sim_sdl.h"

/*

Simulator platform support.

The simulator has no device to serve, so the process ends after a run
time.  The run time, in seconds, is the first positional command-line
argument (eg. main.elf 10); if it is blank, the process runs until
killed.  See --help for the full command line.

The simulator's persistent state is a set of .bin files in the current
directory, created by the HALs on first use (sim_flash.bin,
sim_eeprom.bin, sim_bootdata.bin); --clean removes them so the next
boot starts from erased flash and default state, like a fresh device.

*/

void app_v_init( void ) __attribute__((weak));
void libs_v_init( void ) __attribute__((weak));

// the simulator state files, matching the names in hal_flash25.c,
// hal_eeprom.c and boot_data.c
static const char *sim_state_files[] = {
    "sim_flash.bin",
    "sim_eeprom.bin",
    "sim_bootdata.bin"
};

static uint32_t sim_run_time_ms = 0; // 0 = run until killed

// initial pixel and VM4 configuration for the simulator. on real hardware
// this is exactly what led_detect's load_profile() does after detecting the
// attached LED board (battery detection is disabled on the sim, so that
// never runs): it preconfigures the pixel array through the KV system and
// selects a VM program.
//
// the rainbow demo is the copy embedded in the firmware image: vm4_v_run_prog
// points vm4_prog at the _rainbow.f4b virtual file, so no upload is needed.
//
// seeding only runs when the pixel config is empty (erased or --cleaned
// eeprom): KV changes made at runtime persist in sim_eeprom.bin and survive
// simulator restarts; run with --clean to restore these defaults.
static void sim_v_setup( void ){

    uint16_t pix_count = 0;
    kv_i8_get( __KV__pix_count, &pix_count, sizeof(pix_count) );

    if( pix_count != 0 ){

        // already configured by a previous boot
        return;
    }

    uint16_t u16;
    uint8_t u8;
    bool run = TRUE;

    // 16 x 16 pixel grid
    u16 = 256;
    catbus_i8_set( __KV__pix_count,  CATBUS_TYPE_UINT16, &u16, sizeof(u16) );

    u16 = 16;
    catbus_i8_set( __KV__pix_size_x, CATBUS_TYPE_UINT16, &u16, sizeof(u16) );
    catbus_i8_set( __KV__pix_size_y, CATBUS_TYPE_UINT16, &u16, sizeof(u16) );

    // SK6812 RGB: speaks the WS2811/WS2812 protocol, so use mode 3
    u8 = PIX_MODE_WS2811;
    catbus_i8_set( __KV__pix_mode, CATBUS_TYPE_UINT8, &u8, sizeof(u8) );

    u8 = 0;
    catbus_i8_set( __KV__pix_rgb_order, CATBUS_TYPE_UINT8, &u8, sizeof(u8) );

    // full brightness: master and sub dimmers wide open
    u16 = 65535;
    catbus_i8_set( __KV__gfx_master_dimmer, CATBUS_TYPE_UINT16, &u16, sizeof(u16) );
    catbus_i8_set( __KV__gfx_sub_dimmer, CATBUS_TYPE_UINT16, &u16, sizeof(u16) );

    // preload the built-in rainbow program: sets vm4_prog and raises
    // VM0's reset flag; the vm4_loader thread starts it
    char prog[FFS_FILENAME_LEN] = "_rainbow.f4b";
    vm4_v_run_prog( prog, 0 );

    // persist run so the program also autostarts on later boots
    // (vm4_v_run_prog only sets the in-memory flag)
    catbus_i8_set( __KV__vm4_run, CATBUS_TYPE_BOOL, &run, sizeof(run) );

    printf( "sim: configured 16x16 pixel grid, preloaded VM4 program: _rainbow.f4b\n" );
}

// --prog: a .f4b bytecode file on the host runs in VM0 as a virtual file
// backed by the host file system, so no FFS space is consumed and the
// state files stay untouched.  Same shape as the _rainbow.f4b handler in
// graphics.c: the FS driver bounds-checks pos/len before calling us.
#define SIM_PROG_FNAME "_host.f4b"

static char sim_prog_path[ PATH_MAX ];
static uint32_t sim_prog_size;

static uint32_t sim_prog_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    uint32_t ret_val = len;

    switch( op ){
        case FS_VFILE_OP_READ: {

            FILE *f = fopen( sim_prog_path, "rb" );

            if( f == 0 ){

                return 0;
            }

            fseek( f, pos, SEEK_SET );
            ret_val = fread( ptr, 1, len, f );
            fclose( f );
            break;
        }

        case FS_VFILE_OP_SIZE:
            ret_val = sim_prog_size;
            break;

        default:
            ret_val = 0;
            break;
    }

    return ret_val;
}

// --prog <file.fx4|file.f4b>: run the program in VM0.  .fx4 sources are
// compiled first with the host fx4 CLI, which writes <file>.f4b next to
// the source (the same convention sapphiremake uses).
static void sim_v_run_prog_file( void ){

    const char *path = getenv( "SIM_PROG" );

    if( path == 0 ){

        // a --prog run leaves vm4_prog pointing at the virtual file:
        // with no --prog on this boot, the handler would not exist and
        // the loader would fail on a missing file, so restore the
        // built-in demo program
        char prog[ FFS_FILENAME_LEN ];

        if( ( kv_i8_get( __KV__vm4_prog, prog, sizeof(prog) ) == 0 ) &&
            ( strcmp( prog, SIM_PROG_FNAME ) == 0 ) ){

            char name[ FFS_FILENAME_LEN ];
            memset( name, 0, sizeof(name) );
            strcpy( name, "_rainbow.f4b" );
            vm4_v_run_prog( name, 0 );

            printf( "sim: no --prog: restored default VM4 program: _rainbow.f4b\n" );
        }

        return;
    }

    FILE *f = fopen( path, "rb" );

    if( f == 0 ){

        // parse checked existence, so this means the file vanished
        fprintf( stderr, "sim: cannot open program: %s\n", path );
        exit( 1 );
    }

    fseek( f, 0, SEEK_END );
    sim_prog_size = ftell( f );
    fclose( f );

    if( sim_prog_size == 0 ){

        fprintf( stderr, "sim: program file is empty: %s\n", path );
        exit( 1 );
    }

    snprintf( sim_prog_path, sizeof(sim_prog_path), "%s", path );

    fs_v_create_virtual( PSTR(SIM_PROG_FNAME), sim_prog_vfile_handler );

    char name[ FFS_FILENAME_LEN ];
    memset( name, 0, sizeof(name) );
    strcpy( name, SIM_PROG_FNAME );
    vm4_v_run_prog( name, 0 );

    printf( "sim: running VM4 program: %s (%d bytes)\n", path, (int)sim_prog_size );
}

// --pix-count / --pix-size-x / --pix-size-y: override the pixel array
// configuration.  These write the same KVs as sim_v_setup's seeding, so
// the values flow through the normal KV bindings (pixel.c reconfigure,
// gfx_lib geometry) and persist like any KV change; --clean resets them.
static void sim_v_apply_pix_args( void ){

    static const struct {
        const char *env;
        catbus_hash_t32 hash;
    } pix_args[] = {
        { "SIM_PIX_COUNT", __KV__pix_count  },
        { "SIM_PIX_X",     __KV__pix_size_x },
        { "SIM_PIX_Y",     __KV__pix_size_y },
    };

    for( unsigned i = 0; i < cnt_of_array(pix_args); i++ ){

        const char *str = getenv( pix_args[i].env );

        if( str == 0 ){

            continue;
        }

        uint16_t value = (uint16_t)atoi( str );

        catbus_i8_set( pix_args[i].hash, CATBUS_TYPE_UINT16, &value, sizeof(value) );
    }
}

PT_THREAD( sim_terminate_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    TMR_WAIT( pt, sim_run_time_ms );

    exit( 0 );

PT_END( pt );
}

void hal_sim_v_init( void ){

    if( sim_run_time_ms != 0 ){

        thread_t_create( THREAD_CAST(sim_terminate_thread), PSTR("sim_exit"), 0, 0 );
    }
}

// sockets an idle wait can watch: at least the size of the simulator
// socket table (SIM_WIFI_MAX_PORTS in hal_wifi.c)
#define SIM_IDLE_MAX_FDS 16

void hal_sim_v_idle( uint32_t ms ){

    struct pollfd pfds[SIM_IDLE_MAX_FDS];

    nfds_t nfds = (nfds_t)hal_wifi_i8_poll_fds( pfds, SIM_IDLE_MAX_FDS );

    if( ms > 1000 ){

        ms = 1000; // poll() takes a signed ms timeout
    }

    // zero descriptors is valid: poll() then simply waits for the
    // timeout.  The wait length is re-derived from the scheduler's
    // absolute alarms on every call (see cpu_v_sleep), so the relative
    // timeout here does not accumulate drift.  Waking on data, on the
    // timeout or on a signal all mean the same thing: the scheduler
    // re-checks its threads.
    poll( pfds, nfds, (int)ms );

    // keep the SDL window responsive while the simulator waits
    sim_sdl_v_pump();
}

static void _v_print_usage( const char *prog ){

    printf( "usage: %s [run_time] [options]\n\n", prog );
    printf( "  run_time  (optional) seconds to run before the process exits;\n"
            "            blank or omitted runs until killed ($SIM_RUN_TIME\n"
            "            works too)\n" );
    printf( "  --sdl     draw the pixel array in an SDL window on the host\n"
            "            screen\n" );
    printf( "  --prog    .fx4 or .f4b program file to load and run in VM0;\n"
            "            .fx4 sources are compiled first with the host fx4\n"
            "            tool, overriding the program set by a previous run\n" );
    printf( "  --pix-count N, --pix-size-x N, --pix-size-y N\n"
            "            override the pixel array configuration (defaults\n"
            "            from a fresh --clean state: 256, 16, 16); values\n"
            "            persist in the simulator eeprom like any KV\n" );
    printf( "  --clean   remove the simulator state files (sim_flash.bin,\n"
            "            sim_eeprom.bin, sim_bootdata.bin) so the next boot\n"
            "            starts from a clean slate, like a fresh device\n" );
    printf( "  --help    show this help and exit\n" );
}

// fetch the value that follows a flag on the command line
static const char *_vp_next_arg( int argc, char **argv, int *i, const char *flag ){

    if( ( *i + 1 ) >= argc ){

        fprintf( stderr, "sim: %s requires a value\n", flag );
        exit( 1 );
    }

    ( *i )++;

    return argv[ *i ];
}

static uint16_t _u16_arg( const char *str, const char *flag ){

    char *end;
    long value = strtol( str, &end, 10 );

    if( ( *end != 0 ) || ( value < 0 ) || ( value > UINT16_MAX ) ){

        fprintf( stderr, "sim: %s: bad value: %s\n", flag, str );
        exit( 1 );
    }

    return (uint16_t)value;
}

static void _v_clean_state_files( void ){

    for( unsigned i = 0; i < ( sizeof(sim_state_files) / sizeof(sim_state_files[0]) ); i++ ){

        if( unlink( sim_state_files[i] ) == 0 ){

            printf( "sim: removed %s\n", sim_state_files[i] );
        }
    }
}

int main( int argc, char **argv ){

    char *str = 0;

    for( int i = 1; i < argc; i++ ){

        if( strcmp( argv[i], "--help" ) == 0 || strcmp( argv[i], "-h" ) == 0 ){

            _v_print_usage( argv[0] );
            return 0;
        }

        if( strcmp( argv[i], "--sdl" ) == 0 ){

            // draw the pixel array in an SDL window on the host screen.
            // like the run time above, a simulated reboot re-execs
            // without the command line, so carry the flag in the
            // environment, which survives execve
            setenv( "SIM_SDL", "1", 1 );
            sim_sdl_v_enable();
            fflush( stdout );
            continue;
        }

        if( strcmp( argv[i], "--prog" ) == 0 ){

            // run a host program file in VM0 (see sim_v_run_prog_file);
            // carry the path in the environment across a simulated reboot
            const char *value = _vp_next_arg( argc, argv, &i, "--prog" );

            char path[ PATH_MAX ];

            if( realpath( value, path ) == 0 ){

                fprintf( stderr, "sim: --prog: cannot open: %s\n", value );
                return 1;
            }

            char *dot = strrchr( path, '.' );

            if( ( dot == 0 ) ||
                ( ( strcmp( dot, ".f4b" ) != 0 ) && ( strcmp( dot, ".fx4" ) != 0 ) ) ){

                fprintf( stderr, "sim: --prog: not a .fx4 or .f4b file: %s\n", value );
                return 1;
            }

            if( strcmp( dot, ".fx4" ) == 0 ){

                // compile in place: fx4 writes <file>.f4b next to the
                // source, the same convention sapphiremake uses
                char cmd[ PATH_MAX + 8 ];
                snprintf( cmd, sizeof(cmd), "fx4 '%s'", path );

                if( system( cmd ) != 0 ){

                    fprintf( stderr, "sim: --prog: fx4 compile failed: %s\n", path );
                    return 1;
                }

                strcpy( dot, ".f4b" );

                if( access( path, R_OK ) != 0 ){

                    fprintf( stderr, "sim: --prog: fx4 produced no output: %s\n", path );
                    return 1;
                }
            }

            setenv( "SIM_PROG", path, 1 );
            continue;
        }

        static const struct {
            const char *flag;
            const char *env;
        } pix_flags[] = {
            { "--pix-count",   "SIM_PIX_COUNT" },
            { "--pix-size-x",  "SIM_PIX_X"     },
            { "--pix-size-y",  "SIM_PIX_Y"     },
        };

        {
            bool pix_matched = FALSE;

            for( unsigned p = 0; p < cnt_of_array(pix_flags); p++ ){

                if( strcmp( argv[i], pix_flags[p].flag ) == 0 ){

                    char buf[ 8 ];
                    sprintf( buf, "%d", _u16_arg( _vp_next_arg( argc, argv, &i, pix_flags[p].flag ), pix_flags[p].flag ) );
                    setenv( pix_flags[p].env, buf, 1 );
                    pix_matched = TRUE;
                    break;
                }
            }

            if( pix_matched ){

                continue;
            }
        }

        if( strcmp( argv[i], "--clean" ) == 0 ){

            _v_clean_state_files();

            // flush so the messages are not lost in the stdio buffer
            // while the OS is coming up
            fflush( stdout );
            continue;
        }

        if( argv[i][0] == '-' ){

            fprintf( stderr, "sim: unknown argument: %s\n", argv[i] );
            _v_print_usage( argv[0] );
            return 1;
        }

        // a positional argument is the run time, in seconds
        str = argv[i];
    }

    if( str == 0 ){

        str = getenv( "SIM_RUN_TIME" );
    }

    // the flag survives a simulated reboot (see the --sdl handler above)
    if( getenv( "SIM_SDL" ) != 0 ){

        sim_sdl_v_enable();
    }

    double secs = ( str != 0 ) ? atof( str ) : 0;

    if( secs > 0 ){

        // a simulated reboot re-execs without the command line, but
        // the environment survives: keep the run time for the restart
        char buf[32];

        sprintf( buf, "%g", secs );
        setenv( "SIM_RUN_TIME", buf, 1 );

        sim_run_time_ms = (uint32_t)( secs * 1000 );
    }

    if( sapphire_i8_init() == 0 ){

        if( app_v_init != 0 ){

            app_v_init();
        }

        if( libs_v_init != 0 ){

            libs_v_init();
        }

        // after the OS and app are initialized: preconfigure the pixel
        // array and the VM program before the scheduler starts.
        // the pixel array runs in sim_pixel mode (see hal_pixel.c): it
        // writes every transfer to stdout as a pixel/color tuple, so both
        // the VM4 programs and the pixel hardware abstraction are running.
        sim_v_setup();

        // user-specified pixel geometry and program file, which override
        // the seeded / persisted state when given
        sim_v_apply_pix_args();
        sim_v_run_prog_file();
    }

    sapphire_run();

    // should never get here:
    for(;;);
}
