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



#ifndef _THREADINFO_H
#define _THREADINFO_H

#include <stdint.h>

#define THREAD_MAX_NAME_LEN 64

typedef struct __attribute__((packed)){
    char name[THREAD_MAX_NAME_LEN];
    uint16_t flags;
    uint32_t thread_addr;
    uint16_t data_size;
    uint32_t run_time;
    uint32_t runs;
    uint16_t line;
    uint64_t alarm;
    uint32_t max_time;
    uint8_t reserved[20];
} thread_info_t;


#endif