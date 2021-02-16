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

#ifndef _TRACE_H	
#define _TRACE_H

int dummy_printf(const char* format, ...);

#include "target.h"

#ifdef BOOTLOADER
    #include "rom/ets_sys.h"
    #define trace_printf ets_printf
#else
    #ifdef ENABLE_TRACE
    #define trace_printf printf
    #else
    #define trace_printf dummy_printf
    #endif
#endif

#endif