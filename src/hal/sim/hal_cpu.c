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



#include "hal_cpu.h"

void cpu_v_init( void ){

}

uint8_t cpu_u8_get_reset_source( void ){

    return RESET_SOURCE_POWER_ON;
}

void cpu_v_remap_isrs( void ){

}

void cpu_v_sleep( void ){

}

char *itoa(long i, char* s, int dummy_radix) {
    sprintf(s, "%ld", i);
    return s;
}
