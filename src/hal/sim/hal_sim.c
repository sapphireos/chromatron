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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static void _v_print_usage( const char *prog ){

    printf( "usage: %s [run_time] [options]\n\n", prog );
    printf( "  run_time  (optional) seconds to run before the process exits;\n"
            "            blank or omitted runs until killed ($SIM_RUN_TIME\n"
            "            works too)\n" );
    printf( "  --clean   remove the simulator state files (sim_flash.bin,\n"
            "            sim_eeprom.bin, sim_bootdata.bin) so the next boot\n"
            "            starts from a clean slate, like a fresh device\n" );
    printf( "  --help    show this help and exit\n" );
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
    }

    sapphire_run();

    // should never get here:
    for(;;);
}
