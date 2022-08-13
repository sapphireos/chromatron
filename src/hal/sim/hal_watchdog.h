/*
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
 */

#ifndef _HAL_WATCHDOG_H
#define _HAL_WATCHDOG_H

#include "system.h"

#define CHECK_WATCHDOG_RESET        FALSE

#define WATCHDOG_TIMEOUT_16MS       0
#define WATCHDOG_TIMEOUT_32MS       0
#define WATCHDOG_TIMEOUT_64MS       0
#define WATCHDOG_TIMEOUT_128MS      0
#define WATCHDOG_TIMEOUT_256MS      0
#define WATCHDOG_TIMEOUT_512MS      0
#define WATCHDOG_TIMEOUT_1024MS     0
#define WATCHDOG_TIMEOUT_2048MS     0
#define WATCHDOG_TIMEOUT_4096MS     0
#define WATCHDOG_TIMEOUT_8192MS     0

#define WATCHDOG_FLAGS_INTERRUPT    0
#define WATCHDOG_FLAGS_RESET        0

#endif
