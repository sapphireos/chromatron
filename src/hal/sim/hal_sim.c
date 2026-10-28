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

/*

Simulator platform support.

The simulator has no device to serve, so the process ends after a run
time.  The run time, in seconds, is the first command-line argument
(eg. main.elf 10); if it is blank, the process runs until killed.

*/

void app_v_init( void ) __attribute__((weak));
void libs_v_init( void ) __attribute__((weak));

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

int main( int argc, char **argv ){

    char *str = ( argc > 1 ) ? argv[1] : getenv( "SIM_RUN_TIME" );

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
