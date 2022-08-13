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

#ifndef _BOOL_H
#define _BOOL_H

#if defined(ARM) || defined(ESP32)
#include <stdbool.h>

#define TRUE true
#define FALSE false

#else

#include <stdint.h>

typedef uint8_t bool;

#ifdef ESP8266
#include "user_interface.h"
#define TRUE true
#define FALSE false
#else
#define TRUE 1
#define FALSE 0
#define true TRUE
#define false FALSE
#endif

#endif
#endif
