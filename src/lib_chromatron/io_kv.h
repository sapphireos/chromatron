/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#ifndef _IO_KV_H
#define _IO_KV_H

#include "system.h"

#define IO_CFG_INPUT               0
#define IO_CFG_INPUT_PULLUP        1
#define IO_CFG_INPUT_PULLDOWN      2
#define IO_CFG_OUTPUT              3
#define IO_CFG_OUTPUT_OPEN_DRAIN   4
#define IO_CFG_ANALOG              5



void iokv_v_init( void );

#endif
