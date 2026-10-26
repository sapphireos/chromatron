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

#include "boot_data.h"

volatile boot_data_t BOOTDATA boot_data;

#ifdef __SIM__

#include <stdio.h>
#include <string.h>

// the simulator has no no-init bootdata section; keep the 8 bytes in a
// file so they survive a reboot (re-exec), the same way the section
// does on hardware.
static const char *sim_boot_data_file = "sim_bootdata.bin";

void boot_data_v_save( void ){

    FILE *f = fopen( sim_boot_data_file, "wb" );

    if( f ){

        fwrite( &boot_data, 1, sizeof(boot_data), f );
        fclose( f );
    }
}

void boot_data_v_load( void ){

    FILE *f = fopen( sim_boot_data_file, "rb" );

    if( f && ( fread( &boot_data, 1, sizeof(boot_data), f ) != sizeof(boot_data) ) ){

        memset( &boot_data, 0, sizeof(boot_data) );
    }

    if( f ){

        fclose( f );
    }
}

#endif
