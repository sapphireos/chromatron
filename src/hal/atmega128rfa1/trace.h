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

#ifndef _TRACE_H	
#define _TRACE_H

#include <avr/pgmspace.h>

#define TRACE_BUF_SIZE 256

#ifdef BOOTLOADER
#define trace_printf(format, ...)
#define trace_printf_ram(format, ...)
#else
    #ifdef TRACE
    #define trace_printf(format, ...) _trace_printf(PSTR(format), ##__VA_ARGS__)
    #define trace_printf_ram(format, ...) _trace_printf_ram(format, ##__VA_ARGS__)
    #else
    #define trace_printf(format, ...)
    #define trace_printf_ram(format, ...)
    #endif
#endif

int _trace_printf(PGM_P format, ...);
int _trace_printf_ram(const char* format, ...);

#endif