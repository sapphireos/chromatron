/*
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
 */

#ifndef __DATALOGGER_H
#define __DATALOGGER_H

#include "catbus.h"

#define DATALOG_TICK_RATE       100

typedef struct __attribute__((packed)){
    catbus_hash_t32 hash;
    uint16_t rate;
    uint16_t padding;
} datalog_file_entry_t;

typedef struct __attribute__((packed)){
    catbus_data_t data;
} datalog_data_t;


void datalog_v_init( void );


#endif
